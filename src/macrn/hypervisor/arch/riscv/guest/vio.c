/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/atomic.h>
#include <asm/apicreg.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vpci.h>
#include <asm/guest/instr_emul.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/virq.h>
#include <asm/vmx.h>
#include <asm/guest/s2vm.h>
#include <asm/pgtable.h>
#include <asm/mem.h>
#include <asm/platform_boot.h>
#include <asm/static_vm.h>
#include <asm/pmp_live.h>
#include <io_req.h>
#include <trace.h>
#include <logmsg.h>

void arch_fire_hsm_interrupt(void)
{
	struct acrn_vm *sos_vm;
	struct acrn_vcpu *vcpu;

	sos_vm = get_sos_vm();
	vcpu = vcpu_from_vid(sos_vm, BSP_CPU_ID);

	vplic_accept_intr(vcpu, get_hsm_notification_vector(), true);
}

/**
 * Dummy handler, riscv doesn't support pio.
 */
void
emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	pr_fatal("Wrong state, should not reach here!\n");
}

#ifdef CONFIG_MACRN
#ifdef CONFIG_STATIC_DUAL
static uint64_t get_gpa(struct run_context *ctx, uint64_t gva);
uint32_t get_instruction(struct run_context *ctx, uint32_t *xlen)
{
	uint64_t pc = ctx->cpu_gp_regs.regs.ip, pa;
	uint32_t ins;
	bool translated = (ctx->satp >> 60) != 0;
	*xlen = 0;
	pa = translated ? get_gpa(ctx, pc) : pc;
	if (!static_vm_current_ram_range(pa, 2) || (pa & 1)) return 0;
	ins = *(volatile uint16_t *)pa;
	if ((ins & 3) == 3) {
		pa = translated ? get_gpa(ctx, pc + 2) : pc + 2;
		if (!static_vm_current_ram_range(pa, 2) || (pa & 1)) return 0;
		ins |= (uint32_t)*(volatile uint16_t *)pa << 16;
		*xlen = 32;
	} else *xlen = 16;
	return ins;
}
#else
uint32_t get_instruction(struct run_context *ctx, uint32_t *xlen)
{
	uint64_t m = 0xa0000;
	register uint32_t ins;
	uint64_t st;
	uint64_t status, gva;

	status = ctx->cpu_gp_regs.regs.status;
	gva = ctx->cpu_gp_regs.regs.ip;
	ASSERT((status & 0x1800) == 0x800);
	status |= m;
	local_irq_disable();
	st = cpu_csr_read(mstatus);
	asm volatile (
		"csrw mstatus, %[status] \n\t"
		"lw %[ins], (%[gva]) \n\t"
		"csrw mstatus, %[old] \n\t"
		: [ins] "=r"(ins)
		: [gva] "r"(gva), [status] "r"(status), [old] "r"(st)
		: "memory"
	);
	local_irq_enable();

	if ((ins & 0x3) != 0x3) {
		*xlen = 16;
		ins &= 0xffff;
	} else {
		*xlen = 32;
	}

	return ins;
}
#endif

static uint64_t get_gpa(struct run_context *ctx, uint64_t gva)
{
	uint64_t mode = ctx->satp >> 60;
	uint64_t *table = (uint64_t *)((ctx->satp & ((1UL << 44) - 1UL)) << 12);
	int level;
	if (mode < 8 || mode > 10) return INVALID_HPA;
	/* Sv39, Sv48 and Sv57 have 3, 4 and 5 page-table levels. */
	for (level = (int)mode - 6; level >= 0; --level) {
		uint64_t shift = 12 + 9 * level;
		uint64_t *p = table + ((gva >> shift) & 0x1ffUL);
		uint64_t entry, pa;
#ifdef CONFIG_STATIC_DUAL
		if (!static_vm_current_ram_range((uint64_t)p, sizeof(*p))) return INVALID_HPA;
#else
		if (!platform_guest_ram_range((uint64_t)p, sizeof(*p))) return INVALID_HPA;
#endif
		entry = __atomic_load_n(p, __ATOMIC_RELAXED);
		if (!(entry & 1UL) || ((entry & 6UL) == 4UL)) return INVALID_HPA;
		pa = ((entry >> 10) & ((1UL << 44) - 1UL)) << 12;
		if (entry & 0xaUL)
			return (pa & ~((1UL << shift) - 1UL)) | (gva & ((1UL << shift) - 1UL));
		table = (uint64_t *)pa;
	}
	return INVALID_HPA;
}

