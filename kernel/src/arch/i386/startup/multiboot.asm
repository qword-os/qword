extern sections_text
extern sections_data_end
extern sections_bss_end
extern _start

%define kernel_phys_offset 0xc0000000

section .multiboot

bits 32
legacy_skip_header:
    jmp _start - kernel_phys_offset

align 4

FLAGS equ 0x00010000

multiboot_header:
    .magic dd 0x1BADB002
    .flags dd FLAGS
    .checksum dd -(0x1BADB002 + FLAGS)
    .header_addr dd multiboot_header - kernel_phys_offset
    .load_addr dd sections_text
    .load_end_addr dd sections_data_end
    .bss_end_addr dd sections_bss_end
    .entry_addr dd _start - kernel_phys_offset
