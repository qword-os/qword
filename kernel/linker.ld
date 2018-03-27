OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text ALIGN(4K) :
    {
        sections_text = .;
        *(.multiboot)
        *(.text)
        sections_text_end = .;
    }

    .data ALIGN(4K) :
    {
        sections_data = .;
        *(.data)
        *(.rodata)
        sections_data_end = .;
    }

    .bss ALIGN(4K) :
    {
        sections_bss = .;
        *(COMMON)
        *(.bss)
        sections_bss_end = .;
    }
}
