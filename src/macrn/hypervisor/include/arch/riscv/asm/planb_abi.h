/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef RISCV_PLANB_ABI_H
#define RISCV_PLANB_ABI_H
#ifdef CONFIG_MACRN
#include <types.h>
#else
#include <stdint.h>
#endif

#define PLANB_ABI 1U
#define PLANB_MAGIC 0x504c414e42494f31ULL
#define PLANB_BLK_MMIO 0x93000000ULL
#define PLANB_NET_MMIO 0x93001000ULL
#define PLANB_CTL_MMIO 0x93002000ULL
#define PLANB_CONSOLE_MMIO 0x93100000ULL
#define PLANB_CONSOLE_BYTES (256U * 1024U)
#define PLANB_FEATURE_MAGIC 0x20440002U
#define PLANB_FEATURE_CONSOLE 1U
#define PLANB_FEATURE_NONBLOCK_IO 2U
#define PLANB_SLOT_BLK 0x92002000ULL
#define PLANB_SLOT_TX 0x9200b000ULL
#define PLANB_SLOT_RX 0x9200c000ULL
#define PLANB_SLOT_HEADER 128U
#define PLANB_BLK_BYTES 32768U
#define PLANB_NET_BYTES 2048U
#define PLANB_DISK_BYTES (512ULL * 1024 * 1024)
#define PLANB_BLK_IRQ 35U
#define PLANB_NET_IRQ 36U
#define PLANB_FREE 0U
#define PLANB_REQUEST 1U
#define PLANB_RESPONSE 2U
#define PLANB_BLK_READ 1U
#define PLANB_BLK_WRITE 2U
#define PLANB_BLK_FLUSH 3U
#define PLANB_NET_TX 4U
#define PLANB_NET_RX 5U

/* The monitor owns request metadata. The backend owns response fields only.
 * Saved-register fields are a sanitized vCPU handoff channel: callee-saved
 * registers must remain zero, and never replace the M-private saved context.
 * Publication uses state release/acquire; response_sequence rejects stale I/O.
 */
struct planb_slot {
	uint64_t sequence;
	uint32_t operation;
	uint32_t length;
	uint64_t sector;
	uint64_t saved_s2_s10[9];
	int32_t status;
	uint32_t response_length;
	uint32_t state;
	uint32_t reserved;
	uint64_t response_sequence;
	uint64_t reserved_tail;
};
_Static_assert(sizeof(struct planb_slot) == PLANB_SLOT_HEADER, "Plan B wire ABI");
#endif
