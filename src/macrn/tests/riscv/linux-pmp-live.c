/* SPDX-License-Identifier: BSD-3-Clause */
/* Linux requests fixed, real U-mode probes; the monitor restores its context. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <asm/pmp_live.h>

static volatile uint64_t *audit, *shared;
static unsigned int vm;
static uint64_t peer_pa;
static void fence(void) { asm volatile("fence rw,rw" ::: "memory"); }
static uint64_t ld(const volatile void *p)
{
    uint64_t v;
    asm volatile(".option push\n.option norvc\nld %0,0(%1)\n.option pop"
                 : "=r"(v) : "r"(p) : "memory");
    return v;
}
static void command(uint64_t op)
{
    volatile void *p = (volatile char *)audit + PMP_LIVE_COMMAND;
    asm volatile(".option push\n.option norvc\nsd %0,0(%1)\n.option pop"
                 :: "r"(op), "r"(p) : "memory");
}
static void pause_tick(void)
{
    struct timespec delay = { .tv_nsec = 100000000 };
    nanosleep(&delay, NULL);
}
static uint64_t now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000UL + t.tv_nsec;
}
static int preflight(void)
{
    unsigned char abi[4];
    FILE *f = fopen(PMP_LIVE_ABI_PROPERTY, "rb");
    int fd;
    if (!f || fread(abi, 1, 4, f) != 4 || abi[0] || abi[1] || abi[2] || abi[3] != PMP_LIVE_ABI) {
        if (f) fclose(f);
        fprintf(stderr, "[PMP-LIVE] NEED_B2_FIRMWARE; no /dev/mem access attempted\n");
        return -1;
    }
    fclose(f);
    fd = open("/dev/mem", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("/dev/mem"); return -1; }
    /* No executable or foreign-RAM mapping: original Linux's executable
     * /dev/mem path can dereference invalid struct page metadata.
     */
    audit = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PMP_LIVE_AUDIT);
    shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, PMP_LIVE_SHARED);
    close(fd);
    if (audit == MAP_FAILED || shared == MAP_FAILED) {
        perror("[PMP-LIVE] MAPPING_DENIED; no PMP pass"); return -1;
    }
    if (ld(audit) != PMP_LIVE_MAGIC || ld(audit+1) != PMP_LIVE_ABI || ld(audit+2)>1) return -1;
    vm = ld(audit+2); peer_pa = ld(audit+4);
    for (unsigned int op=4;op<=6;op++) {
        uint64_t state;
        unsigned int retry=0;
        do { command(op); state=ld(audit+14); } while (state==3 && ++retry<100);
        if (state!=5 || ld(audit+15)!=3) {
            fprintf(stderr,"[PMP-LIVE VM%u] POSITIVE_CONTROL_FAILED op=%u state=%" PRIu64 "\n",vm,op,state);
            return -1;
        }
    }
    printf("[PMP-LIVE VM%u] POSITIVE_CONTROLS_OK IRQ_RETRIES=%" PRIu64 "\n",vm,ld(audit+16));
    printf("[PMP-LIVE VM%u] LINUX_READY peer_fixture=%" PRIx64 "\n", vm, peer_pa);
    return 0;
}
static int canary(void)
{
    int ok = ld(audit+15) == 3;
    shared[9+vm] = ok; fence(); return ok;
}
static uint64_t run_probes(void)
{
    const char *names[] = {"read", "write", "execute"};
    const unsigned int causes[] = {5, 7, 1};
    uint64_t mask = 0;
    unsigned int op;
    for (op=0; op<3; ++op) {
        uint64_t before=ld(audit+5), after, state, cause, pa, denied, pc;
        unsigned int tries=0;
        do {
            command(op+1);
            state=ld(audit+14);
        } while (state==3 && ++tries<100); /* IRQ aborted probe; Linux was restored */
        after=ld(audit+5); cause=ld(audit+6); pa=ld(audit+7);
        pc=ld(audit+9); denied=ld(audit+13);
        if (state==1 && after==before+1 && cause==causes[op] &&
            pa==peer_pa+(op==2?8:0) && denied==1 && canary()) {
            mask |= 1UL<<op;
            printf("[PMP-LIVE VM%u] %s PASS monitor_cause=%" PRIu64 " pa=%" PRIx64
                   " pc=%" PRIx64 " LINUX_CONTEXT_RESTORED\n",vm,names[op],cause,pa,pc);
        } else printf("[PMP-LIVE VM%u] %s FAIL state=%" PRIu64 " delta=%" PRIu64
                      " cause=%" PRIu64 " pa=%" PRIx64 " rule_denied=%" PRIu64 "\n",
                      vm,names[op],state,after-before,cause,pa,denied);
    }
    return mask;
}
static void service(void)
{
    uint64_t done=0;
    shared[vm]=PMP_LIVE_MAGIC; fence();
    printf("[PMP-LIVE VM%u] SERVICE_READY\n",vm);
    for (;;) {
        uint64_t request;
        shared[2+vm]++; canary(); fence(); request=shared[4];
        if (request && request!=done) {
            shared[7+vm]=run_probes(); canary();
            shared[5+vm]=request; done=request; fence();
            printf("[PMP-LIVE VM%u] BATCH_DONE mask=%" PRIu64 " CANARIES=%d LINUX_ALIVE\n",
                   vm,shared[7+vm],canary());
        }
        pause_tick();
    }
}
static int run(void)
{
    uint64_t until=now()+10000000000UL, nonce, beat;
    uint64_t peer_before[3],peer_delta[3];
    shared[vm]=PMP_LIVE_MAGIC; canary(); fence();
    while (shared[1]!=PMP_LIVE_MAGIC) {
        if (now()>until) { fprintf(stderr,"[PMP-LIVE] VM1_SERVICE_NOT_READY\n"); return 1; }
        pause_tick();
    }
    for (unsigned int i=0;i<3;i++) peer_before[i]=ld(audit+64+i);
    beat=shared[3]; nonce=now(); shared[4]=nonce; fence();
    shared[7]=run_probes(); shared[5]=nonce; canary(); fence();
    until=now()+10000000000UL;
    while (shared[6]!=nonce || shared[3]<beat+2) {
        shared[2]++;
        if (now()>until) { fprintf(stderr,"[PMP-LIVE] VM1_RESULT_TIMEOUT\n"); return 1; }
        pause_tick();
    }
    for (unsigned int i=0;i<3;i++) peer_delta[i]=ld(audit+64+i)-peer_before[i];
    if (!canary() || !shared[10] || shared[7]!=7 || shared[8]!=7 ||
        peer_delta[0]!=1 || peer_delta[1]!=1 || peer_delta[2]!=1 || ld(audit+67)!=7) {
        fprintf(stderr,"[PMP-LIVE] BATCH_FAILED VM0_mask=%" PRIu64 " VM1_mask=%" PRIu64 "\n",
                shared[7],shared[8]); return 1;
    }
    printf("[PMP-LIVE] PEER_MONITOR_DELTAS=1,1,1 PEER_POSITIVE_CONTROLS=7\n");
    printf("[PMP-LIVE] SIX_CASES_PASSED VM0_LINUX_ALIVE VM1_LINUX_ALIVE PRIVATE_CANARIES_OK\n");
    return 0;
}
int main(int argc,char **argv)
{
    setvbuf(stdout,NULL,_IOLBF,0);
    if (argc==3 && !strcmp(argv[1],"--health-nvme")) {
        void *buffer=NULL;
        int fd=open(argv[2],O_RDONLY | O_DIRECT | O_CLOEXEC);
        unsigned int i;
        if (fd<0 || posix_memalign(&buffer,4096,1048576)) {
            perror("NVMe read preflight"); if (fd>=0) close(fd); return 1;
        }
        for (i=0;i<16;i++) if (pread(fd,buffer,1048576,(off_t)i*1048576)!=1048576) {
            perror("NVMe direct read"); close(fd); free(buffer); return 1;
        }
        memset(buffer,0,1048576);free(buffer);close(fd);
        printf("NVME_READ_EXIT=0 BYTES=16777216\n");return 0;
    }
    if (argc!=2 || (strcmp(argv[1],"--run") && strcmp(argv[1],"--service"))) {
        fprintf(stderr,"Usage: %s --run (VM0) | --service (VM1)\n",argv[0]); return 2;
    }
    if (preflight()) return 2;
    if (!strcmp(argv[1],"--service") && vm==1) service();
    if (!strcmp(argv[1],"--run") && vm==0) return run();
    fprintf(stderr,"[PMP-LIVE] Wrong VM for requested role\n"); return 2;
}
