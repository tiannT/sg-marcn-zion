/* SPDX-License-Identifier: BSD-3-Clause */
	.section .rodata.bringup_guest,"a",@progbits
	.balign 8
	.global builtin_guest_begin
	.global builtin_guest_end
builtin_guest_begin:
	.incbin "bringup-guest.bin"
builtin_guest_end:
