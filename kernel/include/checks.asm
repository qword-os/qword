global check_cpuid

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
    je .no_cpuid
.no_cpuid:
    mov esi, .msg
    jmp error

.msg db "CPUID not supported", 0

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29 ; Check if the LM bit is set in the D register
    jz .no_long_mode
    ret
.no_long_mode:
    mov esi, msg

error:
    call textmodeprint
    jmp .halt
.halt:
    cli
    hlt
    jmp .halt ; Jump right back to this function if the CPU ever wakes up for SMM, an NMI, etc.
