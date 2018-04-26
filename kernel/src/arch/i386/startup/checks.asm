global check_cpuid

extern textmodeprint

%define kernel_phys_offset 0xa0000000

section .data

calls:
    .textmodeprint      dd textmodeprint - kernel_phys_offset

section .text
bits 32
check_cpuid:
    ; Copy flags into eax via the stack.
    pushfd
    pop eax

    ; Store previous state in ecx for comparison later on.
    mov ecx, eax

    ; Flip 21st bit, ID bit.
    xor eax, 1 << 21
    
    push eax
    popfd
    
    pushfd
    pop eax

    push ecx
    popfd

    cmp eax, ecx
    je near .no_cpuid
    ret
.no_cpuid:
    mov esi, .msg - kernel_phys_offset
    call [(calls.textmodeprint) - kernel_phys_offset]
.halt:
    cli
    hlt
    jmp near .halt

.msg db "CPUID not supported", 0
