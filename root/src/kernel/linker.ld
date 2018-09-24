OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0xffffffffc0000000 + 0x100000;

    .text ALIGN(4K) :
    {
        sections_text = . - 0xffffffffc0000000;
        KEEP(*(.multiboot))
        KEEP(*(.text))
        sections_text_end = . - 0xffffffffc0000000;
    }

    .data ALIGN(4K) :
    {
        sections_data = . - 0xffffffffc0000000;
        KEEP(*(.data))
        KEEP(*(.rodata))
        sections_data_end = . - 0xffffffffc0000000;
    }

    .bss ALIGN(4K) :
    {
        sections_bss = . - 0xffffffffc0000000;
        KEEP(*(COMMON))
        KEEP(*(.bss))
        sections_bss_end = . - 0xffffffffc0000000;
    }
}
