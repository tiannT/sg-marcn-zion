/* SPDX-License-Identifier: BSD-3-Clause */
/* Compact ZSBL/QEMU entry at 0x80000000; keep the 0x80200000 payload intact. */
#include <asm/mem.h>
#include <asm/cpu.h>
#include <asm/bringup_trace.h>
	.section .text.entry,"ax",@progbits
	.global _start
_start:
	li t0, 0x88
	csrc mstatus, t0
	csrw mie, zero
	csrr a0, mhartid
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	la t0, bringup_early_trap
	csrw mtvec, t0
#endif
	li t0, BSP_CPU_ID
	bne a0, t0, wait_relocated
	/* ZSBL may place fw_dynamic_info inside our future runtime/BSS.
	 * Snapshot it before either relocation or BSS clearing destroys it.
	 * Never probe a descriptor outside known readable boot memory.
	 */
	beqz a2, dynamic_saved
	andi t0, a2, 7
	bnez t0, dynamic_saved
	li t0, 0x80000000
	bltu a2, t0, dynamic_rom_check
	li t0, 0x2000000000 - 48
	bgeu a2, t0, dynamic_saved
	j snapshot_dynamic
dynamic_rom_check:
#ifndef CONFIG_PLATFORM_SG2044
	li t0, 0x1000
	bltu a2, t0, dynamic_saved
	li t0, 0x2000 - 48
	bgeu a2, t0, dynamic_saved
	j snapshot_dynamic
#else
	j dynamic_saved
#endif
snapshot_dynamic:
	la t0, dynamic_snapshot
	ld t1, 0(a2)
	sd t1, 0(t0)
	ld t1, 8(a2)
	sd t1, 8(t0)
	ld t2, 16(a2)
	sd t2, 16(t0)
	ld t2, 24(a2)
	sd t2, 24(t0)
	ld t2, 32(a2)
	sd t2, 32(t0)
	li t2, -1
	li t3, 2
	bne t1, t3, snapshot_hart
	ld t2, 40(a2)
snapshot_hart:
	sd t2, 40(t0)
	mv a2, t0
dynamic_saved:
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	BRINGUP_PUTS bringup_entry_message
#endif
	la t0, monitor_blob
	la t1, monitor_blob_end
	li t2, MACRN_RAM_START
copy_monitor:
	bgeu t0, t1, publish_monitor
	ld t3, 0(t0)
	sd t3, 0(t2)
	addi t0, t0, 8
	addi t2, t2, 8
	j copy_monitor
publish_monitor:
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	BRINGUP_PUTS bringup_copied_message
#endif
	fence rw, rw
	la t0, relocated
	li t1, 1
	sd t1, 0(t0)
wait_relocated:
	la t0, relocated
	ld t1, 0(t0)
	beqz t1, wait_relocated
	fence r, rw
	fence.i
	li t0, MACRN_RAM_START
	jr t0
#if defined(CONFIG_BRINGUP_GUEST) || defined(CONFIG_STATIC_DUAL)
	.balign 4
bringup_early_trap:
	BRINGUP_PUTS bringup_trap_message
	BRINGUP_CSR_HEX mcause
	BRINGUP_PUTS bringup_pc_message
	BRINGUP_CSR_HEX mepc
	BRINGUP_PUTS bringup_tval_message
	BRINGUP_CSR_HEX mtval
	BRINGUP_PUTS bringup_newline
bringup_trap_park:
	wfi
	j bringup_trap_park
bringup_entry_message:
	.asciz "[BRINGUP] M_ENTRY_80000000\r\n"
bringup_copied_message:
	.asciz "[BRINGUP] MONITOR_COPY_OK\r\n"
bringup_trap_message:
	.asciz "[BRINGUP] EARLY_M_TRAP cause=0x"
bringup_pc_message:
	.asciz " pc=0x"
bringup_tval_message:
	.asciz " tval=0x"
bringup_newline:
	.asciz "\r\n"
#endif
	.balign 8
relocated:
	.dword 0
dynamic_snapshot:
	.zero 48
monitor_blob:
	.incbin "monitor.bin"
	.balign 8
monitor_blob_end:
