/* SPDX-License-Identifier: BSD-3-Clause */
/* Minimal initramfs PID 1. No target libc or shell is required. */
typedef unsigned long u64;
struct time_spec { long seconds, nanoseconds; };
struct uts_name { char field[6][65]; };
static long console_fd = 1;
static long syscall4(long nr, long x0, long x1, long x2, long x3)
{
	register long a0 asm("a0") = x0;
	register long a1 asm("a1") = x1;
	register long a2 asm("a2") = x2;
	register long a3 asm("a3") = x3;
	register long a7 asm("a7") = nr;
	asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
	return a0;
}
static void puts(const char *p)
{
	const char *end = p;
#ifdef STATIC_LINUX_VM
	static int line_start = 1;
#if STATIC_LINUX_VM == 0
	static const char prefix[] = "[LINUX VM0] ";
#else
	static const char prefix[] = "[LINUX VM1] ";
#endif
	if (line_start) syscall4(64, console_fd, (long)prefix, sizeof(prefix) - 1, 0);
#endif
	while (*end) ++end;
	syscall4(64, console_fd, (long)p, end - p, 0);
#ifdef STATIC_LINUX_VM
	if (end != p) line_start = end[-1] == '\n';
#endif
}
void init_main(void)
{
	struct uts_name name;
	struct time_spec before, after, delay = {1, 0};
	u64 cpus = 0;
	int i;
	console_fd = syscall4(56, -100, (long)"/dev/console", 1, 0);
	if (console_fd < 0) goto fail;
	if (syscall4(172, 0, 0, 0, 0) != 1) goto fail;
	puts("GUEST_LINUX_INIT_OK\n");
	if (syscall4(160, (long)&name, 0, 0, 0)) goto fail;
	puts("GUEST_KERNEL="); puts(name.field[2]); puts("\n");
	if (syscall4(123, 0, sizeof(cpus), (long)&cpus, 0) < 0 || cpus != 1) goto fail;
	puts("GUEST_LINUX_SINGLE_VCPU_OK\n");
	for (i = 0; i < 3; ++i) {
		if (syscall4(113, 1, (long)&before, 0, 0)) goto fail;
		if (syscall4(115, 1, 0, (long)&delay, 0)) goto fail;
		if (syscall4(113, 1, (long)&after, 0, 0)) goto fail;
		if (after.seconds < before.seconds ||
		    (after.seconds == before.seconds && after.nanoseconds <= before.nanoseconds)) goto fail;
		puts("GUEST_LINUX_TIMER_TICK\n");
	}
	puts("GUEST_LINUX_TESTS_PASSED\n");
#ifdef STATIC_LINUX_VM
	for (;;) syscall4(115, 1, 0, (long)&delay, 0);
#else
	syscall4(142, 0xfee1dead, 0x28121969, 0x4321fedc, 0);
#endif
fail:
	puts("GUEST_LINUX_TESTS_FAILED\n");
	for (;;) syscall4(115, 1, 0, (long)&delay, 0);
}
