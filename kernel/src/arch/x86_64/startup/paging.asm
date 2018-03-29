global paging_init

extern pml4
extern pdpt
extern pd

section .text
bits 32

paging_init:
    call set_up_page_tables
    call enable_paging
    ret
enable_paging:
    mov eax, pml4
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
set_up_page_tables:
    mov eax, pml4
    or eax, 0b11
    ; Map last entry to pml4 itself.
    mov [pml4 + 511 * 8], eax
    
    ; Map first PML4 entry to PDPT
    mov eax, pdpt
    or eax, 0b11
    mov [pml4], eax
    
    ; Map first PDPT entry to PD
    mov eax, pd
    or eax, 0b11
    mov [pdpt], eax
    
    mov ecx, 0
.identity_map:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    mov [pd + ecx * 8], eax

    inc ecx
    cmp ecx, 512
    jne .identity_map

    ret
