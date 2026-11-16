/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/mem.h>
#include <asm/cpu.h>
#include <asm/bringup_trace.h>

	.section .text.entry,"ax",@progbits

	.globl _start
_start:
#ifdef CONFIG_MACRN
	li t0, 0x88
	csrc mstatus, t0
	csrw mie, zero
	csrr a0, mhartid

	li t0, NR_CPUS
	bgeu a0, t0, hart_halt
	li t0, BSP_CPU_ID
	blt a0, t0, hart_halt
	/* Initialize gp before any address may be relaxed to gp-relative. */
	.option push
	.option norelax
	la gp, __global_pointer$
	.option pop
	bne a0, t0, wait_boot_data
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	BRINGUP_PUTS bringup_runtime_message
#endif
	la t0, __bss_start
	la t1, __bss_end
clear_boot_bss:
	bgeu t0, t1, save_boot_data
	sd zero, 0(t0)
	addi t0, t0, 8
	j clear_boot_bss

save_boot_data:
	la t0, fw_dtb
	sd a1, 0(t0)
	la t0, fw_dinfo
	sd a2, 0(t0)
	fence rw, rw
	la t0, boot_data_ready
	li t1, 1
	sd t1, 0(t0)
wait_boot_data:
	la t0, boot_data_ready
	ld t1, 0(t0)
	beqz t1, wait_boot_data
	fence r, rw
#if CONFIG_SSTC
	csrr t0, 0x30a /* menvcfg; numeric name supports older assemblers */
	/* STCE, PBMTE, ADUE and cache-block permissions. Unsupported WARL
	 * fields read as zero. Do not depend on the previous stage enabling
	 * Svpbmt for the supervisor payload.
	 */
	li t1, 0xe0000000000000f0
	or t0, t0, t1
#ifdef CONFIG_PLAN_B_SERVICES
	/* VM1's virtio "DMA" is a coherent M-mode CPU copy. True CBO.INVAL
	 * can discard those dirty writes (and monitor zeroing). Force lower
	 * privilege invalidations to clean+invalidate via menvcfg.CBIE=01.
	 * Keep the native device-owning VM0's policy unchanged.
	 */
	csrr t2, mhartid
	li t3, 1
	bne t2, t3, .Lplanb_envcfg_ready
	andi t0, t0, -33
.Lplanb_envcfg_ready:
#endif
	csrw 0x30a, t0
#endif
	csrwi mcounteren, 0x7
	csrwi scounteren, 0x7
	jal init_mstack
	call reset_mtimer
	csrw mip, 0x0
	call boot_trap
	li t0, 0x1900
	csrs mstatus, t0

	li t0, 0x222
	csrs mideleg, t0

	li t0, 0xaaa
	csrw mie, t0

	li t0, 0xf0f5ff
	csrw medeleg, t0

	csrw mscratch, sp
	la t0, _boot
	csrw mepc, t0
	mret
#else
	j _boot
#endif

	.globl _boot
_boot:
#ifndef CONFIG_MACRN
	li t0, 0
	csrw sie, t0
	li t0, 0xC0000
	csrw sstatus, t0
	jal init_stack
	li a1, 0
	call kernel_init
#else
	li t0, BSP_CPU_ID
	bne a0, t0, secondary

	li a1, 0
	call kernel_init
1:
	la ra, 1b
	ret
#endif

hart_halt:
	wfi
	j  hart_halt

	.globl secondary
secondary:
	la t0, g_cpus
	li t1, 1
	amoadd.w zero, t1, (t0)
#ifndef CONFIG_MACRN
	li t0, 0
	csrw sie, t0
	li t0, 0xC0000
	csrw sstatus, t0
	jal init_stack
	call boot_trap
#else
	jal boot_idle
#endif
	call start_secondary 

#ifdef CONFIG_MACRN
init_mstack:
	li sp, ACRN_MSTACK_TOP
	li t0, ACRN_MSTACK_SIZE
	mul t0, a0, t0
	sub sp, sp, t0
	csrw mscratch, sp
	ret
#else
init_stack:
	li sp, ACRN_STACK_TOP
	li t0, ACRN_STACK_SIZE
	mul t0, a0, t0
	sub sp, sp, t0
	csrw sscratch, sp
	ret
#endif

	.globl boot_idle
boot_idle:
	wfi
	ret

	.globl _end_boot
_end_boot:

#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	.section .rodata
bringup_runtime_message:
	.asciz "[BRINGUP] M_RUNTIME_90000000\r\n"
#endif

#ifdef CONFIG_MACRN
	.section .data,"aw",@progbits
	.balign 8
boot_data_ready:
	.dword 0
#endif
