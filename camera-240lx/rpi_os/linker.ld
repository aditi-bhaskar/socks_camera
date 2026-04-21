ENTRY(_start)

KERNEL_VBASE = 0xC0000000;

SECTIONS
{
    /* Load ARM code at paddr 0x8000, vaddr 0xC0008000 */
    . = 0x8000 + KERNEL_VBASE;

    .text : AT(ADDR(.text) - KERNEL_VBASE)
    {
        __code_start__ = .;
        KEEP(*(.text.boot)) /* Make sure startup code isn't optimized away */
        *(.text*)
        . = ALIGN(16);
        __code_end__ = .;
    }

    /* Read-only data */
    . = ALIGN(16);
    .rodata : AT(ADDR(.rodata) - KERNEL_VBASE)
    {
        *(.rodata*)
        . = ALIGN(16);
    }

    /* Initialized data */
    . = ALIGN(16);
    .data : AT(ADDR(.data) - KERNEL_VBASE)
    {
        __data_start__ = .;
        *(.data*)
        . = ALIGN(16);
        __data_end__ = .;
    }

    /* Uninitialized data */
    . = ALIGN(16);
    .bss : AT(ADDR(.bss) - KERNEL_VBASE)
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end__ = .;
    }

    /* MMU Translation Table */
    . = ALIGN(0x4000);
    .l1_page_table (NOLOAD) : AT(ADDR(.l1_page_table) - KERNEL_VBASE)
    {
        __l1_page_table_start__ = .;
        *(.l1_page_table)
        __l1_page_table_end__ = .;
    }

    /* Start of usable RAM (aligned to 1MB section) */
    . = ALIGN(0x1000);
    __heap_start__ = .;
}
