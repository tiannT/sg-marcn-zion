/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PLANB_CONSOLE_H
#define RISCV_PLANB_CONSOLE_H
#include <types.h>
#ifdef CONFIG_PLAN_B_SERVICES
void planb_console_putchar(char ch);
void planb_console_reset(void);
uint32_t planb_console_size(void);
uint32_t planb_console_dropped(void);
uint32_t planb_console_generation(void);
uint32_t planb_console_word(uint32_t offset);
#endif
#endif
