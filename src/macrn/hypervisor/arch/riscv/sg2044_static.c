/* SPDX-License-Identifier: BSD-3-Clause */
#if defined(CONFIG_STATIC_DUAL) && defined(CONFIG_PLATFORM_SG2044)
#include <types.h>
#include <sprintf.h>
#include <rtl.h>
#include <asm/io.h>
#include <asm/cpu.h>
#include <asm/early_printk.h>
#include <asm/sg2044.h>
#include <asm/sg2044_static.h>
#include <asm/apicreg.h>
#include <asm/static_vm.h>

static uint64_t irq_counts[4][864];
static const struct plic_regs *vm0_plic;

void sg2044_static_irq_bind(const struct plic_regs *regs)
{
	__atomic_store_n(&vm0_plic, regs, __ATOMIC_RELEASE);
}

/* Exact SG2044_EVB / factory 1.4.3 SD3-10 PCD HNI routes. These CPU-side
 * routes are separate from the DWC iATU ranges managed by the Linux driver.
 * Source: SG2044Pkg/Library/PciPlatformLib/DwPcieSetSlaveMap().
 */
struct pcie_route {
	uint64_t control;
	uint32_t domain, up_start, up_end, down;
};
static const struct pcie_route routes[] = {
	{ 0x6c00780000UL, 0, 0x00400000, 0x004fffff, 0x4fff4000 },
	{ 0x6c000c0000UL, 2, 0x00500000, 0x005fffff, 0x5fff5000 },
	{ 0x6c04780000UL, 4, 0x00780000, 0x007bffff, 0x2fff2000 },
	{ 0x6c040c0000UL, 6, 0x007c0000, 0x007fffff, 0x3fff3000 },
	{ 0x6c08780000UL, 8, 0x00600000, 0x0067ffff, 0x1fff1000 },
};

static uint32_t reg_read(uint64_t address)
{
	return mmio_readl((const void *)address);
}

static void reg_write(uint64_t address, uint32_t value)
{
	mmio_writel(value, (void *)address);
}

bool sg2044_static_devices_prepare(void)
{
	char line[220];
	uint32_t i, context, word;
	for (i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
		const struct pcie_route *route = &routes[i];
		uint32_t start = reg_read(route->control + 0xc64);
		uint32_t end = reg_read(route->control + 0xc68);
		uint32_t down = reg_read(route->control + 0xc6c);
		if (start == 0xffffffffU || end == 0xffffffffU || down == 0xffffffffU)
			return false;
		reg_write(route->control + 0xc64, route->up_start);
		reg_write(route->control + 0xc68, route->up_end);
		reg_write(route->control + 0xc6c, route->down);
		if (reg_read(route->control + 0xc64) != route->up_start ||
		    reg_read(route->control + 0xc68) != route->up_end ||
		    reg_read(route->control + 0xc6c) != route->down)
			return false;
		snprintf(line, sizeof(line),
			"[PCI] DOMAIN%u HNI_READBACK_OK old=%x:%x/%x new=%x:%x/%x\n",
			route->domain, start, end, down, route->up_start, route->up_end, route->down);
		early_printk(line);
	}
	/* Start from owned, masked physical contexts. Guest S contexts are
	 * virtual: physical device interrupts must first enter the M monitor.
	 * Other hardware harts stay parked, and cannot consume a guest IRQ.
	 */
	for (context = 0; context < 128; ++context) {
		for (word = 0; word < 27; ++word) {
			uint64_t address = CONFIG_PLIC_BASE + 0x2000 + context * 0x80 + word * 4;
			reg_write(address, 0);
			if (reg_read(address) != 0) return false;
		}
		reg_write(CONFIG_PLIC_BASE + 0x200000 + context * 0x1000, 7);
	}
	for (i = 1; i < 864; ++i) reg_write(CONFIG_PLIC_BASE + i * 4, 0);
	for (i = 0; i < CONFIG_NR_CPUS; ++i) {
		uint64_t address = CONFIG_PLIC_BASE + 0x200000 + (2 * i) * 0x1000;
		reg_write(address, 0);
		if (reg_read(address) != 0) return false;
	}
	snprintf(line, sizeof(line), "[IRQ] PHYSICAL_CONTEXTS_RESET_OK THEAD_CTRL=%x M0_THR=%x S0_THR=%x\n",
		reg_read(CONFIG_PLIC_BASE + 0x1ffffc), reg_read(CONFIG_PLIC_BASE + 0x200000),
		reg_read(CONFIG_PLIC_BASE + 0x201000));
	early_printk(line);
	early_printk("[DIAG] NVME_IRQ_V1: counters and queue metadata at 0/10/30/60/120 seconds\n");
	return true;
}

