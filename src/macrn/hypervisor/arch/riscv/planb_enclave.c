/* SPDX-License-Identifier: BSD-3-Clause */
/* Fixed no-H profile: one CVM hart, four independent S/U PMP enclaves.
 * The stable Zion create/run/resume/stop/exit/destroy ABI is implemented here;
 * attestation and sealing are intentionally unimplemented, never simulated.
 */
#ifdef CONFIG_PLAN_B_ENCLAVE
#include <types.h>
#include <asm/lib/string.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/static_vm.h>
#include <asm/planb_enclave.h>
#include <asm/planb_io.h>
#include <asm/per_cpu.h>
#include <asm/timer.h>
#include <asm/cpu.h>
#include <ticks.h>
#include <logmsg.h>

#define ENCLAVE_EID 0x08424b45UL
#define SLOT_COUNT 4U
#define PAGE_BYTES 4096UL
#define MAX_EPM (16UL * 1024 * 1024)
#define MAX_UTM (16UL * 1024 * 1024)
#define VECTOR_BYTES 512U
#define STCE (1UL << 63)
#define INTERRUPTED 100002UL
#define INVALID_ID 100001UL
#define ILLEGAL_ARGUMENT 100008UL
#define NOT_ACCESSIBLE 100007UL
#define PROHIBITED 100014UL
#define NOT_IMPLEMENTED 100100UL

enum state { FREE, CLAIMING, FRESH, RUNNING, STOPPED, EXITED };
struct region { uint64_t base, size; };
struct create_args {
	struct region epm, utm;
	uint64_t runtime, user, free, free_requested;
};
_Static_assert(sizeof(struct create_args) == 64, "Zion create ABI");
struct extended {
	uint64_t fp[33];
	uint64_t vstart, vl, vtype, vcsr;
	uint8_t vectors[32 * VECTOR_BYTES] __aligned(16);
	uint64_t scounteren, senvcfg, pending;
};
struct enclave {
	unsigned int state;
	uint64_t handle;
	struct create_args args;
	struct run_context context;
	struct extended extra;
	uint64_t timer;
};
static struct enclave slots[SLOT_COUNT];
static uint64_t generations[SLOT_COUNT];
static struct enclave *active_enclave;
static struct {
	struct run_context context;
	struct extended extra;
	uint64_t medeleg, mideleg, mie, menvcfg, mtimecmp, stimecmp, deadline;
	bool prepared;
} parent;
static uint64_t vector_length;
static bool extensions_checked, has_fp, has_vector;

extern void planb_fp_save(uint64_t *registers);
extern void planb_fp_restore(const uint64_t *registers);
extern uint64_t planb_vector_length(void);
extern void planb_vector_save(struct extended *extra);
extern void planb_vector_restore(const struct extended *extra);
_Static_assert(__builtin_offsetof(struct extended, vstart) == 264, "vector CSR offsets");
_Static_assert(__builtin_offsetof(struct extended, vectors) == 304, "vector data offset");

bool planb_enclave_idle(void)
{
 unsigned int i;
 if (active_enclave) return false;
 for (i = 0; i < SLOT_COUNT; i++)
  if (__atomic_load_n(&slots[i].state, __ATOMIC_ACQUIRE) != FREE) return false;
 return true;
}

static bool inside(struct region r, uint64_t a, uint64_t n)
{
	return n && a >= r.base && a - r.base < r.size && n <= r.size - (a - r.base);
}
static bool overlaps(struct region r, uint64_t a, uint64_t n)
{
	return r.size && n && (a >= r.base ? a - r.base < r.size : r.base - a < n);
}
static bool owned(uint64_t a, uint64_t n)
{
	struct region ram = {static_vms[1].base, static_vms[1].size};
	return inside(ram, a, n);
}
static unsigned int state_of(const struct enclave *e)
{
	return __atomic_load_n(&e->state, __ATOMIC_ACQUIRE);
}

