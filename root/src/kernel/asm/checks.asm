global check_cpuid
global check_long_mode

extern textmodeprint

%define kernel_phys_offset 0xffffffffc0000000

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
    call near textmodeprint
.halt:
    cli
    hlt
    jmp near .halt

.msg db "CPUID not supported", 0

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb near .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29 ; Check if the LM bit is set in the D register
    jz near .no_long_mode
    ret
.no_long_mode:
    mov esi, .no_lm_msg - kernel_phys_offset
    call near textmodeprint
.halt:
    cli
    hlt
    jmp near .halt
.no_lm_msg db "Long mode not available, system halted", 0
