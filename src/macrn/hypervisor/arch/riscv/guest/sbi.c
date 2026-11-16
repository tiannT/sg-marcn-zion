/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <lib/types.h>
#include <asm/lib/bits.h>
#include <asm/cpu.h>
#include <asm/vmx.h>
#include <asm/irq.h>
#include <asm/tlb.h>
#include <asm/smp.h>
#include <asm/notify.h>
#include <asm/cache.h>
#include <asm/early_printk.h>
#include <console.h>
#include <asm/io.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vuart.h>
#include <asm/guest/vclint.h>
#include <asm/static_vm.h>
#include <asm/planb_enclave.h>
#include <asm/planb_runtime.h>
#include "sbi.h"
#include "rpmi.h"
#include "tee.h"

#ifdef CONFIG_STATIC_DUAL
/* Keep diagnostic output from the two Harts readable. Each VM owns a
 * separate bounded line buffer; no guest-provided pointer is followed.
 */
static spinlock_t guest_console_lock;
static char guest_lines[2][512];
static unsigned int guest_line_size[2];
#ifdef CONFIG_PLAN_B_RUNTIME
void planb_guest_console_reset(uint16_t id)
{
 uint64_t flags;
 spin_lock_irqsave(&guest_console_lock, &flags);
 memset(guest_lines[id], 0, sizeof(guest_lines[id]));
 guest_line_size[id] = 0;
 spin_unlock_irqrestore(&guest_console_lock, flags);
}
#endif
static void guest_putchar(struct acrn_vcpu *vcpu, char ch)
{
	uint16_t id = vcpu->vm->vm_id;
	unsigned int i;
	uint64_t flags;
	spin_lock_irqsave(&guest_console_lock, &flags);
	guest_lines[id][guest_line_size[id]++] = ch;
	if (ch == '\n' || guest_line_size[id] == sizeof(guest_lines[id])) {
		for (i = 0; i < guest_line_size[id]; ++i) early_putch(guest_lines[id][i]);
		guest_line_size[id] = 0;
	}
	spin_unlock_irqrestore(&guest_console_lock, flags);
}
#endif

static void sbi_ecall_base_probe(unsigned long id, unsigned long *out_val)
{
	*out_val = 0;
	switch (id) {
	case SBI_ID_BASE:
	case SBI_ID_IPI:
	case SBI_ID_RFENCE:
	case SBI_ID_TIMER:
	case SBI_ID_HSM:
#ifdef CONFIG_STATIC_DUAL
	case SBI_ID_DBCN:
#endif
#ifndef CONFIG_STATIC_DUAL
	case SBI_ID_MPXY:
#endif
		*out_val = 1;
		break;
#if defined(RUN_ON_QEMU) && !defined(CONFIG_PLAN_B_SERVICES)
	case SBI_ID_SRST:
		*out_val = 1;
		break;
#endif
	default:
		break;
	}

	return;
}

#ifdef CONFIG_MACRN
static inline uint64_t get_mvendorid(void)
{
	return cpu_csr_read(mvendorid);
}

static inline uint64_t get_marchid(void)
{
	return cpu_csr_read(marchid);
}

static inline uint64_t get_mimpid(void)
{
	return cpu_csr_read(mimpid);
}
#else
static inline uint64_t get_mvendorid(void)
{
	return 0;
}

static inline uint64_t get_marchid(void)
{
	return 0;
}

static inline uint64_t get_mimpid(void)
{
	return 0;
}
#endif

