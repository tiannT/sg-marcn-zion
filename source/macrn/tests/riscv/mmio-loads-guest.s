/* SPDX-License-Identifier: BSD-3-Clause */
/* Real S-mode MMIO instructions. Device MAC bytes exercise negative loads. */
	.option norvc
	.section .text,"ax",@progbits
	.globl _start
_start:
	csrci sstatus, 2
	csrw sie, zero
	la sp, stack_top
	la t0, failed
	csrw stvec, t0
#if STATIC_GUEST_ID == 0
	la s0, alive
	call puts
idle:
	wfi
	j idle
#else
	li t0, 0x93001100
	lb t1, 2(t0)
	li t2, -128
	bne t1, t2, failed
	lbu t1, 2(t0)
	li t2, 128
	bne t1, t2, failed
	lh t1, 2(t0)
	li t2, -128
	bne t1, t2, failed
	lhu t1, 2(t0)
	li t2, 0xff80
	bne t1, t2, failed
	lw t1, 0(t0)
	li t2, 0xffffffffff804402
	bne t1, t2, failed
	lwu t1, 0(t0)
	li t2, 0xff804402
	bne t1, t2, failed
	ld t1, 0(t0)
	li t2, 0x00010100ff804402
	bne t1, t2, failed
	lw zero, 0(t0)
	bnez zero, failed
	.option push
	.option rvc
	li s1, 0x93001100
	c.lw a3, 0(s1)
	li t2, 0xffffffffff804402
	bne a3, t2, failed
	c.ld a4, 0(s1)
	li t2, 0x00010100ff804402
	bne a4, t2, failed
	mv s2, sp
	mv sp, s1
	c.lwsp t3, 0(sp)
	li t2, 0xffffffffff804402
	bne t3, t2, failed
	c.ldsp t4, 0(sp)
	li t2, 0x00010100ff804402
	bne t4, t2, failed
	mv sp, s2
	.option pop
	/* UART scratch is an actual emulated byte register. */
	li t0, 0x10000007
	li t1, 0x81
	sb t1, 0(t0)
	lb t2, 0(t0)
	li t3, -127
	bne t2, t3, failed
	lbu t2, 0(t0)
	bne t2, t1, failed
	li t1, 0x82
	sh t1, 0(t0)
	lbu t2, 0(t0)
	bne t2, t1, failed
	li t1, 0x83
	sw t1, 0(t0)
	lbu t2, 0(t0)
	bne t2, t1, failed
	li t1, 0x84
	sd t1, 0(t0)
	lbu t2, 0(t0)
	bne t2, t1, failed
	/* These store instructions use their full, distinct register fields. */
	.option push
	.option rvc
	li s1, 0x10000000
	/* The naturally aligned divisor latch permits C.S[WD] accesses. */
	li t0, 0x10000003
	li t1, 0x80
	sb t1, 0(t0)
	li a3, 0x86
	c.sw a3, 0(s1)
	lbu t2, 0(s1)
	bne t2, a3, failed
	li a4, 0x87
	c.sd a4, 0(s1)
	lbu t2, 0(s1)
	bne t2, a4, failed
	mv s2, sp
	mv sp, s1
	li t3, 0x88
	c.swsp t3, 0(sp)
	lbu t2, 0(sp)
	bne t2, t3, failed
	li t4, 0x89
	c.sdsp t4, 0(sp)
	lbu t2, 0(sp)
	bne t2, t4, failed
	mv sp, s2
	.option pop
	li t0, 0x10000003
	li t1, 3
	sb t1, 0(t0)
	la s0, passed
	call puts
idle:
	wfi
	j idle
#endif
failed:
	la sp, stack_top
	la s0, failure
	call puts
	j idle
puts:
	lbu a0, 0(s0)
	beqz a0, puts_end
	li a7, 1
	ecall
	addi s0, s0, 1
	j puts
puts_end:
	ret
	.section .rodata
alive: .asciz "MMIO_VM0_ALIVE\n"
passed: .asciz "MMIO_VM1_SIGN_UNSIGNED_COMPRESSED_SP_AND_STORES_PASS\n"
failure: .asciz "MMIO_EMULATION_FAIL\n"
	.section .bss
	.balign 16
	.space 8192
stack_top:
