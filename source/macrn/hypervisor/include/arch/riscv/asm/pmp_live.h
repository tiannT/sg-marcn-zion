/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PMP_LIVE_H
#define RISCV_PMP_LIVE_H
#define PMP_LIVE_ABI 2
#define PMP_LIVE_MAGIC 0x504d504c49564532UL
#define PMP_LIVE_AUDIT 0x92010000UL
#define PMP_LIVE_PAGE_OFFSET 0x80000UL
#define PMP_LIVE_PAGE_SIZE 4096UL
#define PMP_LIVE_CANARY 0x1122334455667788UL
#define PMP_LIVE_CODE 0x0000806712300513UL
#define PMP_LIVE_SHARED 0x92000000UL
#define PMP_LIVE_COMMAND 0x100UL
#define PMP_LIVE_ABI_PROPERTY "/proc/device-tree/chosen/macrn,pmp-live-test-abi"
#ifdef __ACRN__
#include <types.h>
bool pmp_live_range(uint64_t address, uint64_t size);
struct acrn_vcpu;
int32_t pmp_live_begin(struct acrn_vcpu *vcpu, uint64_t command);
bool pmp_live_finish(struct acrn_vcpu *vcpu);
void pmp_live_early_restore(void);
bool pmp_live_idle(uint16_t vm);
void pmp_live_reset_idle(uint16_t vm);
uint64_t pmp_live_read(uint16_t vm, uint64_t offset);
void pmp_live_record(uint16_t vm, uint64_t cause, uint64_t pa, uint64_t va, uint64_t pc);
#endif
#endif