static void sbi_base_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	unsigned long *ret = &regs->a0;
	bool fail = false;
	unsigned long funcid = regs->a6;
	unsigned long *out_val = &regs->a1;

	switch (funcid) {
	case SBI_TYPE_BASE_GET_SPEC_VERSION:
		*out_val = SBI_SPEC_VERSION_MAJOR << 24;
		*out_val = *out_val | SBI_SPEC_VERSION_MINOR;
		break;
	case SBI_TYPE_BASE_GET_IMP_ID:
		*out_val = SBI_ACRN_IMPID;
		break;
	case SBI_TYPE_BASE_GET_IMP_VERSION:
		*out_val = SBI_ACRN_VERSION_MAJOR << 24;
		*out_val = *out_val | SBI_ACRN_VERSION_MINOR;
		break;
	case SBI_TYPE_BASE_GET_MVENDORID:
		*out_val = get_mvendorid();
		break;
	case SBI_TYPE_BASE_GET_MARCHID:
		*out_val = get_marchid();
		break;
	case SBI_TYPE_BASE_GET_MIMPID:
		*out_val = get_mimpid();
		break;
	case SBI_TYPE_BASE_PROBE_EXT:
		sbi_ecall_base_probe(regs->a0, out_val);
		break;
	default:
		fail = true;
		break;
	}

	if (fail)
		*ret = SBI_ENOTSUPP;
	else
		*ret = SBI_SUCCESS;

	return;
}

static void sbi_timer_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	unsigned long *ret = &regs->a0;
	unsigned long funcid = regs->a6;
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	bool sstc = false;

#ifdef CONFIG_MACRN
	sstc = !!(cpu_csr_read(0x30a) & 0x8000000000000000UL);
#endif
	if (funcid == SBI_TYPE_TIME_SET_TIMER) {
		if (sstc) {
			cpu_csr_write(0x14d, regs->a0);
			*ret = SBI_SUCCESS;
		} else {
			ctx->sip &= ~CLINT_VECTOR_STI;
#ifdef CONFIG_MACRN
			cpu_csr_clear(mip, CLINT_VECTOR_STI);
#endif
			vclint_write_tmr(vcpu_vclint(vcpu), vcpu->vcpu_id, regs->a0);
			*ret = SBI_SUCCESS;
		}
	} else {
		*ret = SBI_ENOTSUPP;
	}

	return;
}

static void send_vipi_mask(struct acrn_vcpu *vcpu, uint64_t mask, uint64_t base)
{
	uint16_t offset;
	uint64_t count = vcpu->vm->hw.created_vcpus;
	if (base == UINT64_MAX) { base = 0; mask = (1UL << count) - 1; }
	if (base >= count) return;

	offset = ffs64(mask);

	while (mask && offset < count - base) {
		struct acrn_vcpu *t = &vcpu->vm->hw.vcpu[base + offset];
		struct acrn_vclint *vclint = vcpu_vclint(t);

		clear_bit(offset, &mask);
		vclint_send_ipi(vclint, base + offset);
		offset = ffs64(mask);
	}
}

static void sbi_ipi_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	unsigned long *ret = &regs->a0;
	unsigned long funcid = regs->a6;

	if (funcid == SBI_TYPE_IPI_SEND_IPI) {
		send_vipi_mask(vcpu, regs->a0, regs->a1);
		*ret = SBI_SUCCESS;
	} else {
		*ret = SBI_ENOTSUPP;
	}

	return;
}

static void sbi_rcall_sfence_vma(struct sbi_rfence_call *rcall)
{
	uint64_t base = rcall->base;
	uint64_t size = rcall->size;
	uint64_t i;

	if ((base == 0 && size == 0) || size >= 0x400000UL) {
		flush_guest_vtlb_local();
		return;
	}

	for (i = 0; i < size; i += PAGE_SIZE)
		flush_guest_vtlb_addr(base + i);
}

static void sbi_rcall_sfence_vma_asid(struct sbi_rfence_call *rcall)
{
	uint64_t base = rcall->base;
	uint64_t size  = rcall->size;
	uint64_t asid  = rcall->asid;
	uint64_t i;

	if (base == 0 && size == 0) {
		flush_guest_vtlb_local();
		return;
	}

	if (size >= 0x400000UL) {
		flush_guest_vtlb_asid(asid);
		return;
	}

	for (i = 0; i < size; i += PAGE_SIZE)
		flush_guest_vtlb_addr_asid(base + i, asid);
}