/* Limit each source/stage independently so one pending source cannot hide
 * the next device. The global saturating budget still bounds serial traffic.
 */
void sg2044_static_irq_trace(const char *phase, uint32_t source)
{
	static uint32_t lines;
	static uint32_t per_stage[4][864];
	static const char *const stages[] = { "M_CLAIM", "S_INJECT", "V_CLAIM", "V_COMPLETE" };
	uint32_t stage, count;
	char line[150];
	if (!source || source >= 864) return;
	for (stage = 0; stage < 4; ++stage)
		if (!strcmp(phase, stages[stage])) break;
	if (stage == 4) return;
	/* Count every source and every event even after the text budget ends.
	 * S_INJECT counts selection checks, not separate Guest deliveries.
	 */
	__atomic_fetch_add(&irq_counts[stage][source], 1, __ATOMIC_RELAXED);
	if (!((source >= 352 && source < 856) || source == 64 || source == 65 ||
	      source == 73 || source == 74 || source == 125)) return;
	count = __atomic_load_n(&per_stage[stage][source], __ATOMIC_RELAXED);
	do {
		if (count >= 2) return;
	} while (!__atomic_compare_exchange_n(&per_stage[stage][source], &count, count + 1,
		false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
	count = __atomic_load_n(&lines, __ATOMIC_RELAXED);
	do {
		if (count >= 96) return;
	} while (!__atomic_compare_exchange_n(&lines, &count, count + 1, false,
		__ATOMIC_RELAXED, __ATOMIC_RELAXED));
	snprintf(line, sizeof(line), "[IRQ] %s source=%u hart=%lu mie=%lx mip=%lx\n", phase, source,
		cpu_csr_read(mhartid), cpu_csr_read(mie), cpu_csr_read(mip));
	early_printk(line);
}

static void observe_irq_source(uint32_t source, const struct plic_regs *regs)
{
	uint32_t bit = 1U << (source & 31), word = source >> 5;
	char line[260];
	snprintf(line, sizeof(line), "[IRQSTATE] source=%u M=%lu CHECK=%lu VCLAIM=%lu VCOMPLETE=%lu P=%u C=%u E=%u PRI=%u THR=%u PHY_PRI=%u\n",
		source, __atomic_load_n(&irq_counts[0][source], __ATOMIC_RELAXED),
		__atomic_load_n(&irq_counts[1][source], __ATOMIC_RELAXED),
		__atomic_load_n(&irq_counts[2][source], __ATOMIC_RELAXED),
		__atomic_load_n(&irq_counts[3][source], __ATOMIC_RELAXED),
		!!(regs->pending[word] & bit), !!(regs->claimed[word] & bit),
		!!(regs->enable[1][word] & bit), regs->source_priority[source],
		regs->target_priority[1], reg_read(CONFIG_PLIC_BASE + source * 4));
	early_printk(line);
}

static uint64_t read_pair(uint64_t address)
{
	return (uint64_t)reg_read(address) | ((uint64_t)reg_read(address + 4) << 32);
}

static void observe_nvme_queues(uint64_t bar)
{
	uint32_t cc = reg_read(bar + 0x14), csts = reg_read(bar + 0x1c);
	uint32_t aqa = reg_read(bar + 0x24), entries = (aqa & 0xfff) + 1;
	uint64_t asq = read_pair(bar + 0x28), acq = read_pair(bar + 0x30);
	uint32_t i, reports = 0;
	char line[260];
	snprintf(line, sizeof(line), "[NVME] CC=%x CSTS=%x INTMS=%x AQA=%x ASQ=%lx ACQ=%lx\n",
		cc, csts, reg_read(bar + 0xc), aqa, asq, acq);
	early_printk(line);
	if (!(cc & 1) || !(csts & 1) || (csts & 2) || (asq & 0xfff)) return;
	if (entries > 64) entries = 64;
	if (!static_vm_ram_range(0, asq, (uint64_t)entries * 64)) {
		early_printk("[NVME] ASQ_NOT_IN_VM0_RAM; no queue memory read\n");
		return;
	}
	/* These are command metadata only. Never follow a data PRP, execute a
	 * command, ring a doorbell, or alter the queue/interrupt/controller.
	 */
	for (i = 0; i < entries && reports < 4; ++i) {
		uint64_t command = asq + i * 64;
		uint32_t opcode = reg_read(command) & 0xff;
		uint32_t dw10, dw11, count, j;
		uint64_t queue;
		if (opcode != 5 && opcode != 1) continue;
		dw10 = reg_read(command + 40);
		if ((dw10 & 0xffff) != 1) continue;
		dw11 = reg_read(command + 44);
		queue = read_pair(command + 24);
		++reports;
		if (opcode == 1) {
			snprintf(line, sizeof(line), "[NVME] CREATE_SQ1 slot=%u QSIZE=%u CQID=%u SQ=%lx\n",
				i, (dw10 >> 16) + 1, dw11 >> 16, queue);
			early_printk(line);
			continue;
		}
		snprintf(line, sizeof(line), "[NVME] CREATE_CQ1 slot=%u QSIZE=%u PC=%u IEN=%u IV=%u CQ=%lx\n",
			i, (dw10 >> 16) + 1, dw11 & 1, (dw11 >> 1) & 1, dw11 >> 16, queue);
		early_printk(line);
		if (!(dw11 & 1)) {
			early_printk("[NVME] CQ_USES_PRP_LIST; list not followed\n");
			continue;
		}
		count = (dw10 >> 16) + 1;
		if (count > 4) count = 4;
		if ((queue & 0xfff) || !static_vm_ram_range(0, queue, (uint64_t)count * 16)) {
			early_printk("[NVME] CQ_NOT_IN_VM0_RAM; no completion memory read\n");
			continue;
		}
		for (j = 0; j < count; ++j) {
			uint32_t dw2 = reg_read(queue + j * 16 + 8);
			uint32_t dw3 = reg_read(queue + j * 16 + 12);
			snprintf(line, sizeof(line), "[NVME] CQ1[%u] SQID=%u SQHEAD=%u CID=%u STATUS=%x PHASE=%u\n",
				j, dw2 >> 16, dw2 & 0xffff, dw3 & 0xffff, dw3 >> 16, (dw3 >> 16) & 1);
			early_printk(line);
		}
	}
	if (!reports) early_printk("[NVME] Q1_CREATE_NOT_IN_CURRENT_ASQ; ring may have wrapped\n");
}

static void observe_nvme_config(void)
{
	const uint64_t config = 0x5000000000UL;
	uint32_t identity = reg_read(config);
	uint32_t command, pin, cap, i;
	char line[220];
	/* This is the supported WD controller. Do not touch endpoint
	 * memory before its config identity, BAR and enable state are valid.
	 */
	if (identity != 0x504515b7U) {
		snprintf(line, sizeof(line), "[NVME] CONFIG_NOT_READY id=%x\n", identity);
		early_printk(line);
		return;
	}
	command = reg_read(config + 4) & 0xffff;
	pin = (reg_read(config + 0x3c) >> 8) & 0xff;
	snprintf(line, sizeof(line), "[NVME] PCI_CMD=%x INTX_DISABLE=%u PIN=%u CTRL_SIG=%x CTRL_IRQ_EN=%x\n",
		command, (command >> 10) & 1, pin,
		/* The OF Linux mapper adds 0xc00 to the PCD ctrl_base. */
		reg_read(0x6c000c0c48UL), reg_read(0x6c000c0ca0UL));
	early_printk(line);
	cap = reg_read(config + 0x34) & 0xfc;
	for (i = 0; i < 48 && cap >= 0x40 && cap <= 0xf8; ++i) {
		uint32_t header = reg_read(config + cap);
		if ((header & 0xff) == 0x11) {
			uint32_t control = header >> 16;
			uint32_t table = reg_read(config + cap + 4);
			uint32_t low = reg_read(config + 0x10);
			uint32_t high = (low & 6) == 4 ? reg_read(config + 0x14) : 0;
			uint64_t bar = ((uint64_t)high << 32) | (low & ~0xfU);
			uint32_t offset = table & ~7U;
			uint32_t vector;
			snprintf(line, sizeof(line), "[NVME] MSIX_ENABLED=%u FUNCTION_MASK=%u VECTORS=%u TABLE=%x BAR0=%lx\n",
				(control >> 15) & 1, (control >> 14) & 1,
				(control & 0x7ff) + 1, table, bar);
			early_printk(line);
			if ((command & 2) && !(low & 1) && bar >= 0x50000000UL && bar <= 0x5fffc000UL)
				observe_nvme_queues(bar);
			if (!(command & 2) || !(control & 0x8000) || (control & 0x7ff) < 1 ||
			    (table & 7) || (low & 1) || bar < 0x50000000UL ||
			    bar > 0x5fffc000UL || offset > 0x3fe0) return;
			for (vector = 0; vector < 2; ++vector) {
				uint64_t entry = bar + offset + vector * 16;
				uint32_t address_low = reg_read(entry);
				uint32_t address_high = reg_read(entry + 4);
				uint32_t data = reg_read(entry + 8);
				uint32_t mask = reg_read(entry + 12);
				uint32_t source = 0;
				if (!address_high && address_low >= 0x7ee00000U &&
				    address_low < 0x7ee00040U && !(address_low & 3) && data < 32) {
					source = 352 + ((address_low - 0x7ee00000U) / 4) * 32 + data;
					if (source >= 856) source = 0;
				}
				snprintf(line, sizeof(line), "[NVME] VECTOR%u addr=%x:%x data=%x masked=%u plic_source=%u\n",
					vector, address_high, address_low, data, mask & 1, source);
				early_printk(line);
			}
			return;
		}
		cap = (header >> 8) & 0xfc;
	}
	early_printk("[NVME] MSIX_CAP_NOT_FOUND\n");
}

void sg2044_static_irq_observe(void)
{
	static uint64_t first;
	static uint32_t samples;
	static const uint32_t at_seconds[] = { 0, 10, 30, 60, 120 };
	static const uint32_t watched[] = { 42, 64, 65, 73, 74, 125 };
	const struct plic_regs *regs = __atomic_load_n(&vm0_plic, __ATOMIC_ACQUIRE);
	uint64_t now;
	uint32_t word, i, extra = 0;
	char line[230];
	if (cpu_csr_read(mhartid) != 0) return;
	if (!regs || samples >= sizeof(at_seconds) / sizeof(at_seconds[0])) return;
	now = cpu_csr_read(time);
	if (!samples) first = now;
	if (now - first < (uint64_t)at_seconds[samples] * CONFIG_TIMEBASE_HZ) return;
	++samples;
	snprintf(line, sizeof(line), "[IRQ] SNAPSHOT%u elapsed=%lus hart=0 mie=%lx mip=%lx sie=%lx sstatus=%lx M_THR=%x S_THR=%x CTRL=%x\n",
		samples, (now - first) / CONFIG_TIMEBASE_HZ, cpu_csr_read(mie), cpu_csr_read(mip),
		cpu_csr_read(sie), cpu_csr_read(sstatus),
		reg_read(CONFIG_PLIC_BASE + 0x200000), reg_read(CONFIG_PLIC_BASE + 0x201000),
		reg_read(CONFIG_PLIC_BASE + 0x1ffffc));
	early_printk(line);
	for (word = 2; word < 27; ++word) {
		uint32_t pending, enabled, s_enabled;
		if (word > 3 && word < 11) continue;
		pending = reg_read(CONFIG_PLIC_BASE + 0x1000 + word * 4);
		enabled = reg_read(CONFIG_PLIC_BASE + 0x2000 + word * 4);
		s_enabled = reg_read(CONFIG_PLIC_BASE + 0x2080 + word * 4);
		if (!pending && word != 2 && word != 3 && word != 11) continue;
		snprintf(line, sizeof(line), "[IRQ] WORD%u pending=%x M_ENABLE=%x S_ENABLE=%x\n",
			word, pending, enabled, s_enabled);
		early_printk(line);
	}
	for (i = 0; i < sizeof(watched) / sizeof(watched[0]); ++i)
		observe_irq_source(watched[i], regs);
	for (i = 1; i < 864 && extra < 8; ++i) {
		uint32_t j;
		for (j = 0; j < sizeof(watched) / sizeof(watched[0]); ++j)
			if (i == watched[j]) break;
		if (j < sizeof(watched) / sizeof(watched[0])) continue;
		if (!__atomic_load_n(&irq_counts[0][i], __ATOMIC_RELAXED) &&
		    !((regs->pending[i >> 5] | regs->claimed[i >> 5]) & (1U << (i & 31)))) continue;
		observe_irq_source(i, regs);
		++extra;
	}
	/* The first snapshot precedes PCI enumeration; later samples may
	 * inspect the configured endpoint without programming any registers.
	 */
	if (samples > 1) observe_nvme_config();
	/* Read only iATU configuration; never claim an interrupt or read
	 * clear-on-read device counters while observing a running Guest.
	 */
	for (word = 0; word < 6; ++word) {
		uint64_t address = 0x6c08700000UL + word * 0x200;
		snprintf(line, sizeof(line), "[PCI] DOMAIN8 OB%u ctrl=%x/%x base=%x:%x limit=%x:%x target=%x:%x\n",
			word, reg_read(address), reg_read(address + 4),
			reg_read(address + 12), reg_read(address + 8),
			reg_read(address + 32), reg_read(address + 16),
			reg_read(address + 24), reg_read(address + 20));
		early_printk(line);
	}
}
#endif
