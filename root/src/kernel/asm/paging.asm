global paging_init
global kernel_cr3

%define kernel_phys_offset 0xffffffffc0000000

section .bss

align 4096

kernel_cr3:
.pml4:
    resb 4096
.pdpt_low:
    resb 4096
.pdpt_hi:
    resb 4096
.pd:
    resb 4096
.pt:
    resb 4096 * 16      ; 16 page tables == 32 MiB mapped
.end:

section .text
bits 32

paging_init:

set_up_page_tables:
    ; zero out page tables
    xor eax, eax
    mov edi, kernel_cr3 - kernel_phys_offset
    mov ecx, (kernel_cr3.end - kernel_cr3) / 4
    rep stosd

    ; set up page tables
    mov eax, 0x03
    mov edi, kernel_cr3.pt - kernel_phys_offset
    mov ecx, 512 * 16
.loop0:
    stosd
    push eax
    xor eax, eax
    stosd
    pop eax
    add eax, 0x1000
    loop .loop0

    ; set up page directories
    mov eax, kernel_cr3.pt - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pd - kernel_phys_offset
    mov ecx, 16
.loop1:
    stosd
    push eax
    xor eax, eax
    stosd
    pop eax
    add eax, 0x1000
    loop .loop1

    ; set up pdpt
    mov eax, kernel_cr3.pd - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pdpt_low - kernel_phys_offset
    stosd
    xor eax, eax
    stosd

    mov eax, kernel_cr3.pd - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pdpt_hi - kernel_phys_offset + 511*8
    stosd
    xor eax, eax
    stosd

    ; set up pml4
    mov eax, kernel_cr3.pdpt_low - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pml4 - kernel_phys_offset
    stosd
    xor eax, eax
    stosd

    mov eax, kernel_cr3.pdpt_low - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pml4 - kernel_phys_offset + 256*8
    stosd
    xor eax, eax
    stosd

    mov eax, kernel_cr3.pdpt_hi - kernel_phys_offset
    or eax, 0x03
    mov edi, kernel_cr3.pml4 - kernel_phys_offset + 511*8
    stosd
    xor eax, eax
    stosd

enable_paging:
    mov eax, kernel_cr3 - kernel_phys_offset
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret
