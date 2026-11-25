/* SPDX-License-Identifier: BSD-3-Clause */
OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS {
 . = 0x80200000;
 .text : { *(.text) }
 .rodata : { *(.rodata) }
 .bss (NOLOAD) : { *(.bss) }
 /DISCARD/ : { *(.note.gnu.build-id .eh_frame) }
}
