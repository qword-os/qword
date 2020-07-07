global smp_prepare_trampoline
global smp_init_cpu0_local
global smp_check_ap_flag

extern syscall_entry

extern load_tss

section .data

%define smp_trampoline_size  smp_trampoline_end - smp_trampoline
smp_trampoline:              incbin "sys/smp_trampoline.bin"
smp_trampoline_end:

section .text

%define TRAMPOLINE_ADDR     0x1000
%define PAGE_SIZE           4096

; Store trampoline data in low memory and return the page index of the
; trampoline code.
smp_prepare_trampoline:
    ; entry point in rdi, page table in rsi
    ; stack pointer in rdx, cpu local in rcx
    ; tss in r8

    ; prepare variables
    mov byte [0x510], 0
    mov qword [0x520], rdi
    mov qword [0x540], rsi
    mov qword [0x550], rdx
    mov qword [0x560], rcx
    mov qword [0x570], syscall_entry
    sgdt [0x580]
    sidt [0x590]

    ; Copy trampoline blob to 0x1000
    mov rsi, smp_trampoline
    mov rdi, TRAMPOLINE_ADDR
    mov rcx, smp_trampoline_size
    rep movsb

    mov rdi, r8
    call load_tss

    mov rax, TRAMPOLINE_ADDR / PAGE_SIZE
    ret

smp_check_ap_flag:
    xor rax, rax
    mov al, byte [0x510]
    ret

smp_init_cpu0_local:
    ; Load GS with the CPU local struct base address
    mov ax, 0x1b
    mov fs, ax
    mov gs, ax
    mov rcx, 0xc0000101
    mov eax, edi
    shr rdi, 32
    mov edx, edi
    wrmsr

    mov rdi, rsi
    call load_tss

    mov ax, 0x38
    ltr ax

    ret
