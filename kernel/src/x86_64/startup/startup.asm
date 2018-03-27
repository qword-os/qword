; This file contains the code that is gonna be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode.

global _start

section .text

bits 32

nolongmode:
    call clearscreen
    mov esi, .msg
    call textmodeprint
    .halt:
        cli
        hlt
        jmp .halt

.msg    db  "This CPU does not support long mode.", 0

textmodeprint:
    pusha
    mov edi, 0xb8000
    .loop:
        lodsb
        test al, al
        jz .out
        stosb
        inc edi
        jmp .loop
    .out:
    popa
    ret

clearscreen:
    ; clear screen
    pusha
    mov edi, 0xb8000
    mov ecx, 80*25
    mov al, ' '
    mov ah, 0x17
    rep stosw
    popa
    ret

_start:
    ; check if long mode is present
    mov eax, 0x80000001
    xor edx, edx
    cpuid
    and edx, 1 << 29
    test edx, edx
    jz nolongmode

    call clearscreen
    mov esi, .msg
    call textmodeprint
    .halt:
        cli
        hlt
        jmp .halt

.msg    db "Hello world", 0
