; This file contains the code that is going to be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode.

global _start

extern textmodeprint
extern clearscreen
extern check_cpuid
extern check_long_mode

section .text
bits 32

_start:
    call clearscreen
    call check_cpuid
    call check_long_mode

    mov esi, .msg
    call textmodeprint
.halt:
    cli
    hlt
    jmp .halt

.msg    db "Hello world", 0
