/* SPDX-License-Identifier: BSD-3-Clause */
/* Standard RV64D/RVV context. Executed only after checking misa and vlenb.
 * These helpers preserve mstatus; no host/enclave FP or vector register is
 * used by the monitor C ABI, and every architectural register is saved.
 */
	.text
	.option push
	.option arch, +d
	.balign 4
	.global planb_fp_save
planb_fp_save:
	csrr t0, mstatus
	li t1, 0x6000
	csrs mstatus, t1
	fsd f0, 0(a0)
	fsd f1, 8(a0)
	fsd f2, 16(a0)
	fsd f3, 24(a0)
	fsd f4, 32(a0)
	fsd f5, 40(a0)
	fsd f6, 48(a0)
	fsd f7, 56(a0)
	fsd f8, 64(a0)
	fsd f9, 72(a0)
	fsd f10, 80(a0)
	fsd f11, 88(a0)
	fsd f12, 96(a0)
	fsd f13, 104(a0)
	fsd f14, 112(a0)
	fsd f15, 120(a0)
	fsd f16, 128(a0)
	fsd f17, 136(a0)
	fsd f18, 144(a0)
	fsd f19, 152(a0)
	fsd f20, 160(a0)
	fsd f21, 168(a0)
	fsd f22, 176(a0)
	fsd f23, 184(a0)
	fsd f24, 192(a0)
	fsd f25, 200(a0)
	fsd f26, 208(a0)
	fsd f27, 216(a0)
	fsd f28, 224(a0)
	fsd f29, 232(a0)
	fsd f30, 240(a0)
	fsd f31, 248(a0)
	frcsr t1
	sd t1, 256(a0)
	csrw mstatus, t0
	ret
	.balign 4
	.global planb_fp_restore
planb_fp_restore:
	csrr t0, mstatus
	li t1, 0x6000
	csrs mstatus, t1
	fld f0, 0(a0)
	fld f1, 8(a0)
	fld f2, 16(a0)
	fld f3, 24(a0)
	fld f4, 32(a0)
	fld f5, 40(a0)
	fld f6, 48(a0)
	fld f7, 56(a0)
	fld f8, 64(a0)
	fld f9, 72(a0)
	fld f10, 80(a0)
	fld f11, 88(a0)
	fld f12, 96(a0)
	fld f13, 104(a0)
	fld f14, 112(a0)
	fld f15, 120(a0)
	fld f16, 128(a0)
	fld f17, 136(a0)
	fld f18, 144(a0)
	fld f19, 152(a0)
	fld f20, 160(a0)
	fld f21, 168(a0)
	fld f22, 176(a0)
	fld f23, 184(a0)
	fld f24, 192(a0)
	fld f25, 200(a0)
	fld f26, 208(a0)
	fld f27, 216(a0)
	fld f28, 224(a0)
	fld f29, 232(a0)
	fld f30, 240(a0)
	fld f31, 248(a0)
	ld t1, 256(a0)
	fscsr t1
	csrw mstatus, t0
	ret
	.option arch, +v
	.balign 4
	.global planb_vector_length
planb_vector_length:
	csrr t0, mstatus
	li t1, 0x600
	csrs mstatus, t1
	csrr a0, vlenb
	csrw mstatus, t0
	ret
	.balign 4
	.global planb_vector_save
planb_vector_save:
	csrr t0, mstatus
	li t1, 0x600
	csrs mstatus, t1
	csrr t1, vstart
	sd t1, 264(a0)
	csrr t1, vl
	sd t1, 272(a0)
	csrr t1, vtype
	sd t1, 280(a0)
	csrr t1, vcsr
	sd t1, 288(a0)
	csrw vstart, zero
	csrr t1, vlenb
	slli t1, t1, 3
	addi t2, a0, 304
	vsetvli t3, zero, e8, m8, ta, ma
	vse8.v v0, (t2)
	add t2, t2, t1
	vse8.v v8, (t2)
	add t2, t2, t1
	vse8.v v16, (t2)
	add t2, t2, t1
	vse8.v v24, (t2)
	/* Saving must not alter the source context's vector CSRs. */
	ld t1, 272(a0)
	ld t2, 280(a0)
	vsetvl zero, t1, t2
	ld t1, 288(a0)
	csrw vcsr, t1
	ld t1, 264(a0)
	csrw vstart, t1
	csrw mstatus, t0
	ret
	.balign 4
	.global planb_vector_restore
planb_vector_restore:
	csrr t0, mstatus
	li t1, 0x600
	csrs mstatus, t1
	csrw vstart, zero
	csrr t1, vlenb
	slli t1, t1, 3
	addi t2, a0, 304
	vsetvli t3, zero, e8, m8, ta, ma
	vle8.v v0, (t2)
	add t2, t2, t1
	vle8.v v8, (t2)
	add t2, t2, t1
	vle8.v v16, (t2)
	add t2, t2, t1
	vle8.v v24, (t2)
	ld t1, 272(a0)
	ld t2, 280(a0)
	vsetvl zero, t1, t2
	ld t1, 288(a0)
	csrw vcsr, t1
	ld t1, 264(a0)
	csrw vstart, t1
	csrw mstatus, t0
	ret
	.option pop
