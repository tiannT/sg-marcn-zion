/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef ZION_NATIVE_UAPI_H
#define ZION_NATIVE_UAPI_H
#include <linux/types.h>
#include <linux/ioctl.h>
#define ZION_NATIVE_ABI 1U
struct zion_native_info {
    __u32 abi, flags;
    __u64 protected_addr, private_bytes, faults;
    __u32 tamper_armed, tamper_used;
};
struct zion_native_read { __u64 physical_address; };
#define ZION_NATIVE_INFO _IOR('Z', 0x70, struct zion_native_info)
#define ZION_NATIVE_READ _IOW('Z', 0x71, struct zion_native_read)
#ifdef __KERNEL__
int zion_native_tamper_set(unsigned int value);
unsigned int zion_native_tamper_get(void);
#endif
#endif
