section .text

global _start
_start:
    mov rax, 0x00
    mov rdi, msg
jmp $
    syscall
    jmp $

section .data

msg db "hello world!", 0
