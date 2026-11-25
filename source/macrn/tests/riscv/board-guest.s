/* SPDX-License-Identifier: BSD-3-Clause */
/* No disk/network/Flash operations. Real boards park after serial output.
 * Only the QEMU build uses the existing SBI shutdown test endpoint.
 */
#ifndef CONFIG_TIMEBASE_HZ
#define CONFIG_TIMEBASE_HZ 10000000
#endif
	.option norvc
	.section .text,"ax",@progbits
	.global _start
_start:
	csrci sstatus, 2
	csrw sie, zero
	mv s1, a1
	la t0, __guest_bss_start
	la t1, __guest_bss_end
clear_guest_bss:
	bgeu t0, t1, guest_ready
	sd zero, 0(t0)
	addi t0, t0, 8
	j clear_guest_bss
guest_ready:
	la sp, guest_stack_top
	la t0, unexpected_trap
	csrw stvec, t0
	la s0, entered
	call puts
	beqz s1, fail
	lw t0, 0(s1)
	li t1, 0xffffffffedfe0dd0
	bne t0, t1, fail
	la s0, fdt_ok
	call puts
	li a7, 0x10
	li a6, 0
	ecall
	bnez a0, fail
	la s0, sbi_ok
	call puts
	la t0, timer_trap
	csrw stvec, t0
	li s4, 3
	call arm_timer
	li t0, 32
	csrs sie, t0
	csrsi sstatus, 2
wait_timer:
	wfi
	j wait_timer
arm_timer:
	csrr a0, time
	li t0, (CONFIG_TIMEBASE_HZ / 100)
	add a0, a0, t0
	li a7, 0x54494d45
	li a6, 0
	ecall
	bnez a0, fail
	ret
	.balign 4
timer_trap:
	csrr t0, scause
	li t1, 0x8000000000000005
	bne t0, t1, fail
	la s0, timer_tick
	call puts
	addi s4, s4, -1
	beqz s4, timer_done
	call arm_timer
	sret
timer_done:
	li t0, 32
	csrc sie, t0
	li a0, -1
	li a7, 0x54494d45
	li a6, 0
	ecall
	bnez a0, fail
	la s0, timer_ok
	call puts
#ifdef CONFIG_BRINGUP_PMP_PROBE
	la s0, probe_start
	call puts
	li t0, 0x90000000
	ld t1, 0(t0)
	la s0, probe_bypassed
	call puts
	j fail
#else
	la s0, passed
	call puts
#ifdef RUN_ON_QEMU
	li a7, 8
	ecall
#endif
	j park
#endif
unexpected_trap:
	la s0, bad_trap
	call puts
fail:
	la s0, failed
	call puts
	csrci sstatus, 2
	csrw sie, zero
park:
	wfi
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
entered: .asciz "[BRINGUP GUEST] S_MODE_ENTERED\n"
fdt_ok: .asciz "[BRINGUP GUEST] FDT_OK\n"
sbi_ok: .asciz "[BRINGUP GUEST] SBI_BASE_OK\n"
timer_tick: .asciz "[BRINGUP GUEST] TIMER_TICK\n"
timer_ok: .asciz "[BRINGUP GUEST] TIMER_IRQ_OK\n"
passed: .asciz "[BRINGUP GUEST] SMOKE_PASS_PARK\n"
failed: .asciz "[BRINGUP GUEST] FAIL\n"
bad_trap: .asciz "[BRINGUP GUEST] UNEXPECTED_S_TRAP\n"
probe_start: .asciz "[BRINGUP GUEST] PMP_PROBE_BEGIN\n"
probe_bypassed: .asciz "[BRINGUP GUEST] PMP_BYPASS_FAIL\n"
	.section .bss
	.balign 16
	.space 8192
guest_stack_top:
