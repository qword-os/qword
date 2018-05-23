global real_routine

section .data

%define real_init_size  real_init_end - real_init
real_init:              incbin "real/real_init.bin"
real_init_end:

section .text

bits 64

real_routine:
    ; RSI = routine location
    ; RCX = routine size
    
    push rsi
    push rcx

    ; Real mode init blob to 0000:1000
    mov rsi, real_init
    mov rdi, 0x1000
    mov rcx, real_init_size
    rep movsb
    
    ; Routine's blob to 0000:8000
    pop rcx
    pop rsi
    mov rdi, 0x8000
    rep movsb
    
    ; Call module
    mov rax, 0x1000
    call rax
    
    ret
