global smp_prepare_trampoline
global smp_init_cpu0_local
global smp_check_ap_flag

extern syscall_entry

extern load_tss

section .data

%define smp_trampoline_size  smp_trampoline_end - smp_trampoline
smp_trampoline:              incbin "real/smp_trampoline.bin"
smp_trampoline_end:

section .text

bits 64

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

    ; enable SSE
    mov rax, cr0
    and al, 0xfb
    or al, 0x02
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9
    mov cr4, rax

    ; set up the PAT properly
    mov rcx, 0x277
    rdmsr
    mov edx, 0x0105     ; write-protect and write-combining
    wrmsr

    mov rdi, rsi
    call load_tss

    mov ax, 0x38
    ltr ax

    ; enable syscall in EFER
    mov rcx, 0xc0000080
    rdmsr
    or al, 1
    wrmsr

    ; setup syscall MSRs
    mov rcx, 0xc0000081
    mov rdx, 0x00130008
    mov rax, 0x00000000
    wrmsr
    mov rcx, 0xc0000082
    mov rax, syscall_entry
    mov rdx, rax
    shr rdx, 32
    mov eax, eax
    wrmsr
    mov rcx, 0xc0000084
    mov rax, ~(0x002)
    xor rdx, rdx
    not rdx
    wrmsr

    ret
