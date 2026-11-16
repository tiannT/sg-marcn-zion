/* SPDX-License-Identifier: BSD-3-Clause */
/* Additional real CPU/PMP/context regression, not a replacement for the
 * original acceptance demo. All accesses target this process's own EPM VMA.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "shared/zion_user.h"

static sigjmp_buf recovery;
static volatile sig_atomic_t expected_fault, caught_signal;
static int device, vector_enabled;
static unsigned long operation;
static struct zion_ioctl_run_enclave run;
extern long native_crypto_fp_guard(void *, uint64_t, uint64_t, uint64_t *, uint64_t *);
extern void planb_parent_vector_set(void);

static void fault(int signal_number)
{
	if (!expected_fault) _exit(128 + signal_number);
	caught_signal = signal_number;
	siglongjmp(recovery, 1);
}

/* Called by the pinned source FP guard; libc ioctl itself uses no FP/RVV. */
long native_crypto_run(void *unused)
{
	long result;
	(void)unused;
	if (vector_enabled) planb_parent_vector_set();
	result = ioctl(device, operation, &run);
	/* Linux6.12 deliberately discards user RVV state on syscall entry
 * (arch/riscv/kernel/traps.c:do_trap_ecall_u). Parent S-mode preservation
 * is checked by the separate real SBI kernel audit, not across ioctl. */
	return result;
}

static void require(int condition, const char *message)
{
	if (condition) return;
	fprintf(stderr, "[PLANB ENCLAVE GUARD] FAIL: %s errno=%d\n", message, errno);
	exit(1);
}

int main(int argc, char **argv)
{
	struct zion_ioctl_create_enclave create = {0}, request;
	struct sigaction handler = {0};
	uint8_t *epm;
	volatile uint64_t *canary, *utm;
	uint64_t observed_fp, observed_fcsr;
	unsigned int count;
	volatile unsigned int edges = 0;
	size_t i;
	FILE *payload;
	long payload_size;
	if (argc != 2) return 2;
	vector_enabled = !!(getauxval(AT_HWCAP) & (1UL << ('V' - 'A')));
	device = open("/dev/zion_enclave", O_RDWR | O_CLOEXEC);
	require(device >= 0, "open actual Zion device");
	create.min_pages = 64;
	require(!ioctl(device, ZION_IOC_CREATE_ENCLAVE, &create), "allocate own EPM");
	require(create.epm_size >= 0x20000, "bounded EPM size");
	epm = mmap(NULL, create.epm_size, PROT_READ | PROT_WRITE, MAP_SHARED, device, 0);
	require(epm != MAP_FAILED, "map EPM before finalization");
	payload = fopen(argv[1], "rb");
	require(payload != NULL, "open actual bare S payload");
	require(!fseek(payload, 0, SEEK_END), "seek payload");
	payload_size = ftell(payload);
	require(payload_size > 0 && payload_size <= 4096, "bounded code size");
	rewind(payload);
	require(fread(epm, 1, payload_size, payload) == (size_t)payload_size, "copy code");
	fclose(payload);
	canary = (volatile uint64_t *)(epm + 0x8000);
	*canary = 0x504d5043414e4152UL;
	request = create;
	request.utm_size = 4096;
	require(!ioctl(device, ZION_IOC_UTM_INIT, &request), "allocate exact UTM");
	/* After finalize mmap offset0 selects the separate explicit UTM. */
	request.runtime_paddr = create.epm_paddr + 4096;
	request.user_paddr = create.epm_paddr + 8192;
	request.free_paddr = create.epm_paddr + 12288;
	request.free_requested = 4096;
	require(!ioctl(device, ZION_IOC_FINALIZE_ENCLAVE, &request), "real monitor CREATE");
	utm = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, device, 0);
	require((void *)utm != MAP_FAILED, "map explicit UTM");
	utm[0] = vector_enabled;
	utm[1] = utm[2] = 0;
	handler.sa_handler = fault;
	sigemptyset(&handler.sa_mask);
	require(!sigaction(SIGBUS, &handler, NULL) && !sigaction(SIGSEGV, &handler, NULL), "signal recovery");
	expected_fault = 1;
	if (!sigsetjmp(recovery, 1)) { volatile uint64_t value = *canary; (void)value; expected_fault = 0; require(0, "parent read was allowed"); }
	expected_fault = 0;
	printf("[PLANB ENCLAVE GUARD] PARENT_READ_DENIED signal=%d\n", caught_signal);
	expected_fault = 1;
	if (!sigsetjmp(recovery, 1)) { *canary = 0xbadbadUL; expected_fault = 0; require(0, "parent write was allowed"); }
	expected_fault = 0;
	printf("[PLANB ENCLAVE GUARD] PARENT_WRITE_DENIED signal=%d\n", caught_signal);
	run.eid = create.eid;
	operation = ZION_IOC_RUN_ENCLAVE;
	for (count = 0; count < 512; count++) {
		long returned;
		observed_fp = observed_fcsr = 0;
		run.error = (uintptr_t)-1;
		/* Reuse the pinned original project's real FP wrapper, not its
 * crypto/attestation runner. Inner payload writes every FP register. */
		require(native_crypto_fp_guard(NULL, 0x3ff00000000000ffUL, 0x40,
			&observed_fp, &observed_fcsr) == 0, "actual ioctl completed");
		returned = run.error;
		require(observed_fp == 0x3ff00000000000ffUL && observed_fcsr == 0x40, "parent FP/FCSR restored");
		if (!returned) break;
		require(returned == 100002 || returned == 100011, "real stop/interrupt code");
		if (returned == 100011) { edges++; memset((void *)(utm + 16), 0xa5, 128); }
		operation = ZION_IOC_RESUME_ENCLAVE;
	}
	require(count < 512 && !run.error && !run.value && edges == 2 && utm[1] == 2 &&
		utm[2] == 0x50415353UL, "inner canary/GPR/32 FP/RVV continuity");
	printf("[PLANB ENCLAVE GUARD] PRIVATE_CONTEXT_PASS fp=32 rvv=%d calls=%u edges=%u\n",
	       vector_enabled, count + 1, edges);
	request = create;
	require(!ioctl(device, ZION_IOC_DESTROY_ENCLAVE, &request), "destroy actual enclave");
	for (i = 0; i < create.epm_size; i++) require(epm[i] == 0, "entire released EPM zeroed");
	require(ioctl(device, ZION_IOC_RESUME_ENCLAVE, &run) < 0, "stale driver handle rejected");
	puts("[PLANB ENCLAVE GUARD] DESTROY_ZERO_AND_STALE_HANDLE_PASS");
	munmap((void *)utm, 4096);
	munmap(epm, create.epm_size);
	close(device);
	return 0;
}
