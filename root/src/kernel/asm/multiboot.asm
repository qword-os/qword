extern sections_text
extern sections_data_end
extern sections_bss_end
extern _start

%define kernel_phys_offset 0xffffffffc0000000

section .multiboot

bits 32
legacy_skip_header:
    ; when booted as "legacy" (aka not multiboot), the command line is passed
    ; in ebx, as a pointer to a zero terminated string

    ; move ebx to [ebx+16]
    mov dword [(.fake_multiboot_struct - kernel_phys_offset) + 16], ebx
    mov ebx, .fake_multiboot_struct - kernel_phys_offset

    jmp [multiboot_header.entry_addr - kernel_phys_offset]

align 16
  .fake_multiboot_struct:
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0

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
