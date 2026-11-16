/* SPDX-License-Identifier: BSD-3-Clause */
#if defined(CONFIG_STATIC_DUAL) && defined(CONFIG_STATIC_LIVE_TEST)
#include <types.h>
#include <asm/static_vm.h>
#include <asm/pmp_live.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/cpu.h>
#include <logmsg.h>
#include <asm/per_cpu.h>

/* Read-only observation of this VM's real faults, never supplied by a Guest. */
static uint64_t audit[2][17];
static struct {
	bool active;
	uint64_t medeleg, mideleg, command, positive_controls;
	struct run_context saved;
} probes[2];

bool pmp_live_idle(uint16_t vm)
{
 return vm < 2 && !probes[vm].active;
}
void pmp_live_reset_idle(uint16_t vm)
{
 if (pmp_live_idle(vm)) memset(&probes[vm], 0, sizeof(probes[vm]));
}

bool pmp_live_range(uint64_t address, uint64_t size)
{
	return size && address >= PMP_LIVE_AUDIT && address - PMP_LIVE_AUDIT < 4096UL &&
		size <= 4096UL - (address - PMP_LIVE_AUDIT);
}

uint64_t pmp_live_read(uint16_t vm, uint64_t offset)
{
	if (vm >= 2 || (offset & 7UL)) return 0;
	/* Diagnostic counters only; never expose peer registers or RAM data. */
	if (offset >= 0x200 && offset <= 0x210)
		return audit[1U-vm][10 + (offset-0x200)/8];
	if (offset == 0x218) return probes[1U-vm].positive_controls;
	if (offset >= sizeof(audit[0])) return 0;
	switch (offset) {
	case 0: return PMP_LIVE_MAGIC;
	case 8: return PMP_LIVE_ABI;
	case 16: return vm;
	case 24: return static_vms[vm].base + PMP_LIVE_PAGE_OFFSET;
	case 32: return static_vms[1U-vm].base + PMP_LIVE_PAGE_OFFSET;
	case 120: {
		volatile uint64_t *a = (void *)(static_vms[0].base + PMP_LIVE_PAGE_OFFSET);
		volatile uint64_t *b = (void *)(static_vms[1].base + PMP_LIVE_PAGE_OFFSET);
		return (a[0] == PMP_LIVE_CANARY && a[1] == PMP_LIVE_CODE ? 1UL : 0UL) |
			(b[0] == PMP_LIVE_CANARY + 1 && b[1] == PMP_LIVE_CODE ? 2UL : 0UL);
	}
	default: return audit[vm][offset / 8UL];
	}
}

int32_t pmp_live_begin(struct acrn_vcpu *vcpu, uint64_t command)
{
	uint16_t vm = vcpu->vm->vm_id;
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	uint64_t op;
	/* Three fixed operations on the peer fixture, plus own-page controls.
	 * No Guest pointer, executable bytes, PMP policy or return PC is accepted.
	 */
	if (vm >= 2 || command < 1 || command > 6 || probes[vm].active ||
	    (ctx->cpu_gp_regs.regs.status & 0x1800UL)) return -1;
	/* Delegated pending S interrupts must not become host M interrupts in
	 * this transition window. vmx_vmrun keeps MIE clear until mret.
	 */
	cpu_disable_mirq();
	probes[vm].saved = *ctx;
	probes[vm].medeleg = cpu_csr_read(medeleg);
	probes[vm].mideleg = cpu_csr_read(mideleg);
	probes[vm].command = command;
	probes[vm].active = true;
	audit[vm][14] = 0;
	op = (command - 1) % 3;
	/* Route every exit back to M while the two-instruction probe is active,
	 * including Sstc interrupts. Linux never sees the bare probe context.
	 */
	cpu_csr_write(medeleg, 0);
	cpu_csr_write(mideleg, 0);
	ctx->satp = 0;
	ctx->cpu_gp_regs.regs.ip = static_vms[vm].base + PMP_LIVE_PAGE_OFFSET + 0x100 + op*8;
	ctx->cpu_gp_regs.regs.t1 = static_vms[command > 3 ? vm : 1U-vm].base + PMP_LIVE_PAGE_OFFSET + (op == 2 ? 8 : 0);
	ctx->cpu_gp_regs.regs.t2 = command > 3 ? PMP_LIVE_CANARY + vm : 0xa55aa55aa55aa55aUL;
	ctx->cpu_gp_regs.regs.status &= ~0x1800UL; /* real U-mode, same Guest PMP */
	return 0;
}

