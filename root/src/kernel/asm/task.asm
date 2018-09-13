global task_spinup
extern pic_send_eoi

section .text

task_spinup:
    push rsi
    push rdi

    call pic_send_eoi

    pop rdi
    pop rsi

    mov cr3, rsi

    mov rsp, rdi

    pop rax
    mov es, ax
    pop rbx
    mov ds, ax

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
    pop rax

    iretq