static bool need_pagetable_walk(uint64_t satp)
{
	return (satp & 0xF000000000000000UL) != 0;
}

#else /* !CONFIG_MACRN */

static uint64_t get_gpa(struct run_context *ctx, __unused uint64_t gva)
{
	return (ctx->cpu_gp_regs.regs.htval << 2) | (ctx->cpu_gp_regs.regs.tval & 0x3);
}

uint32_t get_instruction(struct run_context *ctx, __unused uint32_t *len)
{
	uint64_t ins = ctx->cpu_gp_regs.regs.htinst;

	*len = ((ins & 0x3) == 0x1)? 16: 32;

	return ins;
}

static bool need_pagetable_walk(uint64_t satp)
{
	return true;
}
#endif

#ifdef CONFIG_STATIC_DUAL
/* Reflect the fault, never emulate a protected RAM access in M-mode. The
 * Guest owns its exception recovery (e.g. a Linux exception-table fixup).
 * In particular, do not skip an instruction or invent a successful load.
 */
int32_t vcpu_reflect_access_fault(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	uint64_t cause = ctx->cpu_gp_regs.regs.cause;
	uint64_t previous = (ctx->cpu_gp_regs.regs.status >> 11) & 3UL;
	uint64_t vector = ctx->stvec & ~3UL;
	uint64_t physical = need_pagetable_walk(ctx->satp) ? get_gpa(ctx, vector) : vector;
	uint64_t trap_status;

	/* Exception vectors use BASE even when stvec is vectored. Verify only
	 * the handler address, through private page tables; never dereference
	 * mtval, which may name another Guest or the monitor.
	 */
	if ((cause != HX_EXIT_INS_ACCESS && cause != HX_EXIT_LOAD_ACCESS &&
	     cause != HX_EXIT_STORE_ACCESS) || previous > 1UL ||
	    (ctx->stvec & 3UL) > 1UL || vector == 0UL ||
	    !static_vm_current_ram_range(physical, 4UL)) {
		pr_err("[STATIC] VM%u ACCESS_FAULT_NO_HANDLER cause=%lx stvec=%lx",
		       vcpu->vm->vm_id, cause, ctx->stvec);
		zombie_vcpu(vcpu, VCPU_ZOMBIE);
		return -EACCES;
	}

	/* Reproduce S-mode trap entry: SPIE <- SIE, SIE <- 0, SPP <- old
	 * privilege. mret must enter the S handler even if the fault was in U.
	 * Leave GPRs, satp, PMP permissions and the faulting instruction intact.
	 */
	trap_status = ctx->sstatus & ~(2UL | 32UL | 256UL);
	/* vm_exit captures the Guest's mstatus before restoring the monitor's
	 * status. The later CSR snapshot can already carry the monitor's SIE.
	 */
	if (ctx->cpu_gp_regs.regs.status & 2UL)
		trap_status |= 32UL;
	if (previous == 1UL)
		trap_status |= 256UL;
#ifdef CONFIG_STATIC_LIVE_TEST
	{
		uint64_t va = cause == HX_EXIT_INS_ACCESS ? ctx->cpu_gp_regs.regs.ip : ctx->cpu_gp_regs.regs.tval;
		uint64_t pa = need_pagetable_walk(ctx->satp) ? get_gpa(ctx, va) : va;
		pmp_live_record(vcpu->vm->vm_id, cause, pa, va, ctx->cpu_gp_regs.regs.ip);
	}
#endif
	ctx->sepc = ctx->cpu_gp_regs.regs.ip;
	ctx->scause = cause;
	ctx->stval = ctx->cpu_gp_regs.regs.tval;
	ctx->sstatus = trap_status;
	ctx->cpu_gp_regs.regs.status &= ~(2UL | 32UL | 256UL | 0x1800UL);
	ctx->cpu_gp_regs.regs.status |= (trap_status & (32UL | 256UL)) | 0x800UL;
	ctx->cpu_gp_regs.regs.ip = vector;
	vcpu_retain_ip(vcpu);
	pr_warn("[STATIC] VM%u PMP_FAULT_REFLECTED cause=%lx pc=%lx address=%lx",
		vcpu->vm->vm_id, cause, ctx->sepc, ctx->stval);
	return 0;
}
#endif

int32_t mmio_access_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int ret;
	int32_t status = -1;
	uint64_t exit_qual;
	uint64_t gva, gpa;
	uint32_t ins, xlen = 0;
	struct io_request *io_req = &vcpu->req;
	struct acrn_mmio_request *mmio_req = &io_req->reqs.mmio_request;
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	/* Handle page fault from guest */
	exit_qual = vcpu->arch.exit_qualification;
	gva = ctx->cpu_gp_regs.regs.tval;
	if (need_pagetable_walk(ctx->satp))
		gpa = get_gpa(ctx, gva);
	else
		gpa = gva;

#ifdef CONFIG_MACRN
	/* A protected-memory fault must never become an M-mode proxy read/write. */
	if ((gpa >= MACRN_RAM_START && gpa < MACRN_RAM_START + MACRN_RAM_SIZE) ||
	    (gpa >= MACRN_LOAD_START && gpa < MACRN_LOAD_START + MACRN_LOAD_SIZE)) {
#ifdef CONFIG_STATIC_DUAL
		pr_warn("Guest attempted monitor-memory access: %lx", gpa);
		return vcpu_reflect_access_fault(vcpu);
#else
		pr_fatal("Guest attempted monitor-memory access: %lx", gpa);
		zombie_vcpu(vcpu, VCPU_ZOMBIE);
		return -EACCES;
#endif
	}
#endif

#ifdef CONFIG_STATIC_DUAL
	/* Tianquan's ESWIN kernel maps DIE0 cached PA 0x80000000 to
	 * uncached PA 0xc000000000 over a 32-GiB window. Resolve only VM1
	 * devices that
	 * are already explicitly emulated; never turn a RAM/protected alias
	 * into a physical proxy access or change VM0's native device map.
	 */
	if (vcpu->vm->vm_id == 1 && gpa >= 0xc000000000UL &&
	    gpa - 0xc000000000UL < 0x800000000UL) {
		uint64_t device = gpa - 0xc000000000UL + 0x80000000UL;
		if (static_vm_emulated_mmio(1, device, 8)) gpa = device;
	}
	if (!static_vm_emulated_mmio(vcpu->vm->vm_id, gpa, 8)) {
		if (vcpu->vm->vm_id == 0 && gpa >= static_vms[1].base &&
		    gpa - static_vms[1].base < static_vms[1].size) {
			pr_warn("[SM] TEE security check: the hypervisor is trying to r/w the protected region");
			pr_warn("[SM] TEE security check: protected access blocked, address=0x%lx", gpa);
		}
		if (gpa >= 0x80000000UL && gpa < static_ddr_end)
			pr_warn("[STATIC] VM%u DENIED_PRIVATE_ACCESS address=%lx", vcpu->vm->vm_id, gpa);
		else
			pr_warn("[STATIC] VM%u DENIED_DEVICE_ACCESS address=%lx", vcpu->vm->vm_id, gpa);
		return vcpu_reflect_access_fault(vcpu);
	}
