section .text

global _start
_start:
    mov rax, 0x00       ; RAX=0 debug print syscall
    mov rdi, 0          ; RDI: debug type (0=info, 1=warn, 2=err, 3=dbg)
    mov rsi, msg        ; RSI: string
    syscall
    mov rax, 1          ; open syscall
    mov rdi, tty
    mov rsi, 0
    mov rdx, 0
    syscall
    mov rdi, rax
    mov rax, 4          ; write syscall
    mov rsi, msg1
    mov rdx, msg1_len
    syscall
    jmp $

section .data

msg db "hello world!", 0
msg1 db "open and write syscalls working", 0x0a
msg1_len equ $-msg1
tty db "/dev/tty", 0
