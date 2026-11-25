/* SPDX-License-Identifier: BSD-3-Clause */
#ifdef CONFIG_STATIC_DUAL
#include <types.h>
#include <asm/sg2044_static.h>
#include <sprintf.h>
#include <asm/guest/vm.h>
#include <asm/guest/vcpu.h>
#include <asm/per_cpu.h>
#include <asm/platform_boot.h>
#include <asm/static_vm.h>
#include <asm/early_printk.h>
#include <asm/pmp_live.h>
#include <asm/planb_io.h>
#include <asm/planb_enclave.h>
#include <asm/planb_runtime.h>
#ifdef CONFIG_PLAN_B_RUNTIME
#include "planb_sha3/sha3.h"
#endif
#include <libfdt.h>

struct static_vm_info static_vms[2] = {
	{ .base = STATIC_VM0_BASE, .size = STATIC_VM0_SIZE },
	{ .base = STATIC_VM1_BASE, .size = STATIC_VM1_SIZE },
};
uint64_t static_ddr_end;

struct bundle_vm {
	uint64_t kernel_offset, kernel_size, initrd_offset, initrd_size;
	uint64_t cmdline_offset, cmdline_size;
};
struct bundle_header {
	char magic[8];
	uint32_t version, count;
	uint64_t total_size;
	uint64_t fdt_offset, fdt_size;
	struct bundle_vm vm[2];
};

#ifdef CONFIG_PLAN_B_RUNTIME
struct runtime_bundle_header {
 struct bundle_header base;
 struct bundle_vm linux_image;
 uint32_t initial_image, reserved;
};
_Static_assert(sizeof(struct runtime_bundle_header) == 192, "runtime bundle ABI");
static const struct bundle_header *retained_bundle;
static const void *retained_fdt;
static uint32_t active_image;
static uint64_t guest_epoch = 1;
uint32_t static_vm_initial_image(void) { return active_image; }
uint64_t static_vm_guest_epoch(void) { return __atomic_load_n(&guest_epoch, __ATOMIC_ACQUIRE); }
#endif

static void reject(const char *why)
{
	early_printk("[STATIC] REJECTED: ");
	early_printk(why);
	early_printk("\n");
	for (;;) asm volatile("wfi");
}

static void checked(int result)
{
	if (result < 0) reject("cannot construct private Guest device tree");
}

static bool inside(uint64_t base, uint64_t length, uint64_t address, uint64_t size)
{
	return size && address >= base && address - base < length &&
		size <= length - (address - base);
}

bool static_vm_ram_range(uint16_t vm_id, uint64_t address, uint64_t size)
{
	return vm_id < 2 && planb_enclave_range(vm_id, address, size) &&
		(inside(static_vms[vm_id].base, static_vms[vm_id].size, address, size) ||
		 inside(STATIC_SHARED_BASE, STATIC_SHARED_SIZE, address, size));
}

bool static_vm_current_ram_range(uint64_t address, uint64_t size)
{
	struct acrn_vcpu *vcpu = get_running_vcpu(get_pcpu_id());
	return vcpu && static_vm_ram_range(vcpu->vm->vm_id, address, size);
}

bool static_vm_guest_ram_range(const struct acrn_vm *vm, uint64_t address, uint64_t size)
{
	return vm && static_vm_ram_range(vm->vm_id, address, size);
}

bool static_vm_emulated_mmio(uint16_t vm_id, uint64_t address, uint64_t size)
{
	if (planb_io_range(vm_id, address, size) ||
     planb_runtime_range(vm_id, address, size)) return true;
#ifdef CONFIG_STATIC_LIVE_TEST
	if (pmp_live_range(address, size)) return true;
#endif
	if (inside(CONFIG_UART_BASE, CONFIG_UART_SIZE, address, size) ||
	    inside(CONFIG_PLIC_BASE, CONFIG_PLIC_SIZE, address, size)) return true;
	if (inside(CONFIG_CLINT_BASE, 0x10000UL, address, size)) return false;
	/* VM1 receives no physical DMA devices. All denied RAM stays denied,
	 * including faults with a malicious page-table translation.
	 */
	return vm_id == 0 && size && address + size >= address &&
		(address + size <= 0x80000000UL || address >= static_ddr_end);
}

