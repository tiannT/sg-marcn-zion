/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_SG2044_STATIC_H
#define RISCV_SG2044_STATIC_H
#include <types.h>
struct plic_regs;

#if defined(CONFIG_STATIC_DUAL) && defined(CONFIG_PLATFORM_SG2044)
bool sg2044_static_devices_prepare(void);
void sg2044_static_irq_trace(const char *phase, uint32_t source);
void sg2044_static_irq_observe(void);
void sg2044_static_irq_bind(const struct plic_regs *regs);
#else
static inline bool sg2044_static_devices_prepare(void) { return true; }
static inline void sg2044_static_irq_trace(const char *phase, uint32_t source)
{
	(void)phase;
	(void)source;
}
static inline void sg2044_static_irq_observe(void) {}
static inline void sg2044_static_irq_bind(const struct plic_regs *regs) { (void)regs; }
#endif
#endif
