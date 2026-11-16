/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/lib/bits.h>
#include <asm/system.h>

void set_bit(int nr, volatile void *p)
{
	volatile uint64_t *word = (volatile uint64_t *)p + (nr / 64);
	__atomic_fetch_or(word, 1UL << (nr % 64), __ATOMIC_SEQ_CST);
}

void clear_bit(int nr, volatile void *p)
{
	volatile uint64_t *word = (volatile uint64_t *)p + (nr / 64);
	__atomic_fetch_and(word, ~(1UL << (nr % 64)), __ATOMIC_SEQ_CST);
}

bool test_bit(int nr, uint64_t bits)
{
	return nr >= 0 && nr < 64 && !!(bits & (1UL << nr));
}

int test_and_set_bit(int nr, volatile void *p)
{
	volatile uint64_t *word = (volatile uint64_t *)p + (nr / 64);
	uint64_t mask = 1UL << (nr % 64);
	return !!(__atomic_fetch_or(word, mask, __ATOMIC_SEQ_CST) & mask);
}

int test_and_clear_bit(int nr, volatile void *p)
{
	volatile uint64_t *word = (volatile uint64_t *)p + (nr / 64);
	uint64_t mask = 1UL << (nr % 64);
	return !!(__atomic_fetch_and(word, ~mask, __ATOMIC_SEQ_CST) & mask);
}

int test_and_change_bit(int nr, volatile void *p)
{
	volatile uint64_t *word = (volatile uint64_t *)p + (nr / 64);
	uint64_t mask = 1UL << (nr % 64);
	return !!(__atomic_fetch_xor(word, mask, __ATOMIC_SEQ_CST) & mask);
}

uint32_t bit_weight(uint64_t bits)
{
	return __builtin_popcountl(bits);
}
