global paging_init
global kernel_cr3

%define kernel_phys_offset 0xc0000000

section .bss

align 4096

kernel_cr3:
.pd:
    resb 4096
.pt:
    resb 4096 * 8      ; 8 page tables == 32 MiB mapped
.end:

section .text
bits 32

paging_init:
    call set_up_page_tables
    call enable_paging
    ret

enable_paging:
    mov eax, kernel_cr3 - kernel_phys_offset
    mov cr3, eax

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

set_up_page_tables:
    ; zero out page tables
    xor eax, eax
    mov edi, kernel_cr3 - kernel_phys_offset
    mov ecx, (kernel_cr3.end - kernel_cr3) / 4
    rep stosd

    ; set up page tables
    mov eax, 0x03
    mov edi, kernel_cr3.pt - kernel_phys_offset
    mov ecx, 1024 * 8
.loop0:
    stosd
    add eax, 0x1000
    loop .loop0

    ; set up page directories
    mov eax, kernel_cr3.pt - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pd - kernel_phys_offset
    mov ecx, 8
.loop1:
    stosd
    add eax, 0x1000
    loop .loop1

    mov eax, kernel_cr3.pt - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pd - kernel_phys_offset + (640 + 128) * 4
    mov ecx, 8
.loop2:
    stosd
    add eax, 0x1000
    loop .loop2

    ret
