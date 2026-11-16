/* SPDX-License-Identifier: BSD-3-Clause */
	.text
	.global _start
_start:
	.option push
	.option norelax
	la gp, __global_pointer$
	.option pop
	call init_main
1:
	j 1b