static void sbi_rcall_fence_i(struct sbi_rfence_call *rcall)
{
	invalidate_icache_local();
}

static void sbi_rfence_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;
	uint64_t funcid = regs->a6;
	uint64_t mask = regs->a0;
	uint64_t base = regs->a1;
	uint64_t rcall_mask = 0;
	smp_call_func_t func = NULL;
	struct sbi_rfence_call rcall;
	uint16_t offset;

	*ret = SBI_SUCCESS;
	switch (funcid) {
	case SBI_TYPE_RFENCE_FNECE_I:
		func = (smp_call_func_t)sbi_rcall_fence_i;
		break;
	case SBI_TYPE_RFENCE_SFNECE_VMA:
		func = (smp_call_func_t)sbi_rcall_sfence_vma;
		rcall.base = regs->a2;
		rcall.size = regs->a3;
		break;
	case SBI_TYPE_RFENCE_SFNECE_VMA_ASID:
		func = (smp_call_func_t)sbi_rcall_sfence_vma_asid;
		rcall.base = regs->a2;
		rcall.size = regs->a3;
		rcall.asid = regs->a4;
		break;
	default:
		*ret = SBI_ENOTSUPP;
		break;
	}

	if (func == NULL)
		return;
	if (base == UINT64_MAX) {
		base = 0;
		mask = (1UL << vcpu->vm->hw.created_vcpus) - 1;
	}
	if (base >= vcpu->vm->hw.created_vcpus) return;
	offset = ffs64(mask);
	while (mask && offset < vcpu->vm->hw.created_vcpus - base) {
		uint16_t t = offset + base;

		clear_bit(offset, &mask);
		t = vcpu->vm->hw.vcpu[t].pcpu_id;
		set_bit(t, &rcall_mask);
		offset = ffs64(mask);
	}
	smp_call_function(rcall_mask, func, (void *)&rcall);

	return;
}

static void sbi_hsm_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;
	uint64_t *out_val = &regs->a1;
	uint64_t funcid = regs->a6;
	uint64_t hartid = regs->a0;
	uint64_t saddr = regs->a1;
	uint64_t arg1 = regs->a2;
	struct acrn_vcpu *target;
	struct guest_cpu_context *ctx;
	if (hartid >= vcpu->vm->hw.created_vcpus) {
		*ret = SBI_EINVAL_PARAM;
		*out_val = 0;
		return;
	}

	target = &vcpu->vm->hw.vcpu[hartid];
	ctx = &target->arch.contexts[target->arch.cur_context];
	*ret = SBI_SUCCESS;
	*out_val = 0;
	switch (funcid) {
	case SBI_TYPE_HSM_HART_START:
#ifdef CONFIG_STATIC_DUAL
		/* Both fixed Harts are started by the monitor. A guest cannot
		 * overwrite even its own live context using HART_START.
		 */
		*ret = SBI_ENOTSUPP;
		break;
#endif
		vcpu_set_rip(target, saddr);
		vcpu_set_gpreg(target, CPU_REG_A0, target->vcpu_id);
		vcpu_set_gpreg(target, CPU_REG_A1, arg1);
		ctx->run_ctx.satp = 0;
		ctx->run_ctx.sstatus &= ~0x2UL;
		launch_vcpu(target);
		break;
	case SBI_TYPE_HSM_HART_GET_STATUS:
		if (target->state != VCPU_RUNNING)
			*out_val = 1;
		break;
	case SBI_TYPE_HSM_HART_STOP:
	case SBI_EXT_HSM_HART_SUSPEND:
	default:
		*ret = SBI_ENOTSUPP;
		break;
	}

	return;
}

static void sbi_srst_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	regs->a0 = SBI_ENOTSUPP;

	return;
}

static void sbi_pmu_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	regs->a0 = SBI_ENOTSUPP;

	return;
}

#ifndef CONFIG_STATIC_DUAL
static void sbi_mpxy_get_shm_size(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *val = &regs->a1;
	uint64_t *ret = &regs->a0;

	*val = PAGE_SIZE;
	*ret = SBI_SUCCESS;
}

static void sbi_mpxy_set_shm(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;

	vcpu->mpxy.base = (uint64_t *)regs->a0;
	*ret = SBI_SUCCESS;
}

static void sbi_mpxy_get_channel_ids(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;
	uint64_t *val = &regs->a1;
	uint32_t *p = (uint32_t *)vcpu->mpxy.base;

	*val = 0;
	*p++ = 0;
	*p++ = 2;
	*p++ = 0;
	*p++ = 1;

	*ret = SBI_SUCCESS;
}

static void sbi_mpxy_read_attrs(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;
	uint32_t *p = (uint32_t *)vcpu->mpxy.base;

	*p++ = 0;
	*p++ = 1;
	*p++ = 1024;
	*p++ = 100;
	*p++ = 100;
	*p++ = 0x8;

	*ret = SBI_SUCCESS;
}

static void sbi_mpxy_write_attrs(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;

	*ret = SBI_SUCCESS;
}

static void sbi_mpxy_send_msg_with_resp(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t channel_id = regs->a0;
	uint64_t msg_id = regs->a1;
	uint64_t *ret = &regs->a0;

	if (channel_id == 0 || channel_id == 1) {
		tee_switch(vcpu);
	} else if ((channel_id & 0x10) != 0 && msg_id == RPMI_REQFWD_RETRI_MESG) {
		tee_switch(vcpu);
	} else if ((channel_id & 0x10) != 0 && msg_id == RPMI_REQFWD_COMPL_MESG) {
		struct rpmi_reqfwd_request *p = (struct rpmi_reqfwd_request *)vcpu->mpxy.base;
		tee_answer_ree(vcpu);
		p->compl_status = SBI_SUCCESS;
		p->compl_num = 0x1;
	}

	*ret = SBI_SUCCESS;
}

static void sbi_mpxy_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t *ret = &regs->a0;
	uint64_t funcid = regs->a6;

	switch (funcid) {
	case SBI_TYPE_MPXY_GET_SHM_SIZE:
		sbi_mpxy_get_shm_size(vcpu, regs);
		break;
	case SBI_TYPE_MPXY_SET_SHM:
		sbi_mpxy_set_shm(vcpu, regs);
		break;
	case SBI_TYPE_MPXY_GET_CHANNEL_IDS:
		sbi_mpxy_get_channel_ids(vcpu, regs);
		break;
	case SBI_TYPE_MPXY_READ_ATTRS:
		sbi_mpxy_read_attrs(vcpu, regs);
		break;
	case SBI_TYPE_MPXY_WRITE_ATTRS:
		sbi_mpxy_write_attrs(vcpu, regs);
		break;
	case SBI_TYPE_MPXY_SEND_MSG_WITH_RESP:
		sbi_mpxy_send_msg_with_resp(vcpu, regs);
		break;
	default:
		*ret = SBI_ENOTSUPP;
		break;
	}

	return;
}

#endif /* CONFIG_STATIC_DUAL */

static void sbi_undefined_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	regs->a0 = SBI_ENOTSUPP;

	return;
}

#ifdef CONFIG_STATIC_DUAL
static void sbi_dbcn_handler(struct acrn_vcpu *vcpu, struct cpu_regs *regs)
{
	uint64_t bytes = regs->a0, address = regs->a1, high = regs->a2;
	uint64_t function = regs->a6, count, i;
	const uint8_t *buffer;
	regs->a0 = SBI_SUCCESS;
	regs->a1 = 0;
	if (function == 2) {
		guest_putchar(vcpu, (char)bytes);
		return;
	}
	if (function > 2) {
		regs->a0 = SBI_ENOTSUPP;
		return;
	}
	if (!bytes) return;
	/* Addresses are physical. Never follow the caller's pointer outside
	 * its own RAM or the explicit shared region, even from M-mode.
	 */
	if (high || !static_vm_guest_ram_range(vcpu->vm, address, bytes)) {
		regs->a0 = SBI_EINVAL_PARAM;
		return;
	}
	/* Console input is delivered through each VM's UART. A nonblocking
	 * DBCN read currently has no queued input and must not steal VM0 input.
	 */
	if (function == 1) return;
	count = bytes < 4096UL ? bytes : 4096UL;
	buffer = (const uint8_t *)address;
	for (i = 0; i < count; i++) guest_putchar(vcpu, (char)buffer[i]);
	regs->a1 = count;
}
#endif

