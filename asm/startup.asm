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
extern gdt_ptr_lowerhalf
extern kmain
extern sections_bss
extern sections_bss_end

%define kernel_phys_offset 0xffffffffc0000000

section .bss

cmdline resb 2048

section .text
bits 32

_start:
    mov esp, 0xeffff0

    ; zero out bss
    mov edi, sections_bss
    mov ecx, sections_bss_end
    sub ecx, sections_bss
    xor eax, eax
    rep stosb

    mov esi, dword [ebx+16]
    mov edi, cmdline - kernel_phys_offset
    mov ecx, 2047
  .cpycmdline:
    lodsb
    stosb
    test al, al
    jz near .cpycmdline_out
    dec ecx
    jnz near .cpycmdline
  .cpycmdline_out:

    call near clearscreen

    call near check_cpuid
    call near check_long_mode

    call near paging_init

    lgdt [gdt_ptr_lowerhalf - kernel_phys_offset]

    jmp 0x08:.long_mode_init - kernel_phys_offset

  .long_mode_init:
    bits 64
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Jump to the higher half
    mov rax, .higher_half
    jmp rax

  .higher_half:
    mov rsp, kernel_phys_offset + 0xeffff0

    lgdt [gdt_ptr]

    call kmain

.halt:
    cli
    hlt
    jmp .halt
