/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PLANB_ENCLAVE_H
#define RISCV_PLANB_ENCLAVE_H
struct acrn_vcpu;
#ifdef CONFIG_PLAN_B_ENCLAVE
bool planb_enclave_active(const struct acrn_vcpu *vcpu);
bool planb_enclave_range(uint16_t vm, uint64_t address, uint64_t length);
bool planb_enclave_parent_range(uint64_t address, uint64_t length);
bool planb_enclave_load_pmp(struct acrn_vcpu *vcpu);
void planb_enclave_prepare_entry(struct acrn_vcpu *vcpu);
bool planb_enclave_early_restore(void);
bool planb_enclave_finish_exit(struct acrn_vcpu *vcpu);
bool planb_enclave_sbi(struct acrn_vcpu *vcpu);
bool planb_enclave_idle(void);
void planb_enclave_reset_idle(void);
#else
static inline bool planb_enclave_active(__unused const struct acrn_vcpu *v) { return false; }
static inline bool planb_enclave_range(__unused uint16_t v,
	__unused uint64_t a, __unused uint64_t n) { return true; }
static inline bool planb_enclave_parent_range(__unused uint64_t a,
	__unused uint64_t n) { return true; }
static inline bool planb_enclave_load_pmp(__unused struct acrn_vcpu *v) { return false; }
static inline void planb_enclave_prepare_entry(__unused struct acrn_vcpu *v) { }
static inline bool planb_enclave_finish_exit(__unused struct acrn_vcpu *v) { return false; }
static inline bool planb_enclave_sbi(__unused struct acrn_vcpu *v) { return false; }
#endif
#endif
