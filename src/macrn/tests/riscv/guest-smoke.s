/* SPDX-License-Identifier: BSD-3-Clause */
/* QEMU-only supervisor guest: exercises vCPU entry, MMIO exits and SBI TIME. */
	.option norvc
	.section .text,"ax",@progbits
	.global _start
_start:
	la sp, stack_top
	la s0, entered
	call puts
	li a7, 0x10
	li a6, 0
	ecall
	bnez a0, fail
	la s0, sbi_ok
	call puts
#ifdef PROBE_SVPBMT
	/* Keep code identity mapped; read RAM through a noncacheable alias.
	 * This specifically checks that M-mode enabled menvcfg.PBMTE.
	 */
	la t0, svpbmt_fault
	csrw stvec, t0
	la t0, svpbmt_root
	li t1, 512
4:
	sd zero, 0(t0)
	addi t0, t0, 8
	addi t1, t1, -1
	bnez t1, 4b
	la t0, svpbmt_root
	li t1, 0x200000cf /* 1 GiB identity mapping for 0x80000000 RAM */
	sd t1, 16(t0)
	li t2, 1
	slli t2, t2, 61 /* PBMT=NC */
	or t1, t1, t2
	sd t1, 8(t0) /* 0x40000000 alias -> physical 0x80000000 */
	srli t0, t0, 12
	li t1, 8
	slli t1, t1, 60
	or t0, t0, t1
	sfence.vma
	csrw satp, t0
	sfence.vma
	li t0, 0x40200000
	ld t1, 0(t0)
	la t0, _start
	ld t2, 0(t0)
	bne t1, t2, svpbmt_fault
	csrw satp, zero
	sfence.vma
	la s0, svpbmt_ok
	call puts
#endif
#ifdef PROBE_MONITOR_MEMORY
	li t0, 0x90000000
	ld t1, 0(t0)
	la s0, private_bypass
	call puts
	j fail
#endif
	la t0, timer_trap
	csrw stvec, t0
	li t0, 32
	csrs sie, t0
	csrsi sstatus, 2
	csrr a0, time
	li t0, 100000
	add a0, a0, t0
	li a7, 0x54494d45
	li a6, 0
	ecall
	bnez a0, fail
wait_timer:
	wfi
	j wait_timer
timer_trap:
	csrr t0, scause
	li t1, 0x8000000000000005
	bne t0, t1, fail
	la s0, timer_ok
	call puts
	li t0, 0x100000
	li t1, 0x5555
	sw t1, 0(t0)
	/* QEMU shutdown is asynchronous; do not fall through to fail. */
	j 1f
fail:
	la s0, failed
	call puts
	li t0, 0x100000
	li t1, 0x13333
	sw t1, 0(t0)
1:
	wfi
	j 1b
#ifdef PROBE_SVPBMT
svpbmt_fault:
	csrw satp, zero
	sfence.vma
	j fail
#endif
puts:
2:
	lbu a0, 0(s0)
	beqz a0, 3f
	li a7, 1
	ecall
	addi s0, s0, 1
	j 2b
3:
	ret
	.section .rodata
entered: .asciz "GUEST_SMODE_ENTERED\n"
sbi_ok: .asciz "GUEST_SBI_BASE_OK\n"
timer_ok: .asciz "GUEST_TIMER_IRQ_OK\n"
failed: .asciz "GUEST_FAILED\n"
private_bypass: .asciz "GUEST_PRIVATE_ACCESS_BYPASSED\n"
svpbmt_ok: .asciz "GUEST_SVPBMT_OK\n"
	.section .bss
	.balign 16
	.space 8192
stack_top:
#ifdef PROBE_SVPBMT
	.balign 4096
svpbmt_root:
	.space 4096
#endif