void pmp_live_early_restore(void)
{
	struct acrn_vcpu *vcpu = get_running_vcpu(get_pcpu_id());
	uint16_t vm;
	if (!vcpu) return;
	vm = vcpu->vm->vm_id;
	if (vm < 2 && probes[vm].active) {
		/* Called from vm_exit before MIE is re-enabled. Preserve the live
		 * probe CSRs until save_vmcs captures them; restore only delegation.
		 */
		cpu_csr_write(medeleg, probes[vm].medeleg);
		cpu_csr_write(mideleg, probes[vm].mideleg);
	}
}

bool pmp_live_finish(struct acrn_vcpu *vcpu)
{
	uint16_t vm = vcpu->vm->vm_id;
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	uint64_t cause = ctx->cpu_gp_regs.regs.cause, delegated;
	uint64_t expected;
	if (vm >= 2 || !probes[vm].active) return false;
	expected = probes[vm].command > 3 ? 3 : probes[vm].command == 1 ? 5 : probes[vm].command == 2 ? 7 : 1;
	delegated = probes[vm].mideleg;
	if (cause == expected && probes[vm].command > 3) {
		bool good = (probes[vm].command != 4 || ctx->cpu_gp_regs.regs.t0 == PMP_LIVE_CANARY + vm) &&
			(probes[vm].command != 6 || ctx->cpu_gp_regs.regs.a0 == 0x123UL);
		audit[vm][14] = good ? 5 : 2;
		if (good) probes[vm].positive_controls |= 1UL << (probes[vm].command - 4);
	} else if (cause == expected) {
		uint64_t va = cause == 1 ? ctx->cpu_gp_regs.regs.ip : ctx->cpu_gp_regs.regs.tval;
		pmp_live_record(vm, cause, va, va, ctx->cpu_gp_regs.regs.ip);
		audit[vm][14] = 1; /* real access fault captured */
		pr_warn("[PMP-LIVE] VM%u CPU_ACCESS_DENIED cause=%lx pa=%lx pc=%lx",
			vm, cause, va, ctx->cpu_gp_regs.regs.ip);
	} else {
		audit[vm][14] = (cause >> 63) ? 3 : 2;
		if (cause >> 63) audit[vm][16]++;
	}
	cpu_csr_write(medeleg, probes[vm].medeleg);
	cpu_csr_write(mideleg, probes[vm].mideleg);
	*ctx = probes[vm].saved;
	probes[vm].active = false;
	/* Original M interrupts must still be dispatched. A formerly delegated
	 * S interrupt remains pending and will enter Linux after mret.
	 */
	return !(cause >> 63) || (delegated & (1UL << (cause & 63UL)));
}

void pmp_live_record(uint16_t vm, uint64_t cause, uint64_t pa, uint64_t va, uint64_t pc)
{
	uint64_t own, size;
	bool private, shared;
	if (vm >= 2) return;
	own = static_vms[vm].base;
	size = static_vms[vm].size;
	private = pa >= own && pa - own < size;
	shared = pa >= STATIC_SHARED_BASE && pa - STATIC_SHARED_BASE < STATIC_SHARED_SIZE;
	audit[vm][5]++;
	audit[vm][6] = cause;
	audit[vm][7] = pa;
	audit[vm][8] = va;
	audit[vm][9] = pc;
	if (cause == 5) audit[vm][10]++;
	if (cause == 7) audit[vm][11]++;
	if (cause == 1) audit[vm][12]++;
	/* A fault on own RAM can instead be a PMA/cache-attribute problem.
	 * It is not evidence that a PMP rule rejected this access.
	 */
	audit[vm][13] = !private && (!shared || cause == 1);
}
#endif
