section .text

global _start
_start:
	mov rax, 0x00
	int 0x80
	jmp $
