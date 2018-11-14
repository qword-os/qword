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
    mov rax, 5          ; getauxval syscall
    mov rdi, 10         ; 10 = AT_ENTRY (see src/interrupts/syscalls.c)
    syscall
    mov rax, 6          ; alloc_at syscall
    mov rdi, 0          ; no specific address
    mov rsi, 16         ; 16 pages
    syscall
    mov r8, rax
    mov rax, 6          ; alloc_at syscall
    mov rdi, 0          ; no specific address
    mov rsi, 16         ; 16 pages
    syscall
    mov r9, rax
    mov rax, 6          ; alloc_at syscall
    mov rdi, 0xdead0000 ; allocate at 0xdead0000
    mov rsi, 8          ; 8 pages
    syscall
    mov r10, rax
    jmp $

section .data

msg db "hello world!", 0
msg1 db "open and write syscalls working", 0x0a
msg1_len equ $-msg1
tty db "/dev/tty", 0
