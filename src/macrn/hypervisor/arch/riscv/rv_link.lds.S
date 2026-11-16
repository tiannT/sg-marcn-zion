#include <asm/cache.h>
#include <asm/page.h>
#include <asm/mem.h>
#undef ENTRY
#undef ALIGN

ENTRY(_start)

PHDRS
{
    text PT_LOAD ;
}

SECTIONS
{
    . = ACRN_VIRT_START;
    .text : {
        _stext = .;
        KEEP(*(.text.entry))
        *(.text .text.*)
        _etext = .;
    } :text = 0x9090

    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) } :text
    .data : {
        __global_pointer$ = . + 0x800;
        *(.data .data.* .sdata .sdata.* .got .got.*)
    } :text

    . = ALIGN(PAGE_SIZE);             /* Init code and data */
    .init.text : {
        *(.init.text)
    } :text

    . = ALIGN(PAGE_SIZE);
    .init.data : {
        *(.init.data)
    } :text

    . = ALIGN(16);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } :text

    . = ALIGN(STACK_SIZE);
    _end = . ;
    /DISCARD/ : { *(.note.gnu.build-id .eh_frame .eh_frame.*) }
}

ASSERT( _end_boot - _start <= PAGE_SIZE, "Boot code is larger than 4K")
#ifdef CONFIG_MACRN
ASSERT(__bss_end <= ACRN_MSTACK_TOP - CONFIG_NR_CPUS * CONFIG_MSTACK_SIZE,
       "Monitor overlaps per-hart M-mode stacks")
#endif
