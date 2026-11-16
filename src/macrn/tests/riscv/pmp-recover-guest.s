/* SPDX-License-Identifier: BSD-3-Clause */
/* Actual S/U-mode PMP faults, recovered by a Guest exception handler. */
#include <asm/static_vm.h>
#ifndef CONFIG_TIMEBASE_HZ
#define CONFIG_TIMEBASE_HZ 10000000
#endif
#ifndef STATIC_PROBE_VM
#define STATIC_PROBE_VM 0
#endif
#ifndef PMP_PROBE_KIND
#define PMP_PROBE_KIND 5
#endif
#ifndef PMP_PROBE_VECTORED
#define PMP_PROBE_VECTORED 0
#endif
#ifndef PMP_PROBE_SIE
#define PMP_PROBE_SIE 1
#endif
#ifndef PMP_PROBE_USER
#define PMP_PROBE_USER 0
#endif
#if STATIC_GUEST_ID == 0
#define OWN_BASE STATIC_VM0_BASE
#define PEER_BASE STATIC_VM1_BASE
#else
#define OWN_BASE STATIC_VM1_BASE
#define PEER_BASE STATIC_VM0_BASE
#endif
#ifndef PMP_TARGET_ADDR
#if PMP_PROBE_KIND == 1
#define PMP_TARGET_ADDR (PEER_BASE + 0x80008)
#else
#define PMP_TARGET_ADDR (PEER_BASE + 0x80000)
#endif
#endif

	.option norvc
	.section .text,"ax",@progbits
	.global _start
_start:
	csrci sstatus, 2
	csrw sie, zero
	la sp, stack_top
	la t0, failed
	csrw stvec, t0
	beqz a1, failed
#ifdef PMP_PROBE_PAGING
	/* Sv39 identity mapping; even a permissive Guest PTE cannot bypass PMP. */
	li t0, (OWN_BASE + 0x90000)
	li t1, 0xcf
	li t2, 512
	li t3, 0x10000000
map_page:
	sd t1, 0(t0)
	add t1, t1, t3
	addi t0, t0, 8
	addi t2, t2, -1
	bnez t2, map_page
	li t0, (0x8000000000000000 | ((OWN_BASE + 0x90000) >> 12))
	csrw satp, t0
	sfence.vma
#endif
	la s0, entered
	call puts
	/* Private canary is never put into the shared communication page. */
	li t0, (OWN_BASE + 0x80000)
	li t1, (0x1122334455667788 + STATIC_GUEST_ID)
	sd t1, 0(t0)
	/* Valid code exists at execute targets, so an allowed fetch is a fail. */
	li t1, 0x0000806706600f93
	sd t1, 8(t0)
	fence.i
	li a7, 0x10
	li a6, 3
	li a0, 0x4d505859
	ecall
	bnez a0, failed
	bnez a1, failed
	li a7, 0x4d505859
	li a6, 1
	li a0, 0x90000000
	ecall
	li t0, -2
	bne a0, t0, failed
	li a7, 0x48534d
	li a6, 2
	li a0, -1
	ecall
	li t0, -3
	bne a0, t0, failed
	la s0, sbi_ok
	call puts
	li t0, STATIC_SHARED_BASE
	li t1, 1
	sd t1, (STATIC_GUEST_ID * 8)(t0)
	fence rw, rw
wait_ready:
	ld t1, ((1 - STATIC_GUEST_ID) * 8)(t0)
	beqz t1, wait_ready
	la s0, shared_ok
	call puts
	la t0, timer_trap
	csrw stvec, t0
	li s4, 3
	call arm_timer
	li t0, 32
	csrs sie, t0
	csrsi sstatus, 2
park:
	wfi
	j park

arm_timer:
	csrr a0, time
	li t0, (CONFIG_TIMEBASE_HZ / 50)
	add a0, a0, t0
	li a7, 0x54494d45
	li a6, 0
	ecall
	bnez a0, failed
	ret

	.balign 4
timer_trap:
	csrr t0, scause
	li t1, 0x8000000000000005
	bne t0, t1, failed
	addi s4, s4, -1
	beqz s4, timer_done
	call arm_timer
	sret
timer_done:
	li t0, 32
	csrc sie, t0
	la t0, after_timer
	csrw sepc, t0
	sret
after_timer:
	la s0, timer_ok
	call puts
#if STATIC_GUEST_ID == STATIC_PROBE_VM
	la s0, probe_begin
	call puts
	li s3, 0
	li s10, 0
next_probe:
	la t0, access_trap
	ori t0, t0, PMP_PROBE_VECTORED
	csrw stvec, t0
	li s6, PMP_TARGET_ADDR
	li s9, PMP_PROBE_KIND
	la s7, recovered
#if PMP_PROBE_KIND == 1
	mv s8, s6
#else
	la s8, probe_instruction
#endif
	li s5, 0x55aa55aa55aa55aa
	mv s1, s6
	mv s0, s5
#if PMP_PROBE_USER
	/* Fault in U-mode; the handler checks SPP=0 then returns to S. */
	li t0, 256
	csrc sstatus, t0
#if PMP_PROBE_SIE
	li t0, 32
	csrs sstatus, t0
#else
	li t0, 32
	csrc sstatus, t0
