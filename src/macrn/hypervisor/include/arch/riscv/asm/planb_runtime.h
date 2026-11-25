/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PLANB_RUNTIME_H
#define RISCV_PLANB_RUNTIME_H
#define PLANB_RUNTIME_ABI 1U
#define PLANB_RUNTIME_MAGIC 0x42525431U
#define PLANB_RUNTIME_MMIO 0x93003000ULL
#define PLANB_IMAGE_ASTERINAS 1U
#define PLANB_IMAGE_LINUX 2U
#define PLANB_IMAGE_ENCLAVE 3U
#define PLANB_RT_STOPPED 0U
#define PLANB_RT_RUNNING 1U
#define PLANB_RT_STOPPING 2U
#define PLANB_RT_LOADING 3U
#define PLANB_RT_FAILED 4U
#define PLANB_RT_STOP 1U
#define PLANB_RT_START_ASTERINAS 2U
#define PLANB_RT_START_LINUX 3U
#define PLANB_RT_START_ENCLAVE 4U
#define PLANB_NATIVE_COMPAT_MAGIC 0x5a4e4331U
#ifdef __ACRN__
struct acrn_vm;
struct acrn_vcpu;
#ifdef CONFIG_PLAN_B_RUNTIME
void planb_runtime_init(struct acrn_vm *vm);
bool planb_runtime_entry(struct acrn_vcpu *vcpu);
bool planb_runtime_range(uint16_t vm, uint64_t address, uint64_t size);
bool planb_runtime_block_enabled(void);
void planb_runtime_tamper_detected(void);
void planb_guest_console_reset(uint16_t vm);
#else
static inline void planb_runtime_init(__unused struct acrn_vm *v) { }
static inline bool planb_runtime_entry(__unused struct acrn_vcpu *v) { return true; }
static inline bool planb_runtime_range(__unused uint16_t v,
	__unused uint64_t a, __unused uint64_t n) { return false; }
static inline bool planb_runtime_block_enabled(void) { return true; }
static inline void planb_runtime_tamper_detected(void) { }
#endif
#endif
#endif
