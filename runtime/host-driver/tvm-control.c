/* SPDX-License-Identifier: GPL-2.0 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "native_uapi.h"
int main(int argc,char **argv)
{
    struct zion_native_info info;
    struct zion_native_read request;
    char *end;
    uint64_t pages;
    int fd = open("/dev/tvm",O_RDWR|O_CLOEXEC), result;
    if (fd < 0) { perror("/dev/tvm");return 1; }
    if (ioctl(fd,ZION_NATIVE_INFO,&info) || info.abi != ZION_NATIVE_ABI) { perror("native ABI");close(fd);return 1; }
    if (argc == 2 && !strcmp(argv[1],"--info")) {
        printf("{\"abi\":%u,\"protected_addr\":\"0x%llx\",\"private_bytes\":%llu,\"faults\":%llu,\"tamper\":%u,\"used\":%u}\n",
            info.abi,info.protected_addr,info.private_bytes,info.faults,info.tamper_armed,info.tamper_used);
        close(fd);return 0;
    }
    if (argc == 3 && !strcmp(argv[1],"tvm")) {
        errno=0;pages=strtoull(argv[2],&end,0);
        if (errno || *end || !pages || pages > (info.private_bytes-0x80000)/4096) { close(fd);return 2; }
        printf("Trusted pool ready: address=0x%llx pages=%" PRIu64 "\n",info.protected_addr,pages);
        close(fd);return 0;
    }
    if (argc == 4 && !strcmp(argv[1],"phys_memory_access") && !strcmp(argv[2],"0")) {
        errno=0;request.physical_address=strtoull(argv[3],&end,0);
        if (errno || *end || request.physical_address!=info.protected_addr) { close(fd);return 2; }
        result=ioctl(fd,ZION_NATIVE_READ,&request);
        if (result) perror("phys_memory_access");
        close(fd);return result ? 1 : 0;
    }
    fprintf(stderr,"Usage: %s tvm PAGES | phys_memory_access 0 ADDRESS | --info\n",argv[0]);
    close(fd);return 2;
}
