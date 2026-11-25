// SPDX-License-Identifier: GPL-2.0
/* Historical parameter ABI only; this module does not implement H or /dev/kvm. */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "native_uapi.h"
static int set_tamper(const char *value, const struct kernel_param *parameter)
{
    unsigned int number;
    int result = kstrtouint(value, 0, &number);
    if (result) return result;
    return zion_native_tamper_set(number);
}
static int get_tamper(char *buffer, const struct kernel_param *parameter)
{
    return scnprintf(buffer, PAGE_SIZE, "%u\n", zion_native_tamper_get());
}
static const struct kernel_param_ops tamper_ops = { .set=set_tamper, .get=get_tamper };
module_param_cb(zion_vcpu_tamper, &tamper_ops, NULL, 0600);
MODULE_PARM_DESC(zion_vcpu_tamper,"One-shot S3 test controlled and consumed by the native monitor");
static int __init compat_init(void) { return 0; }
static void __exit compat_exit(void) { }
module_init(compat_init);module_exit(compat_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Zion native monitor compatibility for historical KVM parameter path; no H emulation");
MODULE_SOFTDEP("pre: tvm_driver");
