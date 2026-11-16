/* SPDX-License-Identifier: BSD-3-Clause */
#include <types.h>
#include <asm/boot.h>
#include <asm/mem.h>
#include <asm/early_printk.h>
#include <asm/platform_boot.h>
#include <asm/static_vm.h>
#include <libfdt.h>

#ifdef CONFIG_BRINGUP_GUEST
extern const unsigned char builtin_guest_begin[], builtin_guest_end[];
#endif

static struct fw_dynamic_info saved_next;
static struct { uint64_t base, size; } guest_banks[16];
static unsigned int guest_bank_count;

static uint64_t read_cells(const fdt32_t *p, int n)
{
	uint64_t value = 0;
	while (n--) value = (value << 32) | fdt32_to_cpu(*p++);
	return value;
}

bool platform_guest_ram_range(uint64_t address, uint64_t size)
{
	unsigned int i;
	uint64_t end = address + size;
	if (!size || end < address) return false;
	if ((address < MACRN_RAM_START + MACRN_RAM_SIZE && end > MACRN_RAM_START) ||
	    (address < MACRN_LOAD_START + MACRN_LOAD_SIZE && end > MACRN_LOAD_START))
		return false;
	for (i = 0; i < guest_bank_count; ++i) {
		if (address >= guest_banks[i].base &&
		    end >= address && end - guest_banks[i].base <= guest_banks[i].size)
			return true;
	}
	return false;
}

static void boot_failure(const char *message)
{
	early_printk("m-ACRN boot rejected: ");
	early_printk((char *)message);
	early_printk("\n");
	for (;;) asm volatile("wfi");
}

static void boot_value(const char *label, uint64_t value)
{
	char text[19];
	unsigned int i;

	text[0] = '0';
	text[1] = 'x';
	for (i = 0; i < 16; ++i)
		text[i + 2] = "0123456789abcdef"[(value >> (60 - i * 4)) & 15];
	text[18] = 0;
	early_printk(label);
	early_printk(text);
	early_printk("\n");
}