bool planb_enclave_parent_range(uint64_t a, uint64_t n)
{
	unsigned int i;
	if (!owned(a, n)) return false;
	for (i = 0; i < SLOT_COUNT; i++)
		if (state_of(&slots[i]) != FREE && overlaps(slots[i].args.epm, a, n)) return false;
	return true;
}
bool planb_enclave_active(const struct acrn_vcpu *v)
{
	return v && v->vm->vm_id == 1 && get_pcpu_id() == v->pcpu_id && active_enclave;
}
bool planb_enclave_range(uint16_t vm, uint64_t a, uint64_t n)
{
	if (vm != 1) return true;
	if (get_pcpu_id() == 1 && active_enclave)
		return inside(active_enclave->args.epm, a, n) || inside(active_enclave->args.utm, a, n);
	/* The ordinary CVM also retains its explicit outer shared channel. */
	if (!owned(a, n)) return true;
	return planb_enclave_parent_range(a, n);
}

static void fail_closed(const char *message)
{
	cpu_disable_mirq();
	pr_fatal("[PLANB ENCLAVE] %s", message);
	for (;;) asm volatile("wfi");
}
static void check_extensions(void)
{
	uint64_t isa;
	if (extensions_checked) return;
	isa = cpu_csr_read(misa);
	has_fp = !!(isa & (1UL << ('D' - 'A')));
	has_vector = !!(isa & (1UL << ('V' - 'A')));
	if (has_vector) {
		vector_length = planb_vector_length();
		if (!vector_length || vector_length > VECTOR_BYTES ||
		    (vector_length & (vector_length - 1)))
			fail_closed("unsupported standard RVV register width");
	}
	extensions_checked = true;
	pr_info("[PLANB ENCLAVE] CONTEXT_EXTENSIONS fp=%u rvv=%u vlenb=%lu",
		has_fp, has_vector, vector_length);
}
static void save_extended(struct extended *extra)
{
	if (has_fp) planb_fp_save(extra->fp);
	if (has_vector) planb_vector_save(extra);
	extra->scounteren = cpu_csr_read(scounteren);
	extra->senvcfg = cpu_csr_read(0x10a);
	extra->pending = cpu_csr_read(mip) & 0x222UL;
}
static void restore_extended(const struct extended *extra)
{
	if (has_fp) planb_fp_restore(extra->fp);
	if (has_vector) planb_vector_restore(extra);
	cpu_csr_write(scounteren, extra->scounteren);
	cpu_csr_write(0x10a, extra->senvcfg);
}

void planb_enclave_reset_idle(void)
{
 unsigned int i;
 struct extended *observed = &slots[0].extra;
 /* Only VM1's Hart may call this after checking every slot FREE. Keep the
  * generation counters for stale-handle rejection across Guest reloads. */
 if (!planb_enclave_idle()) fail_closed("runtime reset with allocated enclave");
 check_extensions();
 memset(slots, 0, sizeof(slots));
 memset(&parent, 0, sizeof(parent));
 parent.extra.vtype = 1UL << 63; /* architectural reset: vill, vl=0 */
 restore_extended(&parent.extra);
 save_extended(observed);
 for (i = 0; i < 33; i++)
  if (has_fp && observed->fp[i]) fail_closed("runtime FP erase readback failed");
 if (has_vector) {
  for (i = 0; i < 32 * vector_length; i++)
   if (observed->vectors[i]) fail_closed("runtime RVV erase readback failed");
  if (observed->vstart || observed->vl || observed->vcsr || observed->vtype != (1UL << 63))
   fail_closed("runtime RVV CSR erase readback failed");
 }
 memset(slots, 0, sizeof(slots));
 memset(&parent, 0, sizeof(parent));
}

/* A complete bounded Sv39/Sv48 walk of the caller's VA. No page table or
 * destination in shared/peer/monitor/protected EPM memory is ever followed.
 */
static bool parent_translate(struct run_context *ctx, uint64_t va, uint64_t *pa)
{
	uint64_t satp = ctx->satp, mode = satp >> 60, table, pte, mask;
	int level, levels = mode == 8 ? 3 : mode == 9 ? 4 : 0;
	if (!mode) { *pa = va; return planb_enclave_parent_range(va, 1); }
	if (!levels || (int64_t)va >> (12 + levels * 9 - 1) !=
	    ((va >> (12 + levels * 9 - 1)) & 1 ? -1L : 0L)) return false;
	table = (satp & ((1UL << 44) - 1)) << 12;
	for (level = levels - 1; level >= 0; level--) {
		uint64_t index = (va >> (12 + level * 9)) & 511;
		if (!planb_enclave_parent_range(table, PAGE_BYTES)) return false;
		pte = __atomic_load_n((uint64_t *)(table + index * 8), __ATOMIC_ACQUIRE);
		if (!(pte & 1) || ((pte & 6) == 4) || (pte >> 54)) return false;
		table = ((pte >> 10) & ((1UL << 44) - 1)) << 12;
		if (pte & 10) {
			if (!(pte & 2) || !(pte & 64) || ((pte & 16) && !(ctx->sstatus & (1UL << 18))))
				return false;
			mask = (1UL << (12 + level * 9)) - 1;
			if (table & mask) return false;
			*pa = table | (va & mask);
			return planb_enclave_parent_range(*pa, 1);
		}
		if (pte & (16 | 64 | 128)) return false;
	}
	return false;
}
static bool copy_create(struct run_context *ctx, uint64_t va, struct create_args *args)
{
	uint8_t *target = (uint8_t *)args;
	uint64_t pa, n, remaining = sizeof(*args);
	if (va + remaining < va) return false;
	while (remaining) {
		if (!parent_translate(ctx, va, &pa)) return false;
		n = PAGE_BYTES - (pa & (PAGE_BYTES - 1));
		if (n > remaining) n = remaining;
		if (!planb_enclave_parent_range(pa, n)) return false;
		memcpy(target, (const void *)pa, n);
		target += n; va += n; remaining -= n;
	}
	return true;
}
static bool valid_args(const struct create_args *a)
{
	return a->epm.size >= 2 * PAGE_BYTES && a->epm.size <= MAX_EPM &&
		!(a->epm.size & (a->epm.size - 1)) && !(a->epm.base & (a->epm.size - 1)) &&
		owned(a->epm.base, a->epm.size) && a->utm.size && a->utm.size <= MAX_UTM &&
		!((a->utm.base | a->utm.size | a->runtime | a->user | a->free |
		   a->free_requested) & (PAGE_BYTES - 1)) && owned(a->utm.base, a->utm.size) &&
		!overlaps(a->epm, a->utm.base, a->utm.size) &&
		inside(a->epm, a->runtime, 1) && a->runtime >= a->epm.base + PAGE_BYTES &&
		a->user >= a->runtime && inside(a->epm, a->user, 1) && a->free >= a->user &&
		inside(a->epm, a->free, a->free_requested ? a->free_requested : 1);
}
static uint64_t create(struct run_context *ctx, uint64_t va, uint64_t *handle)
{
	struct create_args a;
	struct enclave *e = NULL;
	unsigned int i, index = 0;
	uint64_t flags;
	if (!copy_create(ctx, va, &a) || !valid_args(&a)) return ILLEGAL_ARGUMENT;
	/* Serialize reservation with every monitor-side virtqueue snapshot and
 * completion. Mark protected before releasing the lock; all later DMA
 * proxies reject the EPM. The parent hart cannot run during this call.
 */
	planb_io_acquire(&flags);
	for (i = 0; i < SLOT_COUNT; i++) {
		if (state_of(&slots[i]) == FREE) { e = &slots[i]; index = i; continue; }
		if (overlaps(slots[i].args.epm, a.epm.base, a.epm.size) ||
		    overlaps(slots[i].args.utm, a.epm.base, a.epm.size) ||
		    overlaps(slots[i].args.epm, a.utm.base, a.utm.size)) {
			planb_io_release(flags); return 100006;
		}
	}
	if (!e || generations[index] >= (1UL << 55) - 1) {
		planb_io_release(flags); return 100013;
	}
	if (planb_io_busy_range(a.epm.base, a.epm.size)) {
		planb_io_release(flags); return NOT_ACCESSIBLE;
	}
	memset(e, 0, sizeof(*e));
	e->args = a;
	e->handle = (++generations[index] << 8) | (index + 1);
	__atomic_store_n(&e->state, CLAIMING, __ATOMIC_RELEASE);
	planb_io_release(flags);
	e->context.cpu_gp_regs.regs.ip = a.epm.base; /* Actual SDK loader.bin. */
	e->context.cpu_gp_regs.regs.sp = a.free + a.free_requested;
	e->context.cpu_gp_regs.regs.status = 0x200002a20UL; /* RV64 S, FS/VS initial, MPIE. */
	e->context.sstatus = 0x200002200UL;
	e->context.cpu_gp_regs.regs.a1 = a.epm.base;
	e->context.cpu_gp_regs.regs.a2 = a.epm.size;
	e->context.cpu_gp_regs.regs.a3 = a.runtime;
	e->context.cpu_gp_regs.regs.a4 = a.user;
	e->context.cpu_gp_regs.regs.a5 = a.free;
	e->context.cpu_gp_regs.regs.a6 = a.utm.base;
	e->context.cpu_gp_regs.regs.a7 = a.utm.size;
	e->extra.scounteren = 7;
	e->extra.vtype = 1UL << 63; /* Architectural reset vtype=VILL, VL=0. */
	e->timer = UINT64_MAX;
	__atomic_store_n(&e->state, FRESH, __ATOMIC_RELEASE);
	*handle = e->handle;
	pr_info("[PLANB ENCLAVE] CREATE handle=%lx epm=%lx+%lx utm=%lx+%lx",
		e->handle, a.epm.base, a.epm.size, a.utm.base, a.utm.size);
	return 0;
}
static struct enclave *lookup(uint64_t handle)
{
	unsigned int n = handle & 255;
	if (!n || n > SLOT_COUNT || slots[n - 1].handle != handle ||
	    state_of(&slots[n - 1]) == FREE) return NULL;
	return &slots[n - 1];
}

void planb_enclave_prepare_entry(struct acrn_vcpu *v)
{
	volatile uint64_t *compare;
	uint64_t deadline;
	if (!planb_enclave_active(v)) return;
	check_extensions();
	compare = (void *)CLINT_MTIMECMP(get_pcpu_id());
	save_extended(&parent.extra);
	parent.medeleg = cpu_csr_read(medeleg);
	parent.mideleg = cpu_csr_read(mideleg);
	parent.mie = cpu_csr_read(mie);
	parent.menvcfg = cpu_csr_read(0x30a);
	parent.mtimecmp = *compare;
	if (parent.menvcfg & STCE) {
		parent.stimecmp = cpu_csr_read(0x14d);
		cpu_csr_write(0x14d, UINT64_MAX);
		cpu_csr_clear(0x30a, STCE);
	}
	cpu_csr_clear(mip, 0x222UL);
	/* U ecall, ordinary page faults, breakpoints and misalignment belong to
 * Eyrie. Access faults and illegal instructions must reach this monitor.
 */
	cpu_csr_write(medeleg, (1UL << 8) | (1UL << 12) | (1UL << 13) |
		(1UL << 15) | (1UL << 3) | (1UL << 0) | (1UL << 4) | (1UL << 6));
	cpu_csr_write(mideleg, 1UL << 5); /* Only the enclave's virtual S timer. */
	cpu_csr_write(mie, 0x888UL);
	deadline = parent.deadline;
	if (active_enclave->timer < deadline) deadline = active_enclave->timer;
	*compare = deadline;
	restore_extended(&active_enclave->extra);
	parent.prepared = true;
}

/* Called by vm_exit while MIE is still clear. Keep it clear through C
 * dispatch: raw parent IRQ handlers must never modify the enclave's frame.
 */