static void private_fdt(unsigned int id, const void *source, const char *cmdline)
{
	struct static_vm_info *info = &static_vms[id];
	void *dtb = (void *)info->dtb;
	fdt64_t memory[2], initrd;
	int node, cpus, chosen, soc, len, depth;
	char memory_name[32];
	bool have_cpu = false;
#ifdef CONFIG_PLAN_B_SERVICES
	const void *seed0, *seed1;
	uint8_t guest_seed[32];
	bool provided_entropy = false;
	int source_chosen, seed0_size, seed1_size;
#endif

	checked(fdt_open_into(source, dtb, MACRN_FDT_SIZE));
#ifdef CONFIG_PLAN_B_SERVICES
	source_chosen = fdt_path_offset(source, "/chosen");
	seed0 = fdt_getprop(source, source_chosen, "macrn,vm0-rng-seed", &seed0_size);
	seed1 = fdt_getprop(source, source_chosen, "macrn,vm1-rng-seed", &seed1_size);
	if (seed0 || seed1) {
		if (!seed0 || !seed1 || seed0_size != 32 || seed1_size != 32 ||
		    !fdt_memcmp(seed0, seed1, 32)) reject("invalid independent test entropy");
		#ifdef CONFIG_PLAN_B_RUNTIME
  if (id) {
   sha3_ctx_t hash;
   uint64_t epoch = guest_epoch;
   uint8_t separation[12];
   unsigned int i;
   /* Domain-separated KDF from the private loader seed; not a new entropy
    * source. Epoch never resets while VM0/this monitor remain running. */
   for (i = 0; i < 4; i++) separation[i] = (uint8_t)(active_image >> (i * 8));
   for (i = 0; i < 8; i++) separation[4+i] = (uint8_t)(epoch >> (i * 8));
   sha3_init(&hash, 32);
   sha3_update(&hash, "mACRN PlanB guest seed v1", 24);
   sha3_update(&hash, seed1, 32);
   sha3_update(&hash, separation, sizeof(separation));
   sha3_final(guest_seed, &hash);
   memset(&hash, 0, sizeof(hash));
  } else
#endif
  memcpy(guest_seed, id ? seed1 : seed0, sizeof(guest_seed));
		provided_entropy = true;
	}
#endif
	if (fdt_address_cells(dtb, 0) != 2 || fdt_size_cells(dtb, 0) != 2)
		reject("static profile requires 64-bit memory cells");
	/* Remove every inherited RAM bank, not just the first one. */
restart_memory:
	fdt_for_each_subnode(node, dtb, 0) {
		const char *type = fdt_getprop(dtb, node, "device_type", &len);
		if (type && len == 7 && !fdt_strcmp(type, "memory")) {
			checked(fdt_del_node(dtb, node));
			goto restart_memory;
		}
	}
	snprintf(memory_name, sizeof(memory_name), "memory@%lx", info->base);
	node = fdt_add_subnode(dtb, 0, memory_name);
	checked(node);
	checked(fdt_setprop_string(dtb, node, "device_type", "memory"));
	memory[0] = cpu_to_fdt64(info->base);
	memory[1] = cpu_to_fdt64(info->size);
	checked(fdt_setprop(dtb, node, "reg", memory, sizeof(memory)));
	cpus = fdt_path_offset(dtb, "/cpus");
	checked(cpus);
	node = fdt_subnode_offset(dtb, cpus, "cpu-map");
	if (node >= 0) checked(fdt_del_node(dtb, node));
	cpus = fdt_path_offset(dtb, "/cpus");
	fdt_for_each_subnode(node, dtb, cpus) {
		const char *type = fdt_getprop(dtb, node, "device_type", &len);
		const fdt32_t *reg;
		if (!type || len != 4 || fdt_strcmp(type, "cpu")) continue;
		reg = fdt_getprop(dtb, node, "reg", &len);
		if (reg && len == 4 && fdt32_to_cpu(*reg) == 0) {
			checked(fdt_setprop_string(dtb, node, "status", "okay"));
			have_cpu = true;
		} else checked(fdt_setprop_string(dtb, node, "status", "disabled"));
	}
	if (!have_cpu) reject("virtual CPU0 template is absent");
	fdt_set_boot_cpuid_phys(dtb, 0);
	/* Both VMs see logical Hart0. Physical Hart1 is hidden behind SBI. */
	depth = 0;
	for (node = fdt_next_node(dtb, -1, &depth); node >= 0;
	     node = fdt_next_node(dtb, node, &depth)) {
		if (!fdt_node_check_compatible(dtb, node, "thead,c900-aclint-mswi") ||
		    !fdt_node_check_compatible(dtb, node, "thead,c900-aclint-mtimer") ||
		    !fdt_node_check_compatible(dtb, node, "thead,c900-aclint-sswi") ||
		    !fdt_node_check_compatible(dtb, node, "riscv,clint0"))
			checked(fdt_setprop_string(dtb, node, "status", "disabled"));
		if (!fdt_node_check_compatible(dtb, node, "riscv,plic0") ||
		    !fdt_node_check_compatible(dtb, node, "riscv,plic1") ||
		    !fdt_node_check_compatible(dtb, node, "thead,c900-plic")) {
			const void *p = fdt_getprop(dtb, node, "interrupts-extended", &len);
			fdt32_t first[4];
			if (!p || len < (int)sizeof(first)) reject("invalid PLIC contexts");
			fdt_memcpy(first, p, sizeof(first));
			checked(fdt_setprop(dtb, node, "interrupts-extended", first, sizeof(first)));
			memory[0] = cpu_to_fdt64(CONFIG_PLIC_BASE);
			memory[1] = cpu_to_fdt64(CONFIG_PLIC_SIZE);
			checked(fdt_setprop(dtb, node, "reg", memory, sizeof(memory)));
			if (id == 1) {
				/* VM1 uses the monitor's standard, level-triggered PLIC.
				 * SG2044's physical T-Head PLIC has two interrupt cells;
				 * advertising that controller for our one-cell virtio IRQs
				 * makes Linux reject every queue IRQ before /init runs.
				 */
				checked(fdt_setprop_string(dtb, node, "compatible", "riscv,plic0"));
				checked(fdt_setprop_u32(dtb, node, "#interrupt-cells", 1));
				fdt_delprop(dtb, node, "clocks");
				fdt_delprop(dtb, node, "clock-names");
				fdt_delprop(dtb, node, "resets");
				fdt_delprop(dtb, node, "power-domains");
			}
		}
		if (!fdt_node_check_compatible(dtb, node, "snps,dw-apb-uart") ||
		    !fdt_node_check_compatible(dtb, node, "ns16550a")) {
			/* The virtual UART has no physical clock-controller ownership. */
			fdt_delprop(dtb, node, "clocks");
			fdt_delprop(dtb, node, "clock-names");
			fdt_delprop(dtb, node, "resets");
			if (id == 1) {
				const fdt32_t *interrupts = fdt_getprop(dtb, node, "interrupts", &len);
				fdt32_t source;
				/* VM1 owns an emulated 16550, not the physical DW-APB
				 * controller. Asterinas only binds ns16550a and accesses
				 * byte-spaced registers. VM0 retains the native binding.
				 */
				checked(fdt_setprop_string(dtb, node, "compatible", "ns16550a"));
				checked(fdt_setprop_u32(dtb, node, "reg-shift", 0));
				checked(fdt_setprop_u32(dtb, node, "reg-io-width", 1));
				/* Property insertions can relocate the interrupt value. */
				interrupts = fdt_getprop(dtb, node, "interrupts", &len);
				/* The retained virtual UART must use the same one-cell
				 * interrupt domain as the added virtio devices. */
				if (interrupts) {
					if (len != 4 && len != 8) reject("invalid virtual UART IRQ");
					source = interrupts[0];
					checked(fdt_setprop(dtb, node, "interrupts", &source, sizeof(source)));
				}
			}
		}
	}
	{
		/* CLK_OF_DECLARE and other early OF scans can visit a child even
		 * when its parent has status=disabled. Remove whole physical-device
		 * subtrees; changing just the parent status is not a boundary.
		 */
		soc = fdt_path_offset(dtb, "/soc");
		checked(soc);
restart_private_soc:
		fdt_for_each_subnode(node, dtb, soc) {
			bool keep = !fdt_node_check_compatible(dtb, node, "ns16550a") ||
				!fdt_node_check_compatible(dtb, node, "snps,dw-apb-uart") ||
				!fdt_node_check_compatible(dtb, node, "riscv,plic0") ||
				!fdt_node_check_compatible(dtb, node, "riscv,plic1") ||
				!fdt_node_check_compatible(dtb, node, "thead,c900-plic");
			if (id == 0)
				keep |= !fdt_node_check_compatible(dtb, node, "sophgo,sg2044-pcie-host") ||
					!fdt_node_check_compatible(dtb, node, "sophgo,sg2044-msi") ||
					!fdt_node_check_compatible(dtb, node, "sophgo,mtli") ||
					!fdt_node_check_compatible(dtb, node, "pci-host-ecam-generic") ||
					!fdt_node_check_compatible(dtb, node, "virtio,mmio");
			if (!keep) {
				checked(fdt_del_node(dtb, node));
				goto restart_private_soc;
			}
		}
	}
	if (id == 1) checked(planb_io_add_fdt(dtb));
	chosen = fdt_path_offset(dtb, "/chosen");
	if (chosen < 0) chosen = fdt_add_subnode(dtb, 0, "chosen");
	checked(chosen);
	checked(fdt_setprop_string(dtb, chosen, "bootargs", cmdline));
#ifdef CONFIG_PLAN_B_SERVICES
	/* The source template stays in monitor/loader-reserved RAM. Neither
 * guest may receive the other seed or a seed shared by both guests.
 * This facility is for freshly generated, single-boot acceptance images;
 * it does not create an entropy source on the SG2044 hardware.
 */
	fdt_delprop(dtb, chosen, "macrn,vm0-rng-seed");
	fdt_delprop(dtb, chosen, "macrn,vm1-rng-seed");
	if (provided_entropy) {
		checked(fdt_setprop(dtb, chosen, "rng-seed", guest_seed, sizeof(guest_seed)));
		memset(guest_seed, 0, sizeof(guest_seed));
		early_printk(id ? "[PLANB] VM1 INDEPENDENT_SINGLE_BOOT_TEST_SEED\n" :
			"[PLANB] VM0 INDEPENDENT_SINGLE_BOOT_TEST_SEED\n");
	}
	checked(fdt_setprop_u32(dtb, chosen, "macrn,planb-services-abi", PLANB_ABI));
#ifdef CONFIG_PLAN_B_RUNTIME
 if (!id) checked(fdt_setprop_u32(dtb, chosen, "macrn,planb-runtime-abi", PLANB_RUNTIME_ABI));
 if (id) {
  fdt64_t epoch = cpu_to_fdt64(guest_epoch);
  checked(fdt_setprop_u32(dtb, chosen, "macrn,guest-image", active_image));
  checked(fdt_setprop(dtb, chosen, "macrn,guest-epoch", &epoch, sizeof(epoch)));
 }
#endif
#endif
#ifdef CONFIG_STATIC_LIVE_TEST
	{
		int reserved, page;
		fdt32_t abi = cpu_to_fdt32(PMP_LIVE_ABI);
		checked(fdt_setprop(dtb, chosen, "macrn,pmp-live-test-abi", &abi, sizeof(abi)));
		reserved = fdt_path_offset(dtb, "/reserved-memory");
		if (reserved < 0) reserved = fdt_add_subnode(dtb, 0, "reserved-memory");
		checked(reserved);
		checked(fdt_setprop_u32(dtb, reserved, "#address-cells", 2));
		checked(fdt_setprop_u32(dtb, reserved, "#size-cells", 2));
		checked(fdt_setprop(dtb, reserved, "ranges", NULL, 0));
		snprintf(memory_name, sizeof(memory_name), "pmp-test@%lx", info->base + PMP_LIVE_PAGE_OFFSET);
		page = fdt_add_subnode(dtb, reserved, memory_name);
		checked(page);
		memory[0] = cpu_to_fdt64(info->base + PMP_LIVE_PAGE_OFFSET);
		memory[1] = cpu_to_fdt64(PMP_LIVE_PAGE_SIZE);
		checked(fdt_setprop(dtb, page, "reg", memory, sizeof(memory)));
		checked(fdt_setprop(dtb, page, "no-map", NULL, 0));
		/* Node insertions can move chosen; reacquire its offset. */
		chosen = fdt_path_offset(dtb, "/chosen");
		checked(chosen);
	}
#endif
	if (info->initrd_size) {
		initrd = cpu_to_fdt64(info->base + STATIC_INITRD_OFFSET);
		checked(fdt_setprop(dtb, chosen, "linux,initrd-start", &initrd, sizeof(initrd)));
		initrd = cpu_to_fdt64(info->base + STATIC_INITRD_OFFSET + info->initrd_size);
		checked(fdt_setprop(dtb, chosen, "linux,initrd-end", &initrd, sizeof(initrd)));
	} else {
		fdt_delprop(dtb, chosen, "linux,initrd-start");
		fdt_delprop(dtb, chosen, "linux,initrd-end");
	}
	checked(fdt_add_mem_rsv(dtb, info->dtb, MACRN_FDT_SIZE));
	checked(fdt_pack(dtb));
}

#ifndef CONFIG_STATIC_SMOKE
static const void *bundle_item(const struct bundle_header *h, uint64_t offset, uint64_t size)
{
	if (offset <
#ifdef CONFIG_PLAN_B_RUNTIME
     sizeof(struct runtime_bundle_header)
#else
     sizeof(*h)
#endif
     || !inside(0, h->total_size, offset, size))
		reject("invalid payload offset/length");
	/* ZSBL owns this boot-time FDT slot even in an expanded bundle. */
	if (h->total_size > STATIC_BOOT_FDT_BASE - STATIC_BUNDLE_BASE &&
	    offset < STATIC_BOOT_FDT_BASE + STATIC_BOOT_FDT_SIZE - STATIC_BUNDLE_BASE &&
	    offset + size > STATIC_BOOT_FDT_BASE - STATIC_BUNDLE_BASE)
		reject("payload overlaps boot device tree");
	return (const char *)h + offset;
}
#ifdef CONFIG_PLAN_B_RUNTIME
static const struct bundle_vm *image_descriptor(const struct bundle_header *h, unsigned int n)
{
 /* Linux and enclave mode share the exact reference kernel/initramfs;
  * enclave mode has a separate runtime identity and no block device. */
 if (n == PLANB_IMAGE_ENCLAVE) n = PLANB_IMAGE_LINUX;
 return &((const struct bundle_vm *)((const char *)h + 40))[n];
}
static void validate_image(const struct bundle_header *h, unsigned int n)
{
 const struct bundle_vm *d = image_descriptor(h, n);
 const struct static_vm_info *info = &static_vms[n ? 1 : 0];
 const uint8_t *kernel = bundle_item(h, d->kernel_offset, d->kernel_size);
 const char *cmdline = bundle_item(h, d->cmdline_offset, d->cmdline_size);
 uint64_t text_offset, memory_size;
 if (d->kernel_size < 64 || d->kernel_size > STATIC_INITRD_OFFSET - STATIC_KERNEL_OFFSET ||
     memcmp(kernel + 56, "RSC\005", 4)) reject("invalid unified Guest kernel");
 memcpy(&text_offset, kernel + 8, 8);
 memcpy(&memory_size, kernel + 16, 8);
 if (text_offset != STATIC_KERNEL_OFFSET || memory_size > STATIC_INITRD_OFFSET - STATIC_KERNEL_OFFSET)
  reject("unified Guest kernel memory exceeds fixed layout");
 if (!d->cmdline_size || d->cmdline_size > 4096 || cmdline[d->cmdline_size-1])
  reject("invalid unified Guest command line");
 if (d->initrd_size) {
  bundle_item(h, d->initrd_offset, d->initrd_size);
  if (d->initrd_size > info->size - STATIC_INITRD_OFFSET) reject("unified initrd exceeds RAM");
 }
}
bool static_vm_clear_guest(void)
{
 volatile uint64_t *p = (void *)static_vms[1].base;
 uint64_t words = static_vms[1].size / 8, i, residue = 0;
 for (i = 0; i < words; i++) p[i] = 0;
 asm volatile("fence rw, rw" ::: "memory");
 for (i = 0; i < words; i++) residue |= p[i];
 return residue == 0;
}
void static_vm_reload_guest(uint32_t image)
{
 const struct bundle_vm *d;
 struct static_vm_info *info = &static_vms[1];
 if (image < PLANB_IMAGE_ASTERINAS || image > PLANB_IMAGE_ENCLAVE || guest_epoch == UINT64_MAX)
  reject("invalid unified Guest reload");
 d = image_descriptor(retained_bundle, image);
 active_image = image;
 __atomic_add_fetch(&guest_epoch, 1, __ATOMIC_RELEASE);
 info->entry = info->base + STATIC_KERNEL_OFFSET;
 info->dtb = info->base + STATIC_DTB_OFFSET;
 info->kernel_size = d->kernel_size;
 info->initrd_size = d->initrd_size;
 fdt_memcpy((void *)info->entry, bundle_item(retained_bundle, d->kernel_offset, d->kernel_size), d->kernel_size);
 if (d->initrd_size) fdt_memcpy((void *)(info->base + STATIC_INITRD_OFFSET),
  bundle_item(retained_bundle, d->initrd_offset, d->initrd_size), d->initrd_size);
#ifdef CONFIG_STATIC_LIVE_TEST
 {
  volatile uint64_t *page = (void *)(info->base + PMP_LIVE_PAGE_OFFSET);
  page[0] = PMP_LIVE_CANARY + 1; page[1] = PMP_LIVE_CODE;
  page[32] = 0x0010007300033283UL;
  page[33] = 0x0010007300733023UL;
  page[34] = 0x00100073000300e7UL;
 }
#endif
 private_fdt(1, retained_fdt, bundle_item(retained_bundle, d->cmdline_offset, d->cmdline_size));
 asm volatile("fence rw, rw; sfence.vma; fence.i" ::: "memory");
}
#endif
#else
extern const unsigned char static_guest0_begin[], static_guest0_end[];
extern const unsigned char static_guest1_begin[], static_guest1_end[];
#endif

