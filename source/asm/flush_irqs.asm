extern real_routine

global flush_irqs

section .data

%define flush_irqs_size           flush_irqs_end - flush_irqs_bin
flush_irqs_bin:                   incbin "real/flush_irqs.bin"
flush_irqs_end:

section .text

bits 64

flush_irqs:
    ; void flush_irqs(void);
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    mov rsi, flush_irqs_bin
    mov rcx, flush_irqs_size
    call real_routine

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
