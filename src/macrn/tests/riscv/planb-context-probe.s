/* SPDX-License-Identifier: BSD-3-Clause */
/* Real bare S-mode enclave. The required acceptance demo remains unmodified.
 * Check private canary/GPR/FP/RVV across two actual untrusted OCALLs.
 */
	.option push
	.option arch, +d, +v
	.option norelax
	.text
	.balign 4
	.global _start
_start:
	mv s0, a6
	mv s1, a1
	li s2, 0
	li s3, 0x1122334455667788
	ld s4, 0(s0)
	li t0, 0x8000
	add t0, t0, s1
	ld t0, 0(t0)
	li t1, 0x504d5043414e4152
	bne t0, t1, fail
	li t0, 0x20
	fscsr t0
	li t0, 0x1234000056780000
.macro fill_fp n
	fmv.d.x f\n, t0
	addi t0, t0, 1
.endm
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	fill_fp \n
.endr
	beqz s4, yield
	vsetvli t0, zero, e8, m8, tu, mu
	li t0, 0x36
	vmv.v.x v0, t0
	vmv.v.x v8, t0
	vmv.v.x v16, t0
	vmv.v.x v24, t0
yield:
	beqz s4, 1f
	li t0, 7
	vsetvli zero, t0, e32, m2, tu, mu
	li t0, 3
	csrw vstart, t0
	li t0, 5
	csrw vcsr, t0
1:
	li a0, 1
	li a6, 3004
	li a7, 0x08424b45
	ecall
	li t0, 0x1122334455667788
	bne s3, t0, fail
	frcsr t0
	li t1, 0x20
	bne t0, t1, fail
	li t0, 0x1234000056780000
.macro check_fp n
	fmv.x.d t1, f\n
	bne t1, t0, fail
	addi t0, t0, 1
.endm
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	check_fp \n
.endr
	beqz s4, next
	csrr t0, vstart
	li t1, 3
	bne t0, t1, fail
	csrr t0, vl
	li t1, 7
	bne t0, t1, fail
	csrr t0, vtype
	li t1, 0x11
	bne t0, t1, fail
	csrr t0, vcsr
	li t1, 5
	bne t0, t1, fail
	csrw vstart, zero
	vsetvli t0, zero, e8, m8, tu, mu
	csrr t0, vlenb
	slli t0, t0, 3
	li t2, 0x10000
	add t2, t2, s1
	mv t3, t2
	vse8.v v0, (t2)
	add t2, t2, t0
	vse8.v v8, (t2)
	add t2, t2, t0
	vse8.v v16, (t2)
	add t2, t2, t0
	vse8.v v24, (t2)
	add t2, t2, t0
	li t1, 0x36
2:
	lbu t0, 0(t3)
	bne t0, t1, fail
	addi t3, t3, 1
	bltu t3, t2, 2b
next:
	addi s2, s2, 1
	sd s2, 8(s0)
	li t0, 2
	bltu s2, t0, yield
	li t0, 0x50415353
	sd t0, 16(s0)
	li a0, 0
	j exit
fail:
	li a0, 1
exit:
	li a6, 3006
	li a7, 0x08424b45
	ecall
	j .
	.option pop
