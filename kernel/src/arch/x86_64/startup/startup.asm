; This file contains the code that is going to be linked at the beginning of
; the kernel binary.
; It should contain core CPU initialisation routines such as entering
; long mode.

global _start
global cmdline

extern textmodeprint
extern clearscreen
extern check_cpuid
extern check_long_mode
extern paging_init
extern gdt_ptr
extern kmain
extern sections_bss
extern sections_bss_end

section .bss

cmdline resb 2048

section .text
bits 32

_start:
    mov esp, 0xeffff0

    ; mask the PIC right away
    mov al, 0xff
    out 0x21, al
    out 0xa1, al

    ; zero out bss
    mov edi, sections_bss
    mov ecx, sections_bss_end
    sub ecx, sections_bss
    xor eax, eax
    rep stosb

    mov esi, dword [ebx+16]
    mov edi, cmdline
    mov ecx, 2047
  .cpycmdline:
    lodsb
    test al, al
    jz .cpycmdline_out
    stosb
    dec ecx
    jnz .cpycmdline
  .cpycmdline_out:
    xor al, al
    stosb

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
