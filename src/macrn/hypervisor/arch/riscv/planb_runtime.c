/* SPDX-License-Identifier: BSD-3-Clause */
/* Two domains, one Host boot. Only the target Hart acknowledges a stop. */
#ifdef CONFIG_PLAN_B_RUNTIME
#include <types.h>
#include <errno.h>
#include <asm/lib/string.h>
#include <asm/guest/vm.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vmcs.h>
#include <asm/guest/vplic.h>
#include <asm/guest/vuart.h>
#include <asm/guest/virq.h>
#include <asm/static_vm.h>
#include <asm/planb_runtime.h>
#include <asm/planb_enclave.h>
#include <asm/planb_io.h>
#include <asm/pmp_live.h>
#include <asm/per_cpu.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/timer.h>
#include <logmsg.h>
#include <io_req.h>

static struct acrn_vcpu *target;
static spinlock_t control_lock;
static uint32_t state = PLANB_RT_RUNNING, image, requested_image, last_error, clean;
static uint64_t stops, boot_mstatus, boot_mideleg, boot_menvcfg, boot_scounteren, boot_senvcfg;
static bool captured;
/* Test state survives Guest reloads and driver unloads, until Host reboot. */
static uint32_t tamper_armed, tamper_used, tamper_error;

bool planb_runtime_block_enabled(void)
{
 return static_vm_initial_image() != PLANB_IMAGE_ENCLAVE;
}

void planb_runtime_tamper_detected(void)
{
 __atomic_store_n(&tamper_used, 1U, __ATOMIC_RELEASE);
 __atomic_store_n(&tamper_armed, 0U, __ATOMIC_RELEASE);
}

