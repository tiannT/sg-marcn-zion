/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef __RISCV_SG2044_H__
#define __RISCV_SG2044_H__

/* SG2044_EVB: values recovered from the board's 1.4.3 ZSBL FDT. */
#define BOARD_HV_RAM_START         0x81000000UL
#define BOARD_HV_RAM_SIZE          0x1f7f000000UL
#define BOARD_HV_DEVICE_START      0x00000000UL
#define BOARD_HV_DEVICE_SIZE       0x8000000000UL
#define SOS_DEVICE_MMIO_START      BOARD_HV_DEVICE_START
#define SOS_DEVICE_MMIO_SIZE       BOARD_HV_DEVICE_SIZE

#define CONFIG_UART_BASE          0x7030001000UL
#define CONFIG_UART_SIZE          0x1000UL
#define CONFIG_UART_REG_SHIFT     2
#define CONFIG_UART_REG_WIDTH     4
#define CONFIG_PLIC_BASE          0x6d40000000UL
#define CONFIG_PLIC_SIZE          0x04000000UL
#define CONFIG_CLINT_BASE         0x6d44000000UL
#define CONFIG_CLINT_TM_BASE      0x6d44004000UL
#define CONFIG_CLINT_SIZE         0xc000UL
#define CONFIG_CLINT_MTIME_CSR    1
#define CONFIG_TIMEBASE_HZ        50000000UL
#define CONFIG_BSP_CPU_ID         0
#ifndef CONFIG_NR_CPUS
#define CONFIG_NR_CPUS            64
#endif
#ifndef CONFIG_MAX_VCPU
#define CONFIG_MAX_VCPU           1
#endif
#define CONFIG_SOS_MEM_START      0x80200000UL
#define CONFIG_SOS_MEM_SIZE       0x10000000UL
#define CONFIG_SOS_DTB_BASE       0x88000000UL
#define CONFIG_SOS_DTB_SIZE       0x20000UL
#define CONFIG_SSTC               1
/* No RTC or second guest is assigned during initial board bring-up. */
#define CONFIG_RTC_BASE           0UL
#define CONFIG_RTC_SIZE           0UL
#define CONFIG_PHY_UART_IRQ       42

#endif
