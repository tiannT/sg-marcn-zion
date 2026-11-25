// SPDX-License-Identifier: GPL-2.0
/* Native Zion compatibility driver. Real S-mode access, PMP fault and fixup. */
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include "native_uapi.h"

#define RUNTIME_BASE 0x93003000UL
#define RUNTIME_MAGIC 0x42525431U
#define COMPAT_MAGIC 0x5a4e4331U
#define AUDIT_BASE 0x92010000UL
#define AUDIT_MAGIC 0x504d504c49564532ULL
static void __iomem *runtime, *audit;
static DEFINE_MUTEX(control_lock);
static u64 audit_read(unsigned int offset)
{
    u64 value;
    /* The read-only audit ABI intentionally accepts one aligned 32-bit LD
     * instruction, rather than arbitrary compressed/unaligned accesses. */
    asm volatile(".option push\n.option norvc\nld %0,0(%1)\n.option pop"
                 : "=r"(value) : "r"(audit + offset) : "memory");
    return value;
}

/* Same architecture exception-table primitive used by Linux maccess.c.
 * Keep the one aligned 64-bit access in this module's own exception table. */
static int protected_load(u64 *destination, const void *source)
{
    pagefault_disable();
    __get_kernel_nofault(destination, source, u64, fault);
    pagefault_enable();
    return 0;
fault:
    pagefault_enable();
    return -EFAULT;
}

int zion_native_tamper_set(unsigned int value)
{
    int error;
    if (value > 1) return -EINVAL;
    mutex_lock(&control_lock);
    writel(value, runtime + 52);
    error = readl(runtime + 56);
    if (!error && readl(runtime + 52) != value) error = EIO;
    mutex_unlock(&control_lock);
    return -error;
}
EXPORT_SYMBOL_GPL(zion_native_tamper_set);

unsigned int zion_native_tamper_get(void) { return readl(runtime + 52); }
EXPORT_SYMBOL_GPL(zion_native_tamper_get);

static long native_ioctl(struct file *file, unsigned int command, unsigned long argument)
{
    struct zion_native_info info = { .abi = ZION_NATIVE_ABI };
    struct zion_native_read request;
    void __iomem *mapping;
    u64 positive = 0x9b17438a2c6150efULL, observed = 0, before, after, canaries;
    int result;
    if (!capable(CAP_SYS_RAWIO)) return -EPERM;
    if (command == ZION_NATIVE_INFO) {
        info.protected_addr = audit_read(32);
        info.private_bytes = readl(runtime + 40) | ((u64)readl(runtime + 44) << 32);
        info.faults = audit_read(40);
        info.tamper_armed = zion_native_tamper_get();
        info.tamper_used = readl(runtime + 60);
        return copy_to_user((void __user *)argument, &info, sizeof(info)) ? -EFAULT : 0;
    }
    if (command != ZION_NATIVE_READ) return -ENOTTY;
    if (copy_from_user(&request, (void __user *)argument, sizeof(request))) return -EFAULT;
    /* Never provide an arbitrary physical read or a caller-supplied mapping. */
    if (request.physical_address != audit_read(32) || (request.physical_address & 4095)) return -EINVAL;
    mutex_lock(&control_lock);
    result = protected_load(&observed, &positive);
    if (result || observed != positive) { result = -EIO; goto unlock; }
    canaries = audit_read(120);
    if (!(canaries & 1)) { result = -EIO; goto unlock; }
    mapping = ioremap(request.physical_address, 4096);
    if (!mapping) { result = -ENOMEM; goto unlock; }
    before = audit_read(40);
    /* protected_load uses the actual Linux exception-table recovery.
     * The monitor reflects the PMP denial; no monitor proxy reads this RAM. */
    result = protected_load(&observed, (const void __force *)mapping);
    after = audit_read(40);
    iounmap(mapping);
    if (result == -EFAULT && after == before + 1 && audit_read(48) == 5 &&
        audit_read(56) == request.physical_address && audit_read(120) == canaries) {
        pr_info("[tvm-driver] protected-memory read blocked: physical address=0x%llx, fault=-14 (recovered)\n",
                request.physical_address);
    } else {
        /* No protected value is copied to userspace even if isolation fails. */
        pr_err("[tvm-driver] protected-memory check failed: result=%d delta=%llu\n",result,after-before);
        result = -EIO;
    }
unlock:
    mutex_unlock(&control_lock);
    return result;
}

static const struct file_operations native_fops = {
    .owner = THIS_MODULE, .unlocked_ioctl = native_ioctl,
    .open = nonseekable_open,
};
static struct miscdevice native_device = {
    .minor = MISC_DYNAMIC_MINOR, .name = "tvm", .fops = &native_fops, .mode = 0600,
};
static int __init native_init(void)
{
    struct device_node *chosen;
    u32 abi;
    int result;
    chosen = of_find_node_by_path("/chosen");
    if (!chosen) return -ENODEV;
    result = of_property_read_u32(chosen, "macrn,planb-runtime-abi", &abi);
    of_node_put(chosen);
    if (result || abi != 1) return -ENODEV;
    runtime = ioremap(RUNTIME_BASE, 4096);
    if (!runtime) return -ENOMEM;
    if (readl(runtime) != RUNTIME_MAGIC || readl(runtime+4) != 1 ||
        readl(runtime+48) != COMPAT_MAGIC) { result = -ENODEV; goto unmap_runtime; }
    audit = ioremap(AUDIT_BASE, 4096);
    if (!audit) { result = -ENOMEM; goto unmap_runtime; }
    if (audit_read(0) != AUDIT_MAGIC || audit_read(8) != 2 || audit_read(16) != 0) {
        result = -ENODEV; goto unmap_audit;
    }
    result = misc_register(&native_device);
    if (result) goto unmap_audit;
    pr_info("[tvm-driver] native monitor ABI ready; device_phys_addr=%llx\n",audit_read(32));
    return 0;
unmap_audit: iounmap(audit);
unmap_runtime: iounmap(runtime);return result;
}
static void __exit native_exit(void)
{
    misc_deregister(&native_device);iounmap(audit);iounmap(runtime);
}
module_init(native_init);module_exit(native_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Zion tvm ABI backed by native PMP protection and real fault recovery");