void platform_boot_prepare(void)
{
	void *dtb = (void *)MACRN_FDT_START;
	int rc, cpus, node, address_cells, size_cells;
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	struct fw_dynamic_info *upstream_next = fw_dinfo;
#else
	uint64_t next;
#endif

	boot_value("[BOOT] INCOMING_FDT=", fw_dtb);
	boot_value("[BOOT] INCOMING_DYNAMIC=", (uint64_t)fw_dinfo);
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	/* This diagnostic Guest replaces the unused upstream next stage.
	 * Do not require an EDK II descriptor or dereference an unknown a2.
	 */
	saved_next.magic = 0x4942534fUL;
	saved_next.version = 2;
	saved_next.addr = MACRN_BRINGUP_GUEST_START;
	saved_next.mode = 1;
	saved_next.options = 0;
	saved_next.hart_id = CONFIG_BSP_CPU_ID;
	fw_dinfo = &saved_next;
	early_printk("[BRINGUP] EMBEDDED_HANDOFF_SELF_CONTAINED\n");
#else
	if (!fw_dinfo)
		boot_failure("missing fw_dynamic_info");
	if ((uint64_t)fw_dinfo & 7)
		boot_failure("unaligned fw_dynamic_info");
	boot_value("[BOOT] DYNAMIC_MAGIC=", fw_dinfo->magic);
	boot_value("[BOOT] DYNAMIC_VERSION=", fw_dinfo->version);
	boot_value("[BOOT] DYNAMIC_MODE=", fw_dinfo->mode);
	boot_value("[BOOT] DYNAMIC_ENTRY=", fw_dinfo->addr);
	if (fw_dinfo->magic != 0x4942534fUL)
		boot_failure("fw_dynamic_info magic mismatch");
	if (fw_dinfo->version != 1 && fw_dinfo->version != 2)
		boot_failure("unsupported fw_dynamic_info version");
	if (fw_dinfo->mode != 1)
		boot_failure("fw_dynamic_info next stage is not S-mode");
	/* Version 1 contains only the first 40 bytes. */
	saved_next.magic = fw_dinfo->magic;
	saved_next.version = fw_dinfo->version;
	saved_next.addr = fw_dinfo->addr;
	saved_next.mode = fw_dinfo->mode;
	saved_next.options = fw_dinfo->options;
	saved_next.hart_id = fw_dinfo->version == 2 ? fw_dinfo->hart_id : (uint64_t)-1;
	fw_dinfo = &saved_next;
	next = saved_next.addr;
	if (!next || (next >= MACRN_RAM_START && next < MACRN_RAM_START + MACRN_RAM_SIZE) ||
	    (next >= MACRN_LOAD_START && next < MACRN_LOAD_START + MACRN_LOAD_SIZE) ||
	    (next >= MACRN_FDT_START && next < MACRN_FDT_START + MACRN_FDT_SIZE))
		boot_failure("next stage overlaps monitor-owned memory");
#endif
	if (!fw_dtb || fdt_check_header((void *)fw_dtb) ||
	    fdt_totalsize((void *)fw_dtb) > MACRN_FDT_SIZE - 4096)
		boot_failure("missing, invalid or oversized device tree");
	early_printk("[BOOT] FDT_HEADER_OK\n");
	rc = fdt_open_into((void *)fw_dtb, dtb, MACRN_FDT_SIZE);
	if (!rc) rc = fdt_add_mem_rsv(dtb, MACRN_LOAD_START, MACRN_LOAD_SIZE);
	if (!rc) rc = fdt_add_mem_rsv(dtb, MACRN_RAM_START, MACRN_RAM_SIZE);
	if (!rc) rc = fdt_add_mem_rsv(dtb, MACRN_FDT_START, MACRN_FDT_SIZE);
	if (rc) boot_failure("cannot reserve monitor memory in the device tree");
	address_cells = fdt_address_cells(dtb, 0);
	size_cells = fdt_size_cells(dtb, 0);
	if (address_cells < 1 || address_cells > 2 || size_cells < 1 || size_cells > 2)
		boot_failure("unsupported memory address/size cells");
	fdt_for_each_subnode(node, dtb, 0) {
		const char *type;
		const fdt32_t *reg;
		int len, cells = address_cells + size_cells;
		type = fdt_getprop(dtb, node, "device_type", &len);
		if (!type || len != 7 || fdt_strcmp(type, "memory")) continue;
		reg = fdt_getprop(dtb, node, "reg", &len);
		if (!reg || len <= 0 || len % (4 * cells))
			boot_failure("invalid memory bank description");
		while (len) {
			uint64_t start = read_cells(reg, address_cells);
			uint64_t size = read_cells(reg + address_cells, size_cells);
			if (!size || start + size < start || guest_bank_count == 16)
				boot_failure("invalid or excessive memory banks");
			guest_banks[guest_bank_count].base = start;
			guest_banks[guest_bank_count++].size = size;
#ifdef CONFIG_STATIC_DUAL
			if (start + size > static_ddr_end) static_ddr_end = start + size;
#endif
			reg += cells; len -= 4 * cells;
		}
	}
	if (!guest_bank_count) boot_failure("no physical memory banks supplied");
#ifdef CONFIG_STATIC_DUAL
	(void)upstream_next;
	static_vm_prepare(dtb);
	return;
#endif
#ifdef CONFIG_BRINGUP_GUEST
	/* Read diagnostics only from an aligned descriptor in validated DDR.
	 * Descriptors in SRAM or other memory are skipped, not assumed invalid.
	 */
	if (upstream_next && !((uint64_t)upstream_next & 7) &&
	    platform_guest_ram_range((uint64_t)upstream_next, 40)) {
		boot_value("[BOOT] UPSTREAM_DYNAMIC_MAGIC=", upstream_next->magic);
		boot_value("[BOOT] UPSTREAM_DYNAMIC_VERSION=", upstream_next->version);
		boot_value("[BOOT] UPSTREAM_DYNAMIC_MODE=", upstream_next->mode);
		boot_value("[BOOT] UPSTREAM_DYNAMIC_ENTRY=", upstream_next->addr);
	} else {
		early_printk("[BOOT] UPSTREAM_DYNAMIC_FIELDS_NOT_READ\n");
	}
#endif
	cpus = fdt_path_offset(dtb, "/cpus");
	if (cpus < 0) boot_failure("device tree has no CPU description");
	fdt_for_each_subnode(node, dtb, cpus) {
		const fdt32_t *reg;
		const char *type;
		int len;
		type = fdt_getprop(dtb, node, "device_type", &len);
		if (!type || len != 4 || fdt_strcmp(type, "cpu")) continue;
		reg = fdt_getprop(dtb, node, "reg", &len);
		if (reg && len >= 4 && fdt32_to_cpu(*reg) >= CONFIG_MAX_VCPU) {
			rc = fdt_setprop_string(dtb, node, "status", "disabled");
			if (rc) boot_failure("cannot restrict guest CPU assignment");
		}
	}
	fw_dtb = (uint64_t)dtb;
#ifdef CONFIG_BRINGUP_GUEST
	{
		uint64_t i, size = builtin_guest_end - builtin_guest_begin;
		volatile unsigned char *target = (void *)MACRN_BRINGUP_GUEST_START;

		if (!size || size > MACRN_BRINGUP_GUEST_SIZE ||
		    !platform_guest_ram_range(MACRN_BRINGUP_GUEST_START,
					      MACRN_BRINGUP_GUEST_SIZE))
			boot_failure("embedded Guest does not fit in physical RAM");
		/* Leave the original EDK II image at 0x80200000 untouched and
		 * unexecuted. The diagnostic Guest uses an independent region.
		 */
		for (i = 0; i < size; ++i) target[i] = builtin_guest_begin[i];
		asm volatile("fence rw, rw; fence.i" ::: "memory");
		saved_next.addr = MACRN_BRINGUP_GUEST_START;
		early_printk("[BRINGUP] EMBEDDED_GUEST_READY_82000000; UEFI skipped\n");
	}
#endif
#ifdef CONFIG_PLATFORM_SG2044
	early_printk("m-ACRN SG2044 M-mode: relocated; FDT memory reserved\n");
#else
	early_printk("m-ACRN QEMU M-mode: relocated; FDT memory reserved\n");
#endif
}
