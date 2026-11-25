/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <asm/guest/vmcs.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/vmx.h>
#include <asm/current.h>
#include <asm/pgtable.h>
#include <asm/per_cpu.h>
#include <asm/init.h>
//#include <cpu_caps.h>
//#include <cpufeatures.h>
#include <asm/guest/vcsr.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/s2vm.h>
#include <logmsg.h>
#include <asm/static_vm.h>
#include <asm/guest/vplic.h>
#include <asm/planb_enclave.h>

#ifndef CONFIG_MACRN
static void init_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	vcpu_set_gpreg(vcpu, OFFSET_REG_A0, vcpu->vcpu_id);
	cpu_csr_write(vsstatus, ctx->run_ctx.sstatus);
	cpu_csr_write(vsepc, ctx->run_ctx.sepc);
	cpu_csr_write(vsip, ctx->run_ctx.sip);
	cpu_csr_write(hvip, (ctx->run_ctx.sip & 0x202) << 1);
	cpu_csr_write(vsie, ctx->run_ctx.sie);
	cpu_csr_write(hie, (ctx->run_ctx.sie & 0x222) << 1);
	cpu_csr_write(vstvec, ctx->run_ctx.stvec);
	cpu_csr_write(vsscratch, ctx->run_ctx.sscratch);
	cpu_csr_write(vstval, ctx->run_ctx.stval);
	cpu_csr_write(vscause, ctx->run_ctx.scause);
	cpu_csr_write(vsatp, ctx->run_ctx.satp);
}

static void load_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	cpu_csr_write(vsstatus, ctx->run_ctx.sstatus);
	cpu_csr_write(vsepc, ctx->run_ctx.sepc);
	cpu_csr_write(hvip, (ctx->run_ctx.sip & 0x202) << 1);
	cpu_csr_write(hie, (ctx->run_ctx.sie & 0x222) << 1);
	cpu_csr_write(vstvec, ctx->run_ctx.stvec);
	cpu_csr_write(vsscratch, ctx->run_ctx.sscratch);
	cpu_csr_write(vstval, ctx->run_ctx.stval);
	cpu_csr_write(vscause, ctx->run_ctx.scause);
	cpu_csr_write(vsatp, ctx->run_ctx.satp);
}

static void save_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	ctx->run_ctx.sstatus = cpu_csr_read(vsstatus);
	ctx->run_ctx.sepc = cpu_csr_read(vsepc);
	ctx->run_ctx.sip = cpu_csr_read(vsip);
	ctx->run_ctx.sie = cpu_csr_read(vsie);
	ctx->run_ctx.stvec = cpu_csr_read(vstvec);
	ctx->run_ctx.sscratch = cpu_csr_read(vsscratch);
	ctx->run_ctx.stval = cpu_csr_read(vstval);
	ctx->run_ctx.scause = cpu_csr_read(vscause);
	ctx->run_ctx.satp = cpu_csr_read(vsatp);
}

static void load_host_state(struct acrn_vcpu *vcpu)
{
	uint64_t osatp;
	uint64_t s2pt_satp = vcpu->vm->arch_vm.s2pt_satp;

	osatp = cpu_csr_read(hgatp);
	if (osatp != s2pt_satp)
		s2pt_flush_guest(vcpu->vm);
}

static void init_host_state(struct acrn_vcpu *vcpu)
{
	uint64_t value64;
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	pr_dbg("Initialize host state");
	value64 = 0x200000180;
	cpu_csr_set(hstatus, value64);
	ctx->run_ctx.cpu_gp_regs.regs.hstatus = value64;

	/* must set the SPP in order to enter into guest s-mode */
	value64 = 0x2000C2100;
	cpu_csr_set(sstatus, value64);
	ctx->run_ctx.cpu_gp_regs.regs.status = value64;

	value64 = 0x444;
	cpu_csr_write(hideleg, value64);

	value64 = 0xf0bfff;
	cpu_csr_write(hedeleg, value64);

	value64 = 0xc000000000000000;
	cpu_csr_write(henvcfg, value64);

	value64 = 0x7;
	cpu_csr_write(hcounteren, value64);

	s2pt_flush_guest(vcpu->vm);
}

static inline void load_guest_pmp(struct acrn_vcpu *vcpu) {}
#else /* CONFIG_MACRN */
static void init_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	cpu_csr_write(sstatus, 0x2000C0000);
	cpu_csr_write(sepc, ctx->run_ctx.sepc);
	cpu_csr_write(sip, ctx->run_ctx.sip);
	cpu_csr_write(sie, ctx->run_ctx.sie);
	cpu_csr_write(stvec, ctx->run_ctx.stvec);
	cpu_csr_write(sscratch, ctx->run_ctx.sscratch);
	cpu_csr_write(stval, ctx->run_ctx.stval);
	cpu_csr_write(scause, ctx->run_ctx.scause);
	cpu_csr_write(satp, ctx->run_ctx.satp);
}

static void load_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	cpu_csr_write(sstatus, ctx->run_ctx.sstatus);
	cpu_csr_write(sepc, ctx->run_ctx.sepc);
	if (is_service_vm(vcpu->vm))
		cpu_csr_set(mip, (ctx->run_ctx.sip & 0x22));
	else
		cpu_csr_set(mip, ctx->run_ctx.sip);
	cpu_csr_write(sie, ctx->run_ctx.sie);
	cpu_csr_write(stvec, ctx->run_ctx.stvec);
	cpu_csr_write(sscratch, ctx->run_ctx.sscratch);
	cpu_csr_write(stval, ctx->run_ctx.stval);
	cpu_csr_write(scause, ctx->run_ctx.scause);
	cpu_csr_write(satp, ctx->run_ctx.satp);
}

static void save_guest_state(struct acrn_vcpu *vcpu)
{
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	ctx->run_ctx.sstatus = cpu_csr_read(sstatus);
	ctx->run_ctx.sepc = cpu_csr_read(sepc);
	ctx->run_ctx.sip = cpu_csr_read(sip);
	ctx->run_ctx.sie = cpu_csr_read(sie);
	ctx->run_ctx.stvec = cpu_csr_read(stvec);
	ctx->run_ctx.sscratch = cpu_csr_read(sscratch);
	ctx->run_ctx.stval = cpu_csr_read(stval);
	ctx->run_ctx.scause = cpu_csr_read(scause);
	ctx->run_ctx.satp = cpu_csr_read(satp);
}

#define load_host_state(vcpu) do {} while(0)

static void init_host_state(struct acrn_vcpu *vcpu)
{
	uint64_t value64;
	struct guest_cpu_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context];

	pr_dbg("Initialize host state");

#ifdef CONFIG_PLAN_B_SERVICES
	if (vcpu->vm->vm_id == 1) {
		value64 = cpu_csr_read(0x30a);
		if (((value64 >> 4) & 3UL) != 1UL) {
			pr_fatal("[PLANB CACHE] VM1 requires CBIE=01; menvcfg=%lx", value64);
			for (;;) asm volatile("wfi");
		}
		pr_info("[PLANB CACHE] hart=%u menvcfg=%lx CBIE=1 invalidate=flush",
			get_pcpu_id(), value64);
	}
#endif
	/* must set the MPP in order to enter into guest s-mode */
	value64 = 0x200000800;
	cpu_csr_set(mstatus, value64);
	ctx->run_ctx.cpu_gp_regs.regs.status = value64;
	value64 = 0xf0b55f;
#ifdef CONFIG_STATIC_DUAL
	/* PMP instruction-access faults must reach the monitor, which reflects
	 * them to the Guest's S-mode handler without relaxing PMP permissions.
	 */
	value64 &= ~2UL;
#endif
	cpu_csr_write(medeleg, value64);
}

static uint64_t pmp_napot(uint64_t base, uint64_t size)
{
	return (base >> 2) | ((size >> 3) - 1UL);
}

