/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_BRINGUP_TRACE_H
#define RISCV_BRINGUP_TRACE_H

/* Stackless diagnostics for the single-hart embedded-Guest build.
 * Only t0-t6 are clobbered: ZSBL's a0/a1/a2 remain intact.
 * UART clocks and baud rate remain as configured by the vendor loader.
 */
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
#ifndef CONFIG_UART_REG_SHIFT
#define CONFIG_UART_REG_SHIFT 0
#endif
#ifndef CONFIG_UART_REG_WIDTH
#define CONFIG_UART_REG_WIDTH 1
#endif

	.macro BRINGUP_PUTC value
	li t4, CONFIG_UART_BASE
	li t6, 0x100000
.Luart_wait\@:
#if CONFIG_UART_REG_WIDTH == 4
	lw t5, (5 << CONFIG_UART_REG_SHIFT)(t4)
#else
	lbu t5, (5 << CONFIG_UART_REG_SHIFT)(t4)
#endif
	andi t5, t5, 0x20
	bnez t5, .Luart_write\@
	addi t6, t6, -1
	bnez t6, .Luart_wait\@
	j .Luart_done\@
.Luart_write\@:
#if CONFIG_UART_REG_WIDTH == 4
	sw \value, 0(t4)
#else
	sb \value, 0(t4)
#endif
.Luart_done\@:
	.endm

	.macro BRINGUP_PUTS label
	.option push
	.option norelax
	la t3, \label
	.option pop
.Luart_string\@:
	lbu t0, 0(t3)
	beqz t0, .Luart_string_done\@
	BRINGUP_PUTC t0
	addi t3, t3, 1
	j .Luart_string\@
.Luart_string_done\@:
	.endm

	.macro BRINGUP_CSR_HEX csr
	csrr t1, \csr
	li t2, 60
.Luart_hex\@:
	srl t0, t1, t2
	andi t0, t0, 15
	li t3, 10
	bltu t0, t3, .Luart_digit\@
	addi t0, t0, 87
	j .Luart_hex_write\@
.Luart_digit\@:
	addi t0, t0, 48
.Luart_hex_write\@:
	BRINGUP_PUTC t0
	addi t2, t2, -4
	bgez t2, .Luart_hex\@
	.endm
#endif
#endif
