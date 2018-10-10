global task_spinup
extern lapic_eoi

section .text

task_spinup:
    push rdi
    push rsi
    call lapic_eoi
    pop rsi
    pop rdi

    test rsi, rsi
    jz .dont_load_cr3
    mov cr3, rsi

  .dont_load_cr3:
    mov rsp, rdi

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx

    mov rax, qword [rsp+32+8]
    mov ds, ax
    mov es, ax

    pop rax

    iretq
