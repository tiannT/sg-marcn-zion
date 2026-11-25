/* SPDX-License-Identifier: BSD-3-Clause */
/* QEMU-only S-mode fixture. GDB supplies an IRQ in the final entry window. */
	.option norvc
	.section .text,"ax",@progbits
	.globl _image
_image:
	j _start
	.word 0
	.quad 0x200000
	.quad 0x10000
	.quad 0
	.word 2
	.word 0
	.quad 0
	.ascii "RISCV\0\0\0"
	.ascii "RSC\005"
	.word 0
_start:
	csrci sstatus, 2
	csrw sie, zero
	/* The second copy parks in VM1's QEMU private bank. */
	auipc t0, 0
	li t1, 0x140000000
	bgeu t0, t1, peer_park
	la sp, stack_top
	la t0, irq_handler
	csrw stvec, t0
	la s0, ready
	call puts
	li t0, 0x0c000000
	li t1, 1
	sw t1, (47 * 4)(t0)
	li t0, 0x0c002084
	li t1, (1 << 15)
	sw t1, 0(t0)
	li t0, 0x0c201000
	sw zero, 0(t0)
	li t0, 0x200
	csrs sie, t0
	csrsi sstatus, 2
	/* SBI BASE has no a2 input. The marker selects one load_vmcs call. */
	li a2, 0x52414345
	li a7, 0x10
	li a6, 0
	ecall
	.globl guest_wait
guest_wait:
	wfi
	j guest_wait
	.balign 4
	.globl irq_handler
irq_handler:
	csrr t0, scause
	li t1, 0x8000000000000009
	bne t0, t1, failed
	li t0, 0x0c201004
	lw t1, 0(t0)
	li t2, 47
	bne t1, t2, failed
	sw t1, 0(t0)
	li a2, 0
	la s0, success
	call puts
	.globl guest_done
guest_done:
	j guest_done
failed:
	csrci sstatus, 2
	li a2, 0
	la s0, failure
	call puts
	j guest_done
peer_park:
	wfi
	j peer_park
	.globl guest_unmask
guest_unmask:
	li t0, 0x0c002084
	li t1, (1 << 15)
	sw t1, 0(t0)
	j guest_wait
puts:
	mv s1, ra
1:
	lbu a0, 0(s0)
	beqz a0, 2f
	li a7, 1
	ecall
	addi s0, s0, 1
	j 1b
2:
	mv ra, s1
	ret
ready:
	.asciz "[ENTRY RACE] READY\n"
success:
	.asciz "[ENTRY RACE] IRQ_HANDLED_OK\n"
failure:
	.asciz "[ENTRY RACE] FAIL\n"
	.balign 16
	.space 1024
stack_top:
