/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_STATIC_VM_H
#define RISCV_STATIC_VM_H
#include <asm/mem.h>

#ifdef CONFIG_PLATFORM_SG2044
#define STATIC_VM0_BASE 0xa0000000
#define STATIC_VM0_SIZE 0x200000000
#define STATIC_VM1_BASE 0x400000000
#define STATIC_VM1_SIZE 0x100000000
#else
#define STATIC_VM0_BASE 0xa0000000
#define STATIC_VM0_SIZE 0x80000000
#define STATIC_VM1_BASE 0x140000000
#define STATIC_VM1_SIZE 0x40000000
#endif
#define STATIC_SHARED_BASE 0x92000000
#define STATIC_SHARED_SIZE 0x10000
#define STATIC_KERNEL_OFFSET 0x200000
#define STATIC_DTB_OFFSET 0x10000
#define STATIC_INITRD_OFFSET 0x10000000
#define STATIC_BUNDLE_BASE 0x80200000
#define STATIC_BUNDLE_LIMIT 0x90000000
#define STATIC_BOOT_FDT_BASE 0x88000000
#define STATIC_BOOT_FDT_SIZE 0x20000

#ifndef __ASSEMBLY__
struct acrn_vm;
struct static_vm_info {
	uint64_t base, size, entry, kernel_size, dtb, initrd_size;
};
extern struct static_vm_info static_vms[2];
extern uint64_t static_ddr_end;
void static_vm_prepare(const void *source_fdt);
void static_vm_set_boot_info(struct acrn_vm *vm);
#ifdef CONFIG_PLAN_B_RUNTIME
bool static_vm_clear_guest(void);
void static_vm_reload_guest(uint32_t image);
uint32_t static_vm_initial_image(void);
uint64_t static_vm_guest_epoch(void);
#endif
bool static_vm_ram_range(uint16_t vm_id, uint64_t address, uint64_t size);
bool static_vm_guest_ram_range(const struct acrn_vm *vm, uint64_t address, uint64_t size);
bool static_vm_emulated_mmio(uint16_t vm_id, uint64_t address, uint64_t size);
bool static_vm_current_ram_range(uint64_t address, uint64_t size);
#endif
#endif
