global textmodeprint
global clearscreen

section .text
bits 32

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