bool planb_runtime_range(uint16_t vm, uint64_t address, uint64_t size)
{
 return vm == 0 && size && address >= PLANB_RUNTIME_MMIO &&
  address - PLANB_RUNTIME_MMIO < 4096 && size <= 4096 - (address - PLANB_RUNTIME_MMIO);
}
static void publish(uint32_t next)
{
 __atomic_store_n(&state, next, __ATOMIC_RELEASE);
}
static int32_t access_control(struct io_request *req, __unused void *context)
{
 struct acrn_mmio_request *m = &req->reqs.mmio_request;
 uint32_t offset = (uint32_t)(m->address - PLANB_RUNTIME_MMIO), s;
 uint64_t flags, epoch;
 if (m->size != 4 || (offset & 3)) return -EINVAL;
 spin_lock_irqsave(&control_lock, &flags);
 s = __atomic_load_n(&state, __ATOMIC_ACQUIRE);
 if (m->direction == ACRN_IOREQ_DIR_WRITE) {
  if (offset == 52) {
   tamper_error = 0;
   if (m->value > 1) tamper_error = EINVAL;
   else if (m->value && __atomic_load_n(&tamper_used, __ATOMIC_ACQUIRE)) tamper_error = EBUSY;
   else {
    __atomic_store_n(&tamper_armed, (uint32_t)m->value, __ATOMIC_RELEASE);
    if (m->value && __atomic_load_n(&tamper_used, __ATOMIC_ACQUIRE)) {
     __atomic_store_n(&tamper_armed, 0U, __ATOMIC_RELEASE);
     tamper_error = EBUSY;
    }
   }
  } else if (offset != 8 || m->value < PLANB_RT_STOP || m->value > PLANB_RT_START_ENCLAVE)
   last_error = EINVAL;
  else if (!target) last_error = ENODEV;
  else if (m->value == PLANB_RT_STOP && s == PLANB_RT_STOPPED) last_error = 0;
  else if (m->value == PLANB_RT_STOP && s == PLANB_RT_RUNNING) {
   last_error = 0;
   publish(PLANB_RT_STOPPING);
   kick_vcpu(target);
  } else if (m->value != PLANB_RT_STOP && s == PLANB_RT_STOPPED) {
   requested_image = (uint32_t)m->value - 1U;
   last_error = 0;
   publish(PLANB_RT_LOADING);
   kick_vcpu(target);
  } else last_error = EBUSY;
 } else {
  epoch = static_vm_guest_epoch();
  switch (offset) {
  case 0: m->value = PLANB_RUNTIME_MAGIC; break;
  case 4: m->value = PLANB_RUNTIME_ABI; break;
  case 12: m->value = s; break;
  case 16: m->value = image; break;
  case 20: m->value = __atomic_load_n(&last_error, __ATOMIC_ACQUIRE); break;
  case 24: m->value = (uint32_t)epoch; break;
  case 28: m->value = epoch >> 32; break;
  case 32: m->value = clean; break;
  case 36: m->value = (uint32_t)stops; break;
  case 40: m->value = (uint32_t)static_vms[1].size; break;
  case 44: m->value = static_vms[1].size >> 32; break;
  case 48: m->value = PLANB_NATIVE_COMPAT_MAGIC; break;
  case 52: m->value = __atomic_load_n(&tamper_armed, __ATOMIC_ACQUIRE); break;
  case 56: m->value = tamper_error; break;
  case 60: m->value = __atomic_load_n(&tamper_used, __ATOMIC_ACQUIRE); break;
  default: m->value = 0; break;
  }
 }
 spin_unlock_irqrestore(&control_lock, flags);
 return 0;
}
void planb_runtime_init(struct acrn_vm *vm)
{
 if (vm->vm_id == 0) {
  spinlock_init(&control_lock);
  image = static_vm_initial_image();
  register_mmio_emulation_handler(vm, access_control, PLANB_RUNTIME_MMIO,
   PLANB_RUNTIME_MMIO + 4096, NULL, false);
 } else if (vm->vm_id == 1) target = &vm->hw.vcpu[0];
}
static void reset_cpu(struct acrn_vcpu *v)
{
 unsigned int i;
 /* Retain the scheduler's stack, ownership, CPU facts and device bindings. */
 memset(v->arch.contexts, 0, sizeof(v->arch.contexts));
 memset(v->arch.guest_csrs, 0, sizeof(v->arch.guest_csrs));
 memset(v->arch.csr_bitmap, 0, sizeof(v->arch.csr_bitmap));
 memset(&v->arch.csr_area, 0, sizeof(v->arch.csr_area));
 memset(v->arch.eoi_exit_bitmap, 0, sizeof(v->arch.eoi_exit_bitmap));
 v->arch.cur_context = NORMAL_WORLD;
 v->arch.exception_info.exception = VECTOR_INVALID;
 v->arch.exception_info.error = 0;
 v->arch.pending_req = 0;
 v->arch.exit_reason = v->arch.exit_qualification = 0;
 v->arch.inst_len = v->arch.nrexits = 0;
 v->arch.nr_sipi = v->arch.lapic_mask = 0;
 v->reg_cached = v->reg_updated = 0;
 v->launched = false;
 memset(&v->req, 0, sizeof(v->req));
 memset(&v->mpxy, 0, sizeof(v->mpxy));
 for (i = 0; i < VCPU_EVENT_NUM; ++i) reset_event(&v->events[i]);
 planb_enclave_reset_idle();
 cpu_csr_write(scounteren, boot_scounteren);
 cpu_csr_write(0x10a, boot_senvcfg);
 cpu_csr_write(mideleg, boot_mideleg);
 cpu_csr_write(0x30a, boot_menvcfg);
 if (boot_menvcfg & (1UL << 63)) cpu_csr_write(0x14d, UINT64_MAX);
 cpu_csr_write(sie, 0);
 cpu_csr_write(stvec, 0);
 cpu_csr_write(sscratch, 0);
 cpu_csr_write(sepc, 0);
 cpu_csr_write(scause, 0);
 cpu_csr_write(stval, 0);
 cpu_csr_write(satp, 0);
 cpu_csr_clear(mip, 0x222UL);
 cpu_csr_write(mstatus, boot_mstatus & ~8UL);
 asm volatile("sfence.vma; fence.i" ::: "memory");
}
bool planb_runtime_entry(struct acrn_vcpu *v)
{
 uint32_t s;
 uint64_t flags;
 if (v->vm->vm_id != 1) return true;
 cpu_disable_mirq();
 if (!captured) {
  boot_mstatus = cpu_csr_read(mstatus);
  boot_mideleg = cpu_csr_read(mideleg);
  boot_menvcfg = cpu_csr_read(0x30a);
  boot_scounteren = cpu_csr_read(scounteren);
  boot_senvcfg = cpu_csr_read(0x10a);
  captured = true;
 }
 s = __atomic_load_n(&state, __ATOMIC_ACQUIRE);
 if (s == PLANB_RT_STOPPING) {
  /* This runs after VM-exit dispatch, never on the management Hart. An
   * EXITED/STOPPED enclave still owns EPM until a successful destroy. */
  if (!planb_enclave_idle()
#ifdef CONFIG_STATIC_LIVE_TEST
   || !pmp_live_idle(1)
#endif
  ) {
   __atomic_store_n(&last_error, EBUSY, __ATOMIC_RELEASE);
   publish(PLANB_RT_RUNNING);
   pr_warn("[PLANB RUNTIME] STOP_REFUSED allocated enclave or active probe");
   return true;
  }
  planb_io_quiesce();
  vclint_reset(&v->vm->vclint, vclint_ops, INIT_RESET);
  mmio_writeq(CLINT_DISABLE_TIMER, (void *)CLINT_MTIMECMP(v->pcpu_id));
  reset_vuarts(v->vm);
  planb_guest_console_reset(1);
  spin_lock_irqsave(&v->vm->vplic.lock, &flags);
  memset(&v->vm->vplic.regs, 0, sizeof(v->vm->vplic.regs));
  spin_unlock_irqrestore(&v->vm->vplic.lock, flags);
#ifdef CONFIG_STATIC_LIVE_TEST
  pmp_live_reset_idle(1);
#endif
  reset_cpu(v);
  clean = 14U; /* CPU, I/O, interrupt/timer/console state reset. */
  if (!static_vm_clear_guest()) {
   __atomic_store_n(&last_error, EIO, __ATOMIC_RELEASE);
   publish(PLANB_RT_FAILED);
   return false;
  }
  clean |= 1U;
  /* Guest timers were removed, but the per-CPU list can still contain
   * monitor scheduler timers. Disabling mtimecmp without rearming those
   * leaves a new SBI timer behind an expired list head, so add_timer()
   * does not program it and Linux waits forever for its first tick. */
  update_physical_timer(&per_cpu(cpu_timers, v->pcpu_id));
  ++stops;
  image = 0;
  publish(PLANB_RT_STOPPED);
  pr_info("[PLANB RUNTIME] STOPPED_ACK hart=%u clean=0x%x bytes=%lu stops=%lu",
   v->pcpu_id, clean, static_vms[1].size, stops);
  return false;
 }
 if (s == PLANB_RT_LOADING) {
  static_vm_reload_guest(requested_image);
  static_vm_set_boot_info(v->vm);
  vcpu_set_status(v, 0x2000C2000UL);
  vcpu_set_rip(v, static_vms[1].entry);
  vcpu_set_gpreg(v, CPU_REG_A0, 0);
  vcpu_set_gpreg(v, CPU_REG_A1, static_vms[1].dtb);
  init_vmcs(v);
  planb_io_online();
  image = requested_image;
  publish(PLANB_RT_RUNNING);
  pr_info("[PLANB RUNTIME] STARTED hart=%u image=%u epoch=%lu clean=0x%x",
   v->pcpu_id, image, static_vm_guest_epoch(), clean);
  return true;
 }
 if (s == PLANB_RT_RUNNING) return true;
 /* MIE remains clear from the state check through WFI: a pending enabled
  * MSIP wakes WFI even with global MIE clear. Service it before the loop. */
 asm volatile("wfi" ::: "memory");
 cpu_enable_mirq();
 return false;
}
#endif
