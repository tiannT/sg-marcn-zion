/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PLANB_IO_H
#define RISCV_PLANB_IO_H
#include <asm/planb_abi.h>
struct acrn_vm;
#ifdef CONFIG_PLAN_B_SERVICES
void planb_io_init(struct acrn_vm *vm);
int planb_io_add_fdt(void *dtb);
bool planb_io_range(uint16_t vm_id, uint64_t address, uint64_t size);
/* Serialize EPM reservations with all virtqueue snapshots/completions. */
void planb_io_acquire(uint64_t *flags);
void planb_io_release(uint64_t flags);
bool planb_io_busy_range(uint64_t address, uint64_t size);
void planb_io_quiesce(void);
void planb_io_online(void);
#else
static inline void planb_io_init(__unused struct acrn_vm *vm) { }
static inline int planb_io_add_fdt(__unused void *dtb) { return 0; }
static inline bool planb_io_range(__unused uint16_t vm_id,
	__unused uint64_t address, __unused uint64_t size) { return false; }
#endif
#endif