#endif
	ins = get_instruction(ctx, &xlen);
#ifdef CONFIG_STATIC_DUAL
	if (!xlen)
		return vcpu_reflect_access_fault(vcpu);
#endif

	io_req->io_type = ACRN_IOREQ_TYPE_MMIO;

	/* Specify if read or write operation */
	switch (exit_qual) {
	case HX_EXIT_PF_GUEST_STORE:
	case HX_EXIT_STORE_ACCESS:
		/* Write operation */
		mmio_req->direction = ACRN_IOREQ_DIR_WRITE;
		mmio_req->value = 0UL;
		break;
	case HX_EXIT_PF_GUEST_LOAD:
	case HX_EXIT_LOAD_ACCESS:
		/* Read operation */
		mmio_req->direction = ACRN_IOREQ_DIR_READ;
		break;
	default:
		status = -1;
		pr_acrnlog("unsupported access to address: 0x%016lx", gpa);
		return status;
		break;
	}

	mmio_req->address = gpa;
	ret = decode_instruction(vcpu, ins, xlen);
	if (ret > 0) {
		mmio_req->size = (uint64_t)ret;
#ifdef CONFIG_STATIC_LIVE_TEST
		if (pmp_live_range(gpa, mmio_req->size)) {
			if (gpa == PMP_LIVE_AUDIT + PMP_LIVE_COMMAND && xlen == 32 &&
			    exit_qual == HX_EXIT_STORE_ACCESS && (ins & 0x707fU) == 0x3023U &&
			    (ctx->cpu_gp_regs.regs.status & 0x1800UL) == 0) {
				status = emulate_instruction(vcpu);
				if (status) return status;
				return pmp_live_begin(vcpu, mmio_req->value);
			}
			/* Audit is read-only, supports aligned ld only, and never
			 * grants direct access to the physical backing page.
			 */
			if (exit_qual != HX_EXIT_LOAD_ACCESS || xlen != 32 ||
			    (ins & 0x707fU) != 0x3003U || (gpa & 7UL))
				return vcpu_reflect_access_fault(vcpu);
			mmio_req->value = pmp_live_read(vcpu->vm->vm_id, gpa - PMP_LIVE_AUDIT);
			return emulate_instruction(vcpu);
		}
#endif
		if (gpa == INVALID_HPA) {
			mmio_req->value = 0UL;
			emulate_instruction(vcpu);
			return 0;
		}

		/*
		 * For MMIO write, ask DM to run MMIO emulation after
		 * instruction emulation. For MMIO read, ask DM to run MMIO
		 * emulation at first.
		 */

		/* Determine value being written. */
		if (mmio_req->direction == ACRN_IOREQ_DIR_WRITE) {
			status = emulate_instruction(vcpu);
			if (status != 0) {
				ret = -EFAULT;
			}
		}

		if (ret > 0) {
			status = emulate_io(vcpu, io_req);
		}
	} else {
#ifdef CONFIG_STATIC_DUAL
		return vcpu_reflect_access_fault(vcpu);
#endif
		if (ret == -EFAULT) {
			pr_info("page fault happen during decode_instruction");
			status = 0;
		}
	}
	if (ret <= 0) {
		//pr_acrnlog("Guest Linear Address: 0x%016lx", exec_vmread(VMX_GUEST_LINEAR_ADDR));
		pr_acrnlog("Guest Physical Address address: 0x%016lx", gpa);
	}

	return status;
}

/**
 * Dummy handler, riscv doesn't support pio.
 */
void allow_guest_pio_access(struct acrn_vm *vm, uint16_t port_address,
		uint32_t nbytes)
{
	pr_fatal("Wrong state, should not reach here!\n");
}

void deny_guest_pio_access(struct acrn_vm *vm, uint16_t port_address,
		uint32_t nbytes)
{
	pr_fatal("Wrong state, should not reach here!\n");
}
