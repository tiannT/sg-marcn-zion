/* SPDX-License-Identifier: BSD-3-Clause */
#define _GNU_SOURCE
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <asm/planb_runtime.h>

static volatile uint32_t *regs;
static uint32_t rd(unsigned off) { uint32_t v = regs[off/4]; __sync_synchronize(); return v; }
static void wr(unsigned off, uint32_t v) { __sync_synchronize(); regs[off/4] = v; __sync_synchronize(); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec+t.tv_nsec/1e9; }
static void status(void)
{
 uint32_t s = rd(12), image = rd(16), error = rd(20), clean = rd(32);
 uint64_t epoch = rd(24) | ((uint64_t)rd(28)<<32);
 uint64_t bytes = rd(40) | ((uint64_t)rd(44)<<32);
 printf("{\"state\":%u,\"image\":%u,\"error\":%u,\"epoch\":%llu,\"clean\":%u,\"stops\":%u,\"private_bytes\":%llu,\"native_compat_abi\":%u}\n",
  s, image, error, (unsigned long long)epoch, clean, rd(36), (unsigned long long)bytes,
  rd(48)==PLANB_NATIVE_COMPAT_MAGIC ? 1U : 0U);
}
static int command(uint32_t cmd, uint32_t expected)
{
 double deadline = now()+120;
 wr(8, cmd);
 for (;;) {
  uint32_t s = rd(12), e = rd(20);
  if (e) { status(); fprintf(stderr,"runtime refused: %s (%u)\n",strerror(e),e); return 1; }
  if (s == expected) { status(); return 0; }
  if (s == PLANB_RT_FAILED || now() > deadline) { status(); return 1; }
  usleep(10000);
 }
}
int main(int argc, char **argv)
{
 uint8_t abi[4];
 int fd, lock, rc = 1;
 uint32_t cmd = 0, expected = 0;
 if (argc == 2 && !strcmp(argv[1], "--status")) { }
 else if (argc == 2 && !strcmp(argv[1], "--stop")) { cmd=PLANB_RT_STOP; expected=PLANB_RT_STOPPED; }
 else if (argc == 3 && !strcmp(argv[1], "--start") &&
  (!strcmp(argv[2], "asterinas") || !strcmp(argv[2], "linux") || !strcmp(argv[2], "enclave"))) {
  cmd = !strcmp(argv[2], "asterinas") ? PLANB_RT_START_ASTERINAS :
        !strcmp(argv[2], "enclave") ? PLANB_RT_START_ENCLAVE : PLANB_RT_START_LINUX;
  expected=PLANB_RT_RUNNING;
 } else { fprintf(stderr,"Usage: %s --status | --stop | --start asterinas|linux|enclave\n",argv[0]); return 2; }
 fd=open("/proc/device-tree/chosen/macrn,planb-runtime-abi",O_RDONLY|O_CLOEXEC);
 if (fd < 0 || read(fd,abi,4)!=4 || memcmp(abi,"\0\0\0\1",4)) {
  fprintf(stderr,"VM0 runtime ABI guard failed; /dev/mem was not opened\n");
  if (fd >= 0) close(fd);
  return 2;
 }
 close(fd);
 lock=open("/run/planb-guestctl.lock",O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
 if (lock<0 || flock(lock,LOCK_EX)) { perror("runtime lock"); return 2; }
 fd=open("/dev/mem",O_RDWR|O_SYNC|O_CLOEXEC);
 if (fd<0) { perror("/dev/mem"); goto out_lock; }
 regs=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,PLANB_RUNTIME_MMIO);
 close(fd);
 if (regs==MAP_FAILED) { perror("mmap"); goto out_lock; }
 if (rd(0)!=PLANB_RUNTIME_MAGIC || rd(4)!=PLANB_RUNTIME_ABI) { fprintf(stderr,"runtime MMIO ABI mismatch\n"); goto out_map; }
 if (cmd) rc=command(cmd,expected);
 else { status(); rc=0; }
 out_map: munmap((void *)regs,4096);
 out_lock: close(lock);
 return rc;
}
