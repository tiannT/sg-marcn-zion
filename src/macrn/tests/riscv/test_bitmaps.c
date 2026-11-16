/* SPDX-License-Identifier: BSD-3-Clause */
#include <asm/lib/bits.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)

int main(void)
{
	volatile uint64_t bits[2] = {0, 0};
	int indices[] = {0, 31, 32, 63, 64, 95, 127};
	unsigned int i;
	for (i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i) {
		int n = indices[i];
		CHECK(!test_and_set_bit(n, bits));
		CHECK(test_and_set_bit(n, bits));
		CHECK(bitmap_test(n, bits));
		CHECK(test_and_clear_bit(n, bits));
		CHECK(!test_and_clear_bit(n, bits));
		CHECK(!bitmap_test(n, bits));
		bitmap_set_lock(n, bits);
		CHECK(bitmap_test_and_set_lock(n, bits));
		CHECK(bitmap_test_and_clear_lock(n, bits));
		CHECK(!bitmap_test_and_clear_lock(n, bits));
	}
	set_bit(63, bits);
	CHECK(bits[0] == 0x8000000000000000UL && bits[1] == 0);
	clear_bit(63, bits);
	CHECK(bits[0] == 0);
	CHECK(!bitmap32_test_and_set_lock(63, bits));
	CHECK(bitmap32_test(63, (volatile uint32_t *)bits));
	CHECK(bitmap32_test_and_clear_lock(63, bits));
	CHECK(!bitmap32_test_and_clear_lock(63, bits));
	CHECK(bits[0] == 0 && bits[1] == 0);
	return 0;
}
