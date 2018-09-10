section .text

global _start
_start:
	mov rax, 0x00
    mov rdi, chode
	int 0x80
	jmp $

section .data

chode db "suck on my chode", 0
