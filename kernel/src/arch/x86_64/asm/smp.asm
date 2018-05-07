global smp_prepare_trampoline
global smp_init_cpu0_local
global smp_check_ap_flag
global smp_get_cpu_number
global smp_get_cpu_kernel_stack
global smp_get_cpu_current_process
global smp_get_cpu_current_thread
global smp_set_cpu_current_process
global smp_set_cpu_current_thread

extern load_tss

section .data

%define smp_trampoline_size  smp_trampoline_end - smp_trampoline
smp_trampoline:              incbin "real/smp_trampoline_x86_64.bin"
smp_trampoline_end:

section .text

bits 64

%define TRAMPOLINE_ADDR     0x1000
%define PAGE_SIZE           4096

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
    ; Load FS with the CPU local struct base address
    mov ax, 0x23
    mov fs, ax
    mov gs, ax
    mov rcx, 0xc0000100
    mov eax, edi
    shr rdi, 32
    mov edx, edi
    wrmsr

    ; enable SSE
    mov rax, cr0
    and al, 0xfb
    or al, 0x02
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9
    mov cr4, rax

    mov rdi, rsi
    call load_tss

    mov ax, 0x38
    ltr ax

    ret

smp_get_cpu_number:
    mov rax, qword [fs:0000]
    ret

smp_get_cpu_kernel_stack:
    mov rax, qword [fs:0008]
    ret
