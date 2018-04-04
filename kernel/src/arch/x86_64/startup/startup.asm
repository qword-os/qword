; This file contains the code that is going to be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode.

global _start

extern textmodeprint
extern clearscreen
extern check_cpuid
extern check_long_mode
extern paging_init
extern gdt_ptr
extern kmain

section .text
bits 32

_start:
    mov esp, 0xeffff0

    call clearscreen

    call check_cpuid
    call check_long_mode

    call paging_init

    lgdt [gdt_ptr]

    jmp 0x08:.long_mode_init

.long_mode_init:
bits 64
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call kmain
.halt:
    cli
    hlt
    jmp .halt
