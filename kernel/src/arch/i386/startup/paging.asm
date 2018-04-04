global paging_init
global kernel_pagemap

section .bss

align 4096

kernel_pagemap:
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
    mov eax, kernel_pagemap
    mov cr3, eax

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

set_up_page_tables:
    ; zero out page tables
    xor eax, eax
    mov edi, kernel_pagemap
    mov ecx, (kernel_pagemap.end - kernel_pagemap) / 4
    rep stosd

    ; set up page tables
    mov eax, 0x03
    mov edi, kernel_pagemap.pt
    mov ecx, 1024 * 8
  .loop0:
    stosd
    add eax, 0x1000
    loop .loop0

    ; set up page directories
    mov eax, kernel_pagemap.pt
    or eax, 0x03
    mov edi, kernel_pagemap.pd
    mov ecx, 8
  .loop1:
    stosd
    add eax, 0x1000
    loop .loop1

    ret
