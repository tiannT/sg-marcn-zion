/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <asm/config.h>
#include <asm/board.h>
#include <asm/io.h>

#ifndef CONFIG_UART_REG_SHIFT
#define CONFIG_UART_REG_SHIFT 0
#endif
#ifndef CONFIG_UART_REG_WIDTH
#define CONFIG_UART_REG_WIDTH 1
#endif

static void *uart_reg(unsigned int reg)
{
	return (void *)(CONFIG_UART_BASE + ((uint64_t)reg << CONFIG_UART_REG_SHIFT));
}

static void uart_write(unsigned int reg, unsigned char c)
{
#if CONFIG_UART_REG_WIDTH == 4
	mmio_writel(c, uart_reg(reg));
#else
	mmio_writeb(c, uart_reg(reg));
#endif
}

static unsigned char uart_read(unsigned int reg)
{
#if CONFIG_UART_REG_WIDTH == 4
	return mmio_readl(uart_reg(reg));
#else
	return mmio_readb(uart_reg(reg));
#endif
}

static void put_char(char c)
{
	while (!(uart_read(5) & 0x20))
		;
	uart_write(0, c);
}

static void get_char(char *c)
{
	char d;

	d = uart_read(5);
	if ((d & 0x1) == 1)
		*c = uart_read(0);
}

char uart16550_getc(void)
{
	char c = -1;

	get_char(&c);
	return c;
}

size_t uart16550_puts(const char *buf, uint32_t len)
{
	int i = 0;
	if (buf == 0)
		return 0;

	for (i = 0U; i < len; i++) {
		if (buf[i] == '\n')
			put_char('\r');
		put_char(buf[i]);
	}

	return len;
}

void uart16550_init(bool early_boot)
{}

void early_putch(char c)
{
	put_char(c);
}

char early_getch(void)
{
	char c = -1;

	get_char(&c);
	return c;
}

void early_printk(const char *buf)
{
	int i = 0;
	if (buf == 0)
		return;

	while (buf[i] != '\0') {
		if (buf[i] == '\n')
			put_char('\r');
		put_char(buf[i++]);
	}
}

/* The initial log level*/
void npk_log_write(const char *buf, size_t buf_len)
{}
