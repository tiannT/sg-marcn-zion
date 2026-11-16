// SPDX-License-Identifier: GPL-2.0
/* Bounded test module for the actual S-mode -> M-mode enclave boundary.
 * No device IO, persistent writes, arbitrary addresses or external inputs.
 */
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/preempt.h>
#include <linux/io.h>
#include <asm/fpu.h>
#include <asm/vector.h>
#include <asm/sbi.h>

#define EXT 0x08424b45UL
struct region { unsigned long base, size; };
struct create_args {
	struct region epm, utm;
	unsigned long runtime, user, free, free_requested;
};
extern const unsigned char audit_payload_begin[], audit_payload_end[];
extern long native_crypto_fp_guard(void *, u64, u64, u64 *, u64 *);
extern void planb_parent_vector_set(void);
extern int planb_parent_vector_check(void *);
static struct sbiret result;
static unsigned long handle, function;
static bool vectors;
static bool last_passed;
module_param(last_passed, bool, 0444);
MODULE_PARM_DESC(last_passed, "All real direct-SBI context and zeroing checks passed");
static unsigned char vector_buffer[16384] __aligned(16);

long native_crypto_run(void *unused);
long native_crypto_run(void *unused)
{
	(void)unused;
	if (vectors) planb_parent_vector_set();
	result = sbi_ecall(EXT, function, handle, 0, 0, 0, 0, 0);
	if (vectors && planb_parent_vector_check(vector_buffer)) return -EIO;
	return 0;
}

static int __init audit_init(void)
{
	const unsigned int order = 6;
	struct page *epm;
	unsigned long utm_page, *utm, size = PAGE_SIZE << order;
	struct create_args args;
	struct sbiret created, destroyed;
	unsigned int calls, edges = 0;
	u64 observed_fp, observed_fcsr;
	int status = -EIO;
	bool protected = false;
	long checked;
	if (audit_payload_end - audit_payload_begin > PAGE_SIZE ||
	    !kernel_fpu_available()) return -ENODEV;
	epm = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (!epm) return -ENOMEM;
	utm_page = get_zeroed_page(GFP_KERNEL);
	if (!utm_page) { __free_pages(epm, order); return -ENOMEM; }
	utm = (void *)utm_page;
	vectors = has_vector();
	memcpy(page_address(epm), audit_payload_begin, audit_payload_end - audit_payload_begin);
	*(u64 *)((char *)page_address(epm) + 0x8000) = 0x504d5043414e4152UL;
	args.epm = (struct region){page_to_phys(epm), size};
	args.utm = (struct region){virt_to_phys((void *)utm_page), PAGE_SIZE};
	args.runtime = args.epm.base + PAGE_SIZE;
	args.user = args.epm.base + 2 * PAGE_SIZE;
	args.free = args.epm.base + 3 * PAGE_SIZE;
	args.free_requested = PAGE_SIZE;
	created = sbi_ecall(EXT, 2001, (unsigned long)&args, 0, 0, 0, 0, 0);
	if (created.error) { pr_err("[PLANB ENCLAVE AUDIT] CREATE failed %ld\n", created.error); goto out; }
	protected = true;
	handle = created.value;
	utm[0] = vectors;
	function = 2003;
	/* Hold the kernel's real FP/vector ownership across direct SBI calls.
 * Unlike a user ioctl boundary, these S-mode registers must be restored. */
	kernel_fpu_begin();
	if (vectors) kernel_vector_begin();
	for (calls = 0; calls < 512; calls++) {
		observed_fp = observed_fcsr = 0;
		checked = native_crypto_fp_guard(NULL, 0x3ff00000000000ffUL, 0x40,
			&observed_fp, &observed_fcsr);
		if (checked || observed_fp != 0x3ff00000000000ffUL || observed_fcsr != 0x40) break;
		if (!result.error) {
			if (!result.value && edges == 2 && utm[1] == 2 && utm[2] == 0x50415353)
				status = 0;
			break;
		}
		if (result.error != 100002 && result.error != 100011) break;
		if (result.error == 100011) edges++;
		function = 2005;
	}
	if (vectors) kernel_vector_end();
	kernel_fpu_end();
	if (status) pr_err("[PLANB ENCLAVE AUDIT] FAIL calls=%u error=%ld value=%ld edges=%u\n",
		calls, result.error, result.value, edges);
	destroyed = sbi_ecall(EXT, 2002, handle, 0, 0, 0, 0, 0);
	if (destroyed.error) {
		/* Never return protected EPM to Linux's allocator on destroy failure.
 * A failing monitor must not turn this self-test into unsafe page reuse. */
		pr_err("[PLANB ENCLAVE AUDIT] destroy failed; protected pages retained\n");
		free_page(utm_page);
		return -EIO;
	}
	protected = false;
	if (memchr_inv(page_address(epm), 0, size)) status = -EIO;
	last_passed = !status;
	if (!status) pr_notice("[PLANB ENCLAVE AUDIT] S_MODE_CONTEXT_PASS rvv=%u fp=1 edges=%u epm_zero=1\n",
		vectors, edges);
out:
	free_page(utm_page);
	if (!protected) __free_pages(epm, order);
	return status;
}
static void __exit audit_exit(void) { }
module_init(audit_init);
module_exit(audit_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Plan B bounded direct SBI FP/RVV enclave context audit");
