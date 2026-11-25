/* SPDX-License-Identifier: BSD-3-Clause */
	.option push
	.option arch, +v
	.text
	.balign 4
	.global planb_parent_vector_set
planb_parent_vector_set:
	vsetvli t0, zero, e8, m8, tu, mu
	li t0, 0x49
	vmv.v.x v0, t0
	vmv.v.x v8, t0
	vmv.v.x v16, t0
	vmv.v.x v24, t0
	li t0, 7
	vsetvli zero, t0, e32, m2, tu, mu
	li t0, 3
	csrw vcsr, t0
	ret
	.balign 4
	.global planb_parent_vector_check
planb_parent_vector_check:
	csrr t0, vl
	li t1, 7
	bne t0, t1, fail
	csrr t0, vtype
	li t1, 0x11
	bne t0, t1, fail
	csrr t0, vcsr
	li t1, 3
	bne t0, t1, fail
	csrr t0, vstart
	bnez t0, fail
	vsetvli t0, zero, e8, m8, tu, mu
	csrr t0, vlenb
	slli t0, t0, 3
	mv t2, a0
	vse8.v v0, (t2)
	add t2, t2, t0
	vse8.v v8, (t2)
	add t2, t2, t0
	vse8.v v16, (t2)
	add t2, t2, t0
	vse8.v v24, (t2)
	add t2, t2, t0
	li t1, 0x49
1:
	lbu t0, 0(a0)
	bne t0, t1, fail
	addi a0, a0, 1
	bltu a0, t2, 1b
	li a0, 0
	ret
fail:
	li a0, 1
	ret
	.option pop
