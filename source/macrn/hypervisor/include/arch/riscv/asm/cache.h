/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef __RISCV_CACHE_H__
#define __RISCV_CACHE_H__

#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/system.h>

#define RISCV_L1_CACHE_SHIFT	CONFIG_RISCV_L1_CACHE_SHIFT

extern size_t dcache_block_size;
static inline size_t get_dcache_block_size(void)
{
	return 1 << RISCV_L1_CACHE_SHIFT;
}

#ifdef CONFIG_PLATFORM_SG2044
/* Zicbom: emit the ratified instruction encoding with binutils 2.38. */
#define clean_dcache(x) do { \
	asm volatile (".insn i 0x0f, 2, x0, %0, 1"::"r"(x):"memory"); \
} while (0)

#define invalidate_dcache(x) do { \
	asm volatile (".insn i 0x0f, 2, x0, %0, 2"::"r"(x):"memory"); \
} while (0)

#define flush_dcache(x) invalidate_dcache(x)
#else
/* QEMU virt has coherent RAM; it need not implement Zicbom. */
#define clean_dcache(x) do { (void)(x); dsb(); } while (0)
#define invalidate_dcache(x) clean_dcache(x)
#define flush_dcache(x) clean_dcache(x)
#endif

static inline void invalidate_icache_local(void)
{
	asm volatile ("fence.i");
	dsb();               /* Ensure completion of the flush I-cache */
	isb();
}

static inline void clean_dcache_range(const void *p, uint64_t size)
{
	const void *e = p + size;

	dsb();
	p = (void *)((uint64_t)p & ~(dcache_block_size - 1));
	for (; p < e; p += dcache_block_size)
		clean_dcache(p);
	dsb();

	return;
}

static inline void invalidate_dcache_range(const void *p, uint64_t size)
{
	const void *e = p + size;

	dsb();
	p = (void *)((uint64_t)p & ~(dcache_block_size - 1));
	for (; p < e; p += dcache_block_size)
		invalidate_dcache(p);
	dsb();

	return;
}

static inline void flush_dcache_range(const void *p, uint64_t size)
{
	const void *e = p + size;

	dsb();
	p = (void *)((uint64_t)p & ~(dcache_block_size - 1));
	for (; p < e; p += dcache_block_size)
		flush_dcache(p);
	dsb();

	return;
}
#endif

#endif /* __RISCV_CACHE_H__ */
