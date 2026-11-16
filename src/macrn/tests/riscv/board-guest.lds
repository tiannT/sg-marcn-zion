/* SPDX-License-Identifier: BSD-3-Clause */
OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS {
 . = 0x82000000;
 .text : { *(.text .text.*) }
 .rodata : { *(.rodata .rodata.*) }
 .data : { *(.data .data.*) }
 . = ALIGN(16);
 __guest_bss_start = .;
 .bss (NOLOAD) : { *(.bss .bss.*) }
 . = ALIGN(16);
 __guest_bss_end = .;
 /DISCARD/ : { *(.note.gnu.build-id .eh_frame .comment) }
}
ASSERT(__guest_bss_end <= 0x82010000, "Bring-up Guest exceeds its 64 KiB region")