static const struct sbi_ecall_dispatch sbi_dispatch_table[NR_HX_EXIT_REASONS] = {
	[SBI_TYPE_BASE] = {
		.ext_id = SBI_ID_BASE,
		.handler = sbi_base_handler},
	[SBI_TYPE_TIMER] = {
		.ext_id = SBI_ID_TIMER,
		.handler = sbi_timer_handler},
	[SBI_TYPE_IPI] = {
		.ext_id = SBI_ID_IPI,
		.handler = sbi_ipi_handler},
	[SBI_TYPE_RFENCE] = {
		.ext_id = SBI_ID_RFENCE,
		.handler = sbi_rfence_handler},
	[SBI_TYPE_HSM] = {
		.ext_id = SBI_ID_HSM,
		.handler = sbi_hsm_handler},
	[SBI_TYPE_SRST] = {
		.ext_id = SBI_ID_SRST,
		.handler = sbi_srst_handler},
	[SBI_TYPE_PMU] = {
		.ext_id = SBI_ID_PMU,
		.handler = sbi_pmu_handler},
#ifdef CONFIG_STATIC_DUAL
	[SBI_TYPE_DBCN] = {
		.ext_id = SBI_ID_DBCN,
		.handler = sbi_dbcn_handler},
#endif
#ifndef CONFIG_STATIC_DUAL
	[SBI_TYPE_MPXY] = {
		.ext_id = SBI_ID_MPXY,
		.handler = sbi_mpxy_handler},
#endif
	[SBI_MAX_TYPES] = {
		.ext_id = SBI_VENDOR_START,
		.handler = sbi_undefined_handler},
};

int sbi_ecall_handler(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	struct cpu_regs *regs = &ctx->cpu_gp_regs.regs;
	uint32_t id = regs->a7;
	const struct sbi_ecall_dispatch *d = &sbi_dispatch_table[SBI_MAX_TYPES];

	if (planb_enclave_sbi(vcpu)) return 0;
	/* EDK II and early Linux can use the legacy SBI console. */
	if (id == 1U) {
#ifdef CONFIG_STATIC_DUAL
		guest_putchar(vcpu, (char)regs->a0);
#else
		early_putch((char)regs->a0);
#endif
		regs->a0 = SBI_SUCCESS;
		return 0;
	}
	if (id == 2U) {
		regs->a0 = (int64_t)early_getch();
		return 0;
	}
#ifdef CONFIG_PLAN_B_SERVICES
	/* A protected guest's poweroff/panic must not reset its management VM. */
	if (id == 8U || id == SBI_ID_SRST) {
		regs->a0 = SBI_ENOTSUPP;
		regs->a1 = 0;
		return 0;
	}
#elif defined(RUN_ON_QEMU)
	if (id == 8U || (id == SBI_ID_SRST && regs->a6 == 0)) {
		struct acrn_vuart *vu = vm_console_vuart(vcpu->vm);
		console_flush_vuart(vu);
		mmio_writel(0x5555, (void *)0x100000UL);
		for (;;) asm volatile("wfi");
	}
#endif

	for (uint32_t i = 0; i < SBI_MAX_TYPES; i++) {
		if (sbi_dispatch_table[i].handler && id == sbi_dispatch_table[i].ext_id) {
			d = &sbi_dispatch_table[i];
			break;
		}
	}

	d->handler(vcpu, regs);

	return 0;
}
