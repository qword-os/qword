global smp_prepare_trampoline
global smp_init_cpu0_local
global smp_check_ap_flag
global smp_get_cpu_number
global smp_get_cpu_kernel_stack

extern gdt_set_fs_base

section .data

%define smp_trampoline_size  smp_trampoline_end - smp_trampoline
smp_trampoline:              incbin "real/smp_trampoline_i386.bin"
smp_trampoline_end:

section .text

bits 32

%define TRAMPOLINE_ADDR     0x1000
%define PAGE_SIZE           4096

smp_prepare_trampoline:
    ; entry point in esp+16, page table in esp+20
    ; stack pointer in esp+24, cpu local in esp+28

    push esi
    push edi
    push ecx

    ; prepare variables
    mov byte [0x510], 0
    mov eax, dword [esp+16]
    mov dword [0x520], eax
    mov eax, dword [esp+20]
    mov dword [0x540], eax
    mov eax, dword [esp+24]
    mov dword [0x550], eax
    mov eax, dword [esp+28]
    call gdt_set_fs_base
    sgdt [0x580]
    sidt [0x590]

    ; Copy trampoline blob to 0x1000
    mov esi, smp_trampoline
    mov edi, TRAMPOLINE_ADDR
    mov ecx, smp_trampoline_size
    rep movsb

    pop ecx
    pop edi
    pop esi

    mov eax, TRAMPOLINE_ADDR / PAGE_SIZE
    ret

smp_check_ap_flag:
    xor eax, eax
    mov al, byte [0x510]
    ret

smp_init_cpu0_local:
    ; Load FS with the CPU local struct base address
    mov eax, dword [esp+4]
    call gdt_set_fs_base
    mov ax, 0x38
    mov fs, ax
    ret

smp_get_cpu_number:
    mov eax, dword [fs:0000]
    ret

smp_get_cpu_kernel_stack:
    mov eax, dword [fs:0004]
    ret