bool planb_enclave_early_restore(void)
{
	if (get_pcpu_id() != 1 || !active_enclave || !parent.prepared) return false;
	save_extended(&active_enclave->extra);
	cpu_csr_write(medeleg, parent.medeleg);
	cpu_csr_write(mideleg, parent.mideleg);
	if (parent.menvcfg & STCE) {
		cpu_csr_write(0x30a, parent.menvcfg);
		cpu_csr_write(0x14d, parent.stimecmp);
	} else cpu_csr_write(0x30a, parent.menvcfg);
	*(volatile uint64_t *)CLINT_MTIMECMP(get_pcpu_id()) = parent.mtimecmp;
	cpu_csr_write(mie, parent.mie);
	cpu_csr_clear(mip, 0x222UL);
	cpu_csr_set(mip, parent.extra.pending);
	restore_extended(&parent.extra);
	parent.prepared = false;
	return true;
}
static void leave(struct acrn_vcpu *v, uint64_t error, uint64_t value, bool exited)
{
	struct run_context *ctx = &v->arch.contexts[0].run_ctx;
	active_enclave->context = *ctx;
	__atomic_store_n(&active_enclave->state, exited ? EXITED : STOPPED, __ATOMIC_RELEASE);
	*ctx = parent.context;
	ctx->cpu_gp_regs.regs.a0 = error;
	ctx->cpu_gp_regs.regs.a1 = value;
	active_enclave = NULL;
	/* The frame and CSRs now both belong to the parent; pending physical
 * interrupts may safely run their ordinary monitor/virtual IRQ handlers.
 */
	cpu_enable_mirq();
}
bool planb_enclave_finish_exit(struct acrn_vcpu *v)
{
	struct run_context *ctx = &v->arch.contexts[0].run_ctx;
	uint64_t cause = ctx->cpu_gp_regs.regs.cause, now;
	if (!planb_enclave_active(v)) return false;
	/* save_vmcs saw the restored parent pending levels; retain the actual
 * inner levels captured before restoring the parent interrupt environment.
 */
	ctx->sip = active_enclave->extra.pending;
	if (cause >> 63) {
		now = get_tick();
		if (now >= active_enclave->timer) {
			ctx->sip |= 1UL << 5;
			active_enclave->timer = UINT64_MAX;
		}
		leave(v, INTERRUPTED, (cause & 63) == 7 ? 2 :
			(cause & 63) == 3 ? 3 : 4, false);
		return true;
	}
	if (cause != 9) {
		pr_warn("[PLANB ENCLAVE] ACCESS_ABORT cause=%lx pc=%lx tval=%lx",
			cause, ctx->cpu_gp_regs.regs.ip, ctx->cpu_gp_regs.regs.tval);
		leave(v, NOT_ACCESSIBLE, cause, true);
		return true;
	}
	return false;
}

bool planb_enclave_sbi(struct acrn_vcpu *v)
{
	struct run_context *ctx = &v->arch.contexts[0].run_ctx;
	struct cpu_regs *r = &ctx->cpu_gp_regs.regs;
	struct enclave *e;
	uint64_t id = r->a7, fid = r->a6, argument = r->a0, value = 0, error = 0, flags;
	if (planb_enclave_active(v)) {
		/* Console output remains bounded by its normal physical-range gate. */
		if (id == 1 || id == 0x4442434eUL) return false;
		if (id == 0 || (id == 0x54494d45UL && !fid)) {
			active_enclave->timer = argument; ctx->sip &= ~(1UL << 5);
			r->a0 = r->a1 = 0; return true;
		}
		if (id != ENCLAVE_EID) { r->a0 = -2UL; r->a1 = 0; return true; }
		if (fid == 3004) {
			if (argument > 1) { r->a0 = ILLEGAL_ARGUMENT; r->a1 = 0; return true; }
			r->a0 = r->a1 = 0;
			leave(v, argument == 1 ? 100011 : INTERRUPTED, argument == 1 ? 0 : 1, false);
			return true;
		}
		if (fid == 3006) {
			r->a0 = r->a1 = 0;
			pr_info("[PLANB ENCLAVE] EXIT handle=%lx retval=%lx", active_enclave->handle, argument);
			leave(v, 0, argument, true); return true;
		}
		if (fid == 3008) { r->a0 = 0; r->a1 = (uint64_t)cpu_tickrate() * 1000; return true; }
		r->a0 = fid < 3000 || fid >= 4000 ? PROHIBITED : NOT_IMPLEMENTED;
		r->a1 = 0; return true;
	}
	if (id != ENCLAVE_EID) return false;
	if (v->vm->vm_id != 1 || v->pcpu_id != 1 || get_pcpu_id() != 1) {
		r->a0 = PROHIBITED; r->a1 = 0; return true;
	}
	cpu_disable_mirq();
	if (fid == 2001) error = create(ctx, argument, &value);
	else {
		e = lookup(argument);
		if (!e) error = INVALID_ID;
		else if (fid == 2002) {
			if (state_of(e) == RUNNING || state_of(e) == CLAIMING) error = 100005;
			else {
				/* Zero before releasing the PMP reservation, including all old
 * runtime/app keys and stacks. Existing parent mappings then see zeros. */
				memset((void *)e->args.epm.base, 0, e->args.epm.size);
				asm volatile("fence rw,rw" ::: "memory");
				planb_io_acquire(&flags);
				__atomic_store_n(&e->state, FREE, __ATOMIC_RELEASE);
				planb_io_release(flags);
				pr_info("[PLANB ENCLAVE] DESTROY handle=%lx EPM_ZEROED", argument);
			}
		} else if (fid == 2003 || fid == 2005) {
			if (state_of(e) != (fid == 2003 ? FRESH : STOPPED)) error = fid == 2003 ? 100004 : 100010;
			else {
				parent.context = *ctx;
				parent.deadline = get_tick() + (uint64_t)cpu_tickrate() * 10;
				*ctx = e->context;
				__atomic_store_n(&e->state, RUNNING, __ATOMIC_RELEASE);
				active_enclave = e;
				return true;
			}
		} else error = NOT_IMPLEMENTED;
	}
	r->a0 = error; r->a1 = value;
	cpu_enable_mirq();
	return true;
}