#endif
	la t0, probe_instruction
	csrw sepc, t0
	sret
#else
#if PMP_PROBE_SIE
	csrsi sstatus, 2
#else
	csrci sstatus, 2
#endif
#endif
	.global probe_instruction
probe_instruction:
#ifdef PMP_PROBE_COMPRESSED
	.option push
	.option rvc
#if PMP_PROBE_KIND == 5
	c.ld s0, 0(s1)
#elif PMP_PROBE_KIND == 7
	c.sd s0, 0(s1)
#else
	c.jr s1
#endif
	/* stvec BASE must stay four-byte aligned after a two-byte probe. */
	c.nop
	.option pop
#else
#if PMP_PROBE_KIND == 5
	ld s5, 0(s6)
#elif PMP_PROBE_KIND == 7
	sd s5, 0(s6)
#else
	jalr ra, 0(s6)
#endif
#endif
	j failed

	.balign 4
access_trap:
	csrr t0, scause
	bne t0, s9, failed
	csrr t0, stval
	bne t0, s6, failed
	csrr t0, sepc
	bne t0, s8, failed
	csrr t0, sstatus
	li t1, 0x122
	and t0, t0, t1
	li t1, ((1 - PMP_PROBE_USER) * 256 + PMP_PROBE_SIE * 32)
	bne t0, t1, failed
	addi s10, s10, 1
	/* A fixup belongs to the Guest. The monitor never advances sepc. */
	csrw sepc, s7
	li t0, 256
	csrs sstatus, t0
	li a0, -14
	sret
recovered:
	li t0, -14
	bne a0, t0, failed
	li t0, 0x55aa55aa55aa55aa
	bne s5, t0, failed
#ifdef PMP_PROBE_COMPRESSED
	bne s0, t0, failed
#endif
	addi s3, s3, 1
	bne s10, s3, failed
	la s0, recovery_ok
	call puts
	li t0, 3
	bltu s3, t0, next_probe
	li t0, STATIC_SHARED_BASE
	li t1, 1
	sd t1, (16 + STATIC_GUEST_ID * 8)(t0)
	fence rw, rw
#else
	li t0, STATIC_SHARED_BASE
wait_probe:
	ld t1, (16 + STATIC_PROBE_VM * 8)(t0)
	beqz t1, wait_probe
#endif
	/* The victim checks its own protected data after the denied writes. */
	li t0, (OWN_BASE + 0x80000)
	ld t1, 0(t0)
	li t2, (0x1122334455667788 + STATIC_GUEST_ID)
	bne t1, t2, failed
	ld t1, 8(t0)
	li t2, 0x0000806706600f93
	bne t1, t2, failed
	la s0, canary_ok
	call puts
	li t0, STATIC_SHARED_BASE
	li t1, (0x12340000 + STATIC_GUEST_ID)
	sd t1, (32 + STATIC_GUEST_ID * 8)(t0)
	fence rw, rw
wait_exchange:
	ld t1, (32 + (1 - STATIC_GUEST_ID) * 8)(t0)
	li t2, (0x12340000 + 1 - STATIC_GUEST_ID)
	bne t1, t2, wait_exchange
	la s0, shared_after
	call puts
	li s4, 3
	la t0, alive_trap
	csrw stvec, t0
	call arm_timer
	li t0, 32
	csrs sie, t0
	csrsi sstatus, 2
	j park

	.balign 4
alive_trap:
	csrr t0, scause
	li t1, 0x8000000000000005
	bne t0, t1, failed
	addi s4, s4, -1
	beqz s4, alive_done
	call arm_timer
	sret
alive_done:
	li t0, 32
	csrc sie, t0
	la s0, alive
	call puts
	j park

failed:
	la s0, failure
	call puts
	csrci sstatus, 2
	csrw sie, zero
	j park
puts:
	lbu a0, 0(s0)
	beqz a0, puts_done
	li a7, 1
	ecall
	addi s0, s0, 1
	j puts
puts_done:
	ret
	.section .rodata
#if STATIC_GUEST_ID == 0
#define VM_PREFIX "[RECOVER VM0] "
#else
#define VM_PREFIX "[RECOVER VM1] "
#endif
entered: .asciz VM_PREFIX "S_MODE_ENTERED\r\n"
sbi_ok: .asciz VM_PREFIX "SBI_BOUNDARY_OK\r\n"
shared_ok: .asciz VM_PREFIX "SHARED_OK\r\n"
timer_ok: .asciz VM_PREFIX "TIMER_IRQ_OK\r\n"
probe_begin: .asciz VM_PREFIX "PMP_PROBE_BEGIN\r\n"
recovery_ok: .asciz VM_PREFIX "ACCESS_DENIED_RECOVERED errno=-14\r\n"
canary_ok: .asciz VM_PREFIX "PRIVATE_CANARY_OK\r\n"
shared_after: .asciz VM_PREFIX "SHARED_AFTER_DENIAL_OK\r\n"
alive: .asciz VM_PREFIX "ALIVE_AFTER_DENIAL TIMER_TICKS=3\r\n"
failure: .asciz VM_PREFIX "FAIL\r\n"
	.section .bss
	.balign 16
	.space 8192
stack_top:
