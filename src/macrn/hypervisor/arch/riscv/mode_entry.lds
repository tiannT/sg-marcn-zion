/* SPDX-License-Identifier: BSD-3-Clause */
OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS {
    . = 0x80000000;
    .text : { *(.text.entry) }
    /DISCARD/ : { *(.note.gnu.build-id .eh_frame .comment) }
}
ASSERT(SIZEOF(.text) <= 0x200000,
       "M-mode entry would overlap the 0x80200000 next-stage image")