void load_guest_pmp(struct acrn_vcpu *vcpu)
{
#ifdef CONFIG_STATIC_DUAL
	const struct static_vm_info *info = &static_vms[vcpu->vm->vm_id];
	uint64_t cfg = 0x00181808001b0f00UL;
	uint64_t own_begin = info->base >> 2;
	uint64_t own_end = (info->base + info->size) >> 2;
	uint64_t shared = pmp_napot(STATIC_SHARED_BASE, STATIC_SHARED_SIZE);
#ifdef CONFIG_PLATFORM_SG2044
	uint64_t uart = pmp_napot(CONFIG_UART_BASE, CONFIG_UART_SIZE);
	/* The aligned 128 MiB block covers both PLIC and ACLINT on SG2044. */
	uint64_t irq = pmp_napot(CONFIG_PLIC_BASE, 0x08000000UL);
#else
	/* virt's aligned low 256 MiB includes ACLINT and PLIC, while its
	 * UART, virtio and PCI regions stay outside that block.
	 */
	uint64_t uart = pmp_napot(CONFIG_UART_BASE, CONFIG_UART_SIZE);
	uint64_t irq = pmp_napot(0, 0x10000000UL);
#endif
	uint64_t all;
	static bool reported[2];
	if (planb_enclave_load_pmp(vcpu)) return;
	if (cpu_csr_read(pmpcfg0) & 0x8080808080808080UL) {
		pr_fatal("Locked PMP entries prevent static partitioning");
		for (;;) asm volatile("wfi");
	}
	/* OFF0/TOR1 is the exact contiguous private interval, which need not
	 * be NAPOT aligned. VM0 can therefore own low DMA32 and high DDR in
	 * the same private bank. OFF3/TOR4 denies the remaining DDR.
	 */
	if (vcpu->vm->vm_id == 0) cfg |= 0x1f00000000000000UL;
	else cfg |= 0x1800000000000000UL;
	cpu_csr_write(pmpcfg0, 0);
	cpu_csr_write(pmpaddr0, own_begin);
	cpu_csr_write(pmpaddr1, own_end);
	cpu_csr_write(pmpaddr2, shared);
	cpu_csr_write(pmpaddr3, 0x80000000UL >> 2);
	cpu_csr_write(pmpaddr4, static_ddr_end >> 2);
	cpu_csr_write(pmpaddr5, uart);
	cpu_csr_write(pmpaddr6, irq);
	cpu_csr_write(pmpaddr7, UINT64_MAX);
	cpu_csr_write(pmpcfg0, cfg);
	asm volatile("sfence.vma" ::: "memory");
	all = cpu_csr_read(pmpaddr7);
	if (cpu_csr_read(pmpcfg0) != cfg ||
	    cpu_csr_read(pmpaddr0) != own_begin || cpu_csr_read(pmpaddr1) != own_end ||
	    cpu_csr_read(pmpaddr2) != shared ||
	    cpu_csr_read(pmpaddr3) != (0x80000000UL >> 2) ||
	    cpu_csr_read(pmpaddr4) != (static_ddr_end >> 2) ||
	    cpu_csr_read(pmpaddr5) != uart || cpu_csr_read(pmpaddr6) != irq ||
	    (all & (all + 1)) ||
	    all < (BOARD_HV_DEVICE_SIZE >> 3)) {
		pr_fatal("Static PMP readback failed; refusing to enter Guest");
		for (;;) asm volatile("wfi");
	}
	if (!reported[vcpu->vm->vm_id]) {
		reported[vcpu->vm->vm_id] = true;
		pr_info("[STATIC] VM%u HART%u PMP_PRIVATE_READBACK_OK RAM=%lx+%lx",
			vcpu->vm->vm_id, vcpu->pcpu_id, info->base, info->size);
		pr_info("[IRQFIX] VM%u FINAL_ENTRY_SEIP_SYNC_ENABLED", vcpu->vm->vm_id);
		pr_info("[SCHEME_B] VM%u PMP_DENIAL_POLICY=GUEST_TRAP_RECOVERY", vcpu->vm->vm_id);
	}
#else
	uint64_t cfg = 0x1f1818181818UL;
	uint64_t old = cpu_csr_read(pmpcfg0);
	uint64_t monitor = pmp_napot(MACRN_RAM_START, MACRN_RAM_SIZE);
	uint64_t entry = pmp_napot(MACRN_LOAD_START, MACRN_LOAD_SIZE);
	uint64_t uart = pmp_napot(CONFIG_UART_BASE, CONFIG_UART_SIZE);
	uint64_t plic_region = pmp_napot(CONFIG_PLIC_BASE, CONFIG_PLIC_SIZE);
	uint64_t clint_region = pmp_napot(CONFIG_CLINT_BASE, 0x10000UL);
#ifdef CONFIG_BRINGUP_GUEST
	static bool bringup_pmp_reported;
#endif
	(void)vcpu;

	/* All M-mode monitor code is trusted. Ordinary guests cannot access it. */
	if (old & 0x8080808080808080UL) {
		pr_fatal("Locked PMP entries prevent installation of monitor policy");
		for (;;) asm volatile("wfi");
	}
	cpu_csr_write(pmpcfg0, 0);
	cpu_csr_write(pmpaddr0, monitor);
	cpu_csr_write(pmpaddr1, entry);
	cpu_csr_write(pmpaddr2, uart);
	cpu_csr_write(pmpaddr3, plic_region);
	cpu_csr_write(pmpaddr4, clint_region);
	cpu_csr_write(pmpaddr5, UINT64_MAX);
	cpu_csr_write(pmpcfg0, cfg);
	asm volatile("sfence.vma" ::: "memory");
	if (cpu_csr_read(pmpcfg0) != cfg ||
	    cpu_csr_read(pmpaddr0) != monitor ||
	    cpu_csr_read(pmpaddr1) != entry ||
	    cpu_csr_read(pmpaddr2) != uart ||
	    cpu_csr_read(pmpaddr3) != plic_region ||
	    cpu_csr_read(pmpaddr4) != clint_region) {
		pr_fatal("PMP policy readback failed; refusing to enter the guest");
		for (;;) asm volatile("wfi");
	}
#ifdef CONFIG_BRINGUP_GUEST
	/* Embedded bring-up is restricted to one Hart. Keep SBI character
	 * output readable while checking the PMP policy on every resume.
	 */
	if (!bringup_pmp_reported) {
		bringup_pmp_reported = true;
		pr_info("[BRINGUP] PMP_POLICY_READBACK_OK");
	}
#endif
#endif /* CONFIG_STATIC_DUAL */
}
#endif /* CONFIG_MACRN */

/**
 * @pre vcpu != NULL
 */
void init_vmcs(struct acrn_vcpu *vcpu)
{
	void **vcpu_ptr = &get_cpu_var(vcpu_run);

	/* Log message */
	pr_dbg("Initializing VMCS");
	/* Initialize the Virtual Machine Control Structure (VMCS) */
	init_host_state(vcpu);
	init_guest_state(vcpu);
	*vcpu_ptr = (void *)vcpu;
}

/**
 * @pre vcpu != NULL
 */
void load_vmcs(struct acrn_vcpu *vcpu)
{
	void **vcpu_ptr = &get_cpu_var(vcpu_run);

#ifdef CONFIG_MACRN
	/* Request processing and this function used to leave an M-interrupt
	 * window. A late IRQ could be claimed into the virtual PLIC after the
	 * EXTINT request was checked, then the Guest entered WFI without SEIP.
	 * Freeze monitor IRQs before restoring CSRs, then derive the SEIP level
	 * from the current PLIC state. vmx_vmrun keeps MIE clear until mret;
	 * higher-privilege IRQs can interrupt S/U code on the next entry.
	 */
	cpu_disable_mirq();
#endif
	planb_enclave_prepare_entry(vcpu);
	load_host_state(vcpu);
	load_guest_state(vcpu);
#ifdef CONFIG_MACRN
	#ifdef CONFIG_PLAN_B_SERVICES
	vplic_flush_deferred(vcpu);
	#endif
	if (!planb_enclave_active(vcpu)) vcpu_inject_extint(vcpu);
#endif
	load_guest_pmp(vcpu);
	*vcpu_ptr = (void *)vcpu;
}

void save_vmcs(struct acrn_vcpu *vcpu)
{
	save_guest_state(vcpu);
}