void static_vm_prepare(const void *source_fdt)
{
	unsigned int id;
#ifndef CONFIG_STATIC_SMOKE
	const struct bundle_header *h = (void *)STATIC_BUNDLE_BASE;
	#ifdef CONFIG_PLAN_B_RUNTIME
 const struct runtime_bundle_header *rh = (const void *)h;
 if (fdt_memcmp(h->magic, "MACRNBRT", 8) || h->version != 2 || h->count != 3 ||
     h->total_size < sizeof(*rh) || h->total_size > STATIC_BUNDLE_LIMIT - STATIC_BUNDLE_BASE ||
     rh->initial_image < 1 || rh->initial_image > 2 || rh->reserved)
  reject("invalid unified runtime bundle; use matching SG2044.fd v2");
 for (id = 0; id < 3; id++) validate_image(h, id);
 retained_bundle = h;
 active_image = rh->initial_image;
#else
 if (fdt_memcmp(h->magic, "MACRN2VM", 8) || h->version != 1 || h->count != 2 ||
     h->total_size < sizeof(*h) || h->total_size > STATIC_BUNDLE_LIMIT - STATIC_BUNDLE_BASE)
  reject("invalid two-Linux payload bundle; use matching SG2044.fd v1");
#endif
	if (h->fdt_size) {
		const void *template = bundle_item(h, h->fdt_offset, h->fdt_size);
		if (h->fdt_size > MACRN_FDT_SIZE - 4096 ||
		    fdt_check_header(template) || fdt_totalsize(template) != h->fdt_size)
			reject("invalid Linux device-tree template");
#ifdef CONFIG_PLATFORM_SG2044
		if (fdt_node_check_compatible(template, 0, "sophgo,sg2044"))
			reject("device-tree template is not for SG2044");
#else
		if (fdt_node_check_compatible(template, 0, "riscv-virtio"))
			reject("device-tree template is not for QEMU virt");
#endif
		source_fdt = template;
	}
#endif
	#ifdef CONFIG_PLAN_B_RUNTIME
 retained_fdt = source_fdt;
#endif
 if (!sg2044_static_devices_prepare())
		reject("native PCIe/PLIC initialization readback failed");
	if (!platform_guest_ram_range(STATIC_SHARED_BASE, STATIC_SHARED_SIZE))
		reject("shared buffer is outside DDR");
	memset((void *)STATIC_SHARED_BASE, 0, STATIC_SHARED_SIZE);
	for (id = 0; id < 2; ++id) {
		struct static_vm_info *info = &static_vms[id];
		const void *kernel;
		const char *cmdline = "";
		uint64_t kernel_size;
#ifndef CONFIG_STATIC_SMOKE
  const struct bundle_vm *d =
#ifdef CONFIG_PLAN_B_RUNTIME
   image_descriptor(h, id ? active_image : 0);
#else
   &h->vm[id];
#endif
#endif
		if (!platform_guest_ram_range(info->base, info->size))
			reject("private RAM partition is outside DDR");
		/* Static ownership begins with clean pages, including free memory
		 * that Linux will later give to userspace or subsequent modules.
		 */
		{
			volatile uint64_t *p = (void *)info->base;
			uint64_t words = info->size / sizeof(*p);
			early_printk(id ? "[STATIC] VM1 private RAM clearing\n" :
				"[STATIC] VM0 private RAM clearing\n");
			while (words--) *p++ = 0;
			early_printk(id ? "[STATIC] VM1 private RAM clean\n" :
				"[STATIC] VM0 private RAM clean\n");
		}
		info->entry = info->base + STATIC_KERNEL_OFFSET;
		info->dtb = info->base + STATIC_DTB_OFFSET;
#ifdef CONFIG_STATIC_SMOKE
		kernel = id ? static_guest1_begin : static_guest0_begin;
		kernel_size = id ? static_guest1_end - static_guest1_begin :
			static_guest0_end - static_guest0_begin;
#else
  kernel_size = d->kernel_size;
  kernel = bundle_item(h, d->kernel_offset, kernel_size);
  info->initrd_size = d->initrd_size;
  cmdline = bundle_item(h, d->cmdline_offset, d->cmdline_size);
  if (!d->cmdline_size || d->cmdline_size > 4096 ||
      cmdline[d->cmdline_size - 1] != 0)
			reject("invalid Guest command line");
		if (info->initrd_size) {
			const void *initrd = bundle_item(h, d->initrd_offset, info->initrd_size);
			if (info->initrd_size > info->size - STATIC_INITRD_OFFSET)
				reject("initramfs exceeds private RAM");
			fdt_memcpy((void *)(info->base + STATIC_INITRD_OFFSET), initrd, info->initrd_size);
		}
#endif
		if (!kernel_size || kernel_size > STATIC_INITRD_OFFSET - STATIC_KERNEL_OFFSET)
			reject("kernel overlaps initramfs");
		info->kernel_size = kernel_size;
		fdt_memcpy((void *)info->entry, kernel, kernel_size);
#ifdef CONFIG_STATIC_LIVE_TEST
		{
			volatile uint64_t *page = (void *)(info->base + PMP_LIVE_PAGE_OFFSET);
			page[0] = PMP_LIVE_CANARY + id;
			page[1] = PMP_LIVE_CODE;
			page[32] = 0x0010007300033283UL; /* ld t0,0(t1); ebreak */
			page[33] = 0x0010007300733023UL; /* sd t2,0(t1); ebreak */
			page[34] = 0x00100073000300e7UL; /* jalr ra,0(t1); ebreak */
		}
#endif
		private_fdt(id, source_fdt, cmdline);
	}
	asm volatile("fence rw, rw; fence.i" ::: "memory");
	early_printk("[STATIC] TWO_PRIVATE_GUESTS_READY; UEFI skipped\n");
#ifdef CONFIG_STATIC_LIVE_TEST
	early_printk("[PMP-LIVE] NORMAL_DUAL_LINUX_TEST_ABI=2 PRIVATE_TEST_PAGES_RESERVED\n");
#endif
}

void static_vm_set_boot_info(struct acrn_vm *vm)
{
	struct static_vm_info *info = &static_vms[vm->vm_id];
	vm->sw.kernel_info.kernel_addr = info->entry;
	vm->sw.kernel_info.kernel_len = info->kernel_size;
	vm->sw.kernel_info.text_offset = 0;
	vm->sw.dtb_info.dtb_addr = info->dtb;
	vm->sw.dtb_info.dtb_len = MACRN_FDT_SIZE;
}
#endif
