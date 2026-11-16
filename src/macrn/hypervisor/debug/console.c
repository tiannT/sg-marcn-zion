/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>
#include <uart16550.h>
#include <shell.h>
#include <timer.h>
#include <ticks.h>
#include <logmsg.h>
#include <acrn_hv_defs.h>
#include <asm/guest/vm.h>
#include <console.h>
#ifndef CONFIG_RISCV64
#include <boot.h>
#include <vuart.h>
#else
#include <asm/guest/vuart.h>
#include <asm/sg2044_static.h>
#endif
#include <dbg_cmd.h>

struct hv_timer console_timer;
static spinlock_t console_tx_lock;

#define CONSOLE_KICK_TIMER_TIMEOUT  40UL /* timeout is 40ms*/
/* Switching key combinations for shell and uart console */
#define GUEST_CONSOLE_TO_HV_SWITCH_KEY      0       /* CTRL + SPACE */
uint16_t console_vmid = CONFIG_CONSOLE_DEFAULT_VM;

/* if use INIT to kick pcpu only, if not notification IPI still is used for sharing CPU */
static bool use_init_ipi = false;

bool is_using_init_ipi(void)
{
	return use_init_ipi;
}

#ifndef CONFIG_RISCV64
static void parse_hvdbg_cmdline(void)
{
	const char *start = NULL;
	const char *end = NULL;
	struct acrn_boot_info *abi = get_acrn_boot_info();

	start = abi->cmdline;

	while ((*start) != '\0') {
		while ((*start) == ' ')
			start++;
		if ((*start) != '\0') {
			end = start + 1;
			while ((*end != ' ') && ((*end) != '\0'))
				end++;

			if (!handle_dbg_cmd(start, (int32_t)(end - start))) {
				/* if not handled by handle_dbg_cmd, it can be handled further */
				if (strncmp(start, "USE_INIT_IPI", (size_t)(end - start)) == 0) {
					use_init_ipi = true;
				}
			}
			start = end;
		}
	}

}
#else
#define parse_hvdbg_cmdline()
#endif

void console_init(void)
{
	/*Parse cmdline to get UART setting*/
	parse_hvdbg_cmdline();

	/*
	 * Enable UART as early as possible.
	 * Then we could use printf for debugging on early boot stage.
	 */
	uart16550_init(true);
}

void console_putc(const char *ch)
{
	(void)uart16550_puts(ch, 1U);
}


size_t console_write(const char *s, size_t len)
{
	return  uart16550_puts(s, len);
}

char console_getc(void)
{
	return uart16550_getc();
}

/*
 * @post return != NULL
 */
struct acrn_vuart *vm_console_vuart(struct acrn_vm *vm)
{
	return &vm->vuart[0];
}

/**
 * @pre vu != NULL
 * @pre vu->active == true
 */
static void vuart_console_rx_chars(struct acrn_vuart *vu)
{
	char ch = -1;

	/* Get data from physical uart */
	ch = uart16550_getc();

	if (ch == GUEST_CONSOLE_TO_HV_SWITCH_KEY) {
		/* Switch the console */
		console_vmid = ACRN_INVALID_VMID;
		printf("\r\n\r\n ---Entering ACRN SHELL---\r\n");
	}
	if (ch != (char)(-1)) {
		vuart_putchar(vu, ch);
		vuart_toggle_intr(vu);
	}

}

/**
 * @pre vu != NULL
 */
static void vuart_console_tx_chars(struct acrn_vuart *vu)
{
	uint64_t flags;
	char c;

	/* Timer and VM-exit callers must not reorder bytes dequeued from
	 * the same FIFO on different physical harts.
	 */
	spinlock_irqsave_obtain(&console_tx_lock, &flags);
	c = vuart_getchar(vu);
	while(c != (char)(-1)) {
		printf("%c", c);
		c = vuart_getchar(vu);
	}
	spinlock_irqrestore_release(&console_tx_lock, flags);
}

void console_flush_vuart(struct acrn_vuart *vu)
{
	if (vu != NULL && vu->active)
		vuart_console_tx_chars(vu);
}

static struct acrn_vuart *vuart_console_active(void)
{
	struct acrn_vm *vm = NULL;
	struct acrn_vuart *vu = NULL;

	if (console_vmid < CONFIG_MAX_VM_NUM) {
		vm = get_vm_from_vmid(console_vmid);
		if (!is_paused_vm(vm) && !is_poweroff_vm(vm)) {
			vu = vm_console_vuart(vm);
		} else {
			/* Console vm is invalid, switch back to HV-Shell */
			console_vmid = ACRN_INVALID_VMID;
		}
	}

	return ((vu != NULL) && vu->active) ? vu : NULL;
}

static void console_timer_callback(__unused void *data)
{
	struct acrn_vuart *vu;

#if defined(CONFIG_STATIC_DUAL) && defined(CONFIG_PLATFORM_SG2044)
	/* Sample while Linux is waiting silently, before its I/O timeout.
	 * The observer is Hart0-only and waits for VM0's PLIC to be bound.
	 */
	sg2044_static_irq_observe();
#endif

	/* Kick HV-Shell and Uart-Console tasks */
	vu = vuart_console_active();
	if (vu != NULL) {
		/* serial Console Rx operation */
		vuart_console_rx_chars(vu);
		/* serial Console Tx operation */
		vuart_console_tx_chars(vu);
	} else {
		shell_kick();
	}
#ifdef CONFIG_STATIC_DUAL
	/* Output from protected Linux must be visible without moving the
	 * physical input away from the selected management console.
	 */
	for (uint16_t id = 0; id < 2; ++id) {
		struct acrn_vm *vm = get_vm_from_vmid(id);
		struct acrn_vuart *other = vm_console_vuart(vm);
		if (other != vu && !is_poweroff_vm(vm) && !is_paused_vm(vm))
			console_flush_vuart(other);
	}
#endif
}

void console_setup_timer(void)
{
	uint64_t period_in_cycle, fire_tsc;

	period_in_cycle = TICKS_PER_MS * CONSOLE_KICK_TIMER_TIMEOUT;
	fire_tsc = cpu_ticks() + period_in_cycle;
	initialize_timer(&console_timer,
			console_timer_callback, NULL,
			fire_tsc, period_in_cycle);

	/* Start an periodic timer */
	if (add_timer(&console_timer) != 0) {
		pr_err("Failed to add console kick timer");
	}
}

/* When lapic-pt is enabled for a vcpu working on the pcpu hosting
 * console timer (currently BSP), we utilize vm-exits to drive the console.
 *
 * Note that currently this approach will result in a laggy shell when
 * the number of VM-exits/second is low (which is mostly true when lapic-pt is
 * enabled).
 */
void console_vmexit_callback(struct acrn_vcpu *vcpu)
{
	static uint64_t prev_tsc = 0;
	uint64_t tsc;

	/* console_setup_timer is called on BSP only. */
	if ((pcpuid_from_vcpu(vcpu) == BSP_CPU_ID)
#ifndef CONFIG_MACRN
	    && is_lapic_pt_enabled(vcpu)
#endif
	   ) {
		tsc = cpu_ticks();
		if (tsc - prev_tsc > (TICKS_PER_MS * CONSOLE_KICK_TIMER_TIMEOUT)) {
			console_timer_callback(NULL);
			prev_tsc = tsc;
		}
	}
}

void suspend_console(void)
{
	del_timer(&console_timer);
}

void resume_console(void)
{
	console_setup_timer();
}
