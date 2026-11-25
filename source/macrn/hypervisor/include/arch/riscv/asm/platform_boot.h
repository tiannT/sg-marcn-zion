/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PLATFORM_BOOT_H
#define RISCV_PLATFORM_BOOT_H
#include <types.h>
void platform_boot_prepare(void);
bool platform_guest_ram_range(uint64_t address, uint64_t size);
#endif
