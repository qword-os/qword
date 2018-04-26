OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0xa0000000 + 0x100000;

    .text ALIGN(4K) :
    {
        sections_text = . - 0xa0000000;
        KEEP(*(.multiboot))
        *(.text)
        sections_text_end = . - 0xa0000000;
    }

    .data ALIGN(4K) :
    {
        sections_data = . - 0xa0000000;
        *(.data)
        *(.rodata)
        sections_data_end = . - 0xa0000000;
    }

    .bss ALIGN(4K) :
    {
        sections_bss = . - 0xa0000000;
        *(COMMON)
        *(.bss)
        sections_bss_end = . - 0xa0000000;
    }
}