static uint64_t napot(struct region r) { return (r.base >> 2) | ((r.size >> 3) - 1); }
bool planb_enclave_load_pmp(struct acrn_vcpu *v)
{
	uint64_t address[8] = {0}, cfg = 0;
	unsigned int i;
	bool any = false;
	if (v->vm->vm_id != 1) return false;
	if (active_enclave) {
		address[0] = napot(active_enclave->args.epm);
		address[1] = active_enclave->args.utm.base >> 2;
		address[2] = (active_enclave->args.utm.base + active_enclave->args.utm.size) >> 2;
		cfg = 0x0b001fUL; /* EPM RWX, exact UTM RW, unmatched denies all else. */
		any = true;
	} else {
		for (i = 0; i < SLOT_COUNT; i++) if (state_of(&slots[i]) != FREE) {
			address[i] = napot(slots[i].args.epm);
			cfg |= 0x18UL << (8 * i); /* Higher priority EPM denial. */
			any = true;
		}
		address[4] = static_vms[1].base >> 2;
		address[5] = (static_vms[1].base + static_vms[1].size) >> 2;
		address[6] = napot((struct region){STATIC_SHARED_BASE, STATIC_SHARED_SIZE});
		cfg |= 0x001b0f0000000000UL;
	}
	if (!any) return false;
	if (cpu_csr_read(pmpcfg0) & 0x8080808080808080UL) fail_closed("locked PMP");
	cpu_csr_write(pmpcfg0, 0);
#define WRITE_PMP(n) cpu_csr_write(pmpaddr##n, address[n])
	WRITE_PMP(0); WRITE_PMP(1); WRITE_PMP(2); WRITE_PMP(3);
	WRITE_PMP(4); WRITE_PMP(5); WRITE_PMP(6); WRITE_PMP(7);
	cpu_csr_write(pmpcfg0, cfg);
	asm volatile("sfence.vma\n\tfence.i" ::: "memory");
#define CHECK_PMP(n) if (cpu_csr_read(pmpaddr##n) != address[n]) fail_closed("PMP address readback")
	CHECK_PMP(0); CHECK_PMP(1); CHECK_PMP(2); CHECK_PMP(3);
	CHECK_PMP(4); CHECK_PMP(5); CHECK_PMP(6); CHECK_PMP(7);
	if (cpu_csr_read(pmpcfg0) != cfg) fail_closed("PMP permissions readback");
	return true;
}
#endif
