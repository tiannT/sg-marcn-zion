/* SPDX-License-Identifier: BSD-3-Clause */
	.section .rodata.static_guests,"a",@progbits
	.balign 8
	.global static_guest0_begin, static_guest0_end
	.global static_guest1_begin, static_guest1_end
static_guest0_begin:
	.incbin "static-guest0.bin"
static_guest0_end:
	.balign 8
static_guest1_begin:
	.incbin "static-guest1.bin"
static_guest1_end:
