/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef LIBFDT_ENV_H
#define LIBFDT_ENV_H
#ifdef __ACRN__
#include <types.h>
typedef uint64_t uintptr_t;
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif

#define INT_MAX 0x7fffffff
#define UINT_MAX 0xffffffffU
#define INT32_MAX INT_MAX
#ifndef UINT32_MAX
#define UINT32_MAX UINT_MAX
#endif
#define FDT_FORCE
#define FDT_BITWISE
typedef uint16_t fdt16_t;
typedef uint32_t fdt32_t;
typedef uint64_t fdt64_t;
static inline uint16_t fdt16_to_cpu(fdt16_t x) { return __builtin_bswap16(x); }
static inline fdt16_t cpu_to_fdt16(uint16_t x) { return __builtin_bswap16(x); }
static inline uint32_t fdt32_to_cpu(fdt32_t x) { return __builtin_bswap32(x); }
static inline fdt32_t cpu_to_fdt32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint64_t fdt64_to_cpu(fdt64_t x) { return __builtin_bswap64(x); }
static inline fdt64_t cpu_to_fdt64(uint64_t x) { return __builtin_bswap64(x); }

static inline void *fdt_memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--) *d++ = *s++;
	return dst;
}
static inline void *fdt_memmove(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	if ((uint64_t)d > (uint64_t)s) {
		while (n) { --n; d[n] = s[n]; }
	} else {
		while (n--) *d++ = *s++;
	}
	return dst;
}
static inline void *fdt_memset(void *dst, int value, size_t n)
{
	unsigned char *d = dst;
	while (n--) *d++ = (unsigned char)value;
	return dst;
}
static inline int fdt_memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *x = a, *y = b;
	while (n--) { if (*x != *y) return *x - *y; ++x; ++y; }
	return 0;
}
static inline void *fdt_memchr(const void *a, int c, size_t n)
{
	const unsigned char *p = a;
	while (n--) { if (*p == (unsigned char)c) return (void *)p; ++p; }
	return NULL;
}
static inline size_t fdt_strnlen(const char *s, size_t max)
{
	size_t n = 0;
	while (n < max && s[n]) ++n;
	return n;
}
static inline size_t fdt_strlen(const char *s) { return fdt_strnlen(s, UINT_MAX); }
static inline int fdt_strncmp(const char *a, const char *b, size_t n)
{
	while (n--) {
		if ((unsigned char)*a != (unsigned char)*b || !*a)
			return (unsigned char)*a - (unsigned char)*b;
		++a; ++b;
	}
	return 0;
}
static inline int fdt_strcmp(const char *a, const char *b) { return fdt_strncmp(a, b, UINT_MAX); }
static inline char *fdt_strchr(const char *s, int c)
{
	do { if (*s == c) return (char *)s; } while (*s++);
	return NULL;
}
static inline char *fdt_strrchr(const char *s, int c)
{
	char *last = NULL;
	do { if (*s == c) last = (char *)s; } while (*s++);
	return last;
}
static inline char *fdt_strcpy(char *d, const char *s) { fdt_memcpy(d, s, fdt_strlen(s) + 1); return d; }
#define memcpy fdt_memcpy
#define memmove fdt_memmove
#define memset fdt_memset
#define memcmp fdt_memcmp
#define memchr fdt_memchr
#define strlen fdt_strlen
#define strnlen fdt_strnlen
#define strcmp fdt_strcmp
#define strncmp fdt_strncmp
#define strchr fdt_strchr
#define strrchr fdt_strrchr
#define strcpy fdt_strcpy
#endif
