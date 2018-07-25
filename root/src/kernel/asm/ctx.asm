global ctx_switch
section .data
    new_cr3 dq  0

section .text
bits 64
ctx_switch:
    ; Save new pagemap.
    mov qword [new_cr3], rsi
    
    ; Load GPRs
    mov rbx, qword [rdi + (8 * 14)]
    mov rcx, qword [rdi + (8 * 13)]
    mov rdx, qword [rdi + (8 * 12)]
    mov rsi, qword [rdi + (8 * 11)]
    mov rbp, qword [rdi + (8 * 10)]
    mov r8, qword [rdi + (8 * 9)]
    mov r9, qword [rdi + (8 * 8)]
    mov r10, qword [rdi + (8 * 7)]
    mov r11, qword [rdi + (8 * 6)]
    mov r12, qword [rdi + (8 * 5)]
    mov r13, qword [rdi + (8 * 4)]
    mov r14, qword [rdi + (8 * 3)]
    mov r15, qword [rdi + (8 * 2)]
    
    ; load extra segment
    mov rax, qword [rdi]
    mov es, ax

    ; Push RSP, CS, RFLAGS, SS and RIP on the stack.
    push qword [rdi + (8 * 21)]
    push qword [rdi + (8 * 20)]
    push qword [rdi + (8 * 19)]
    push qword [rdi + (8 * 18)]
    push qword [rdi + (8 * 17)]
    
    ; RAX, RDI, DS.
    push qword [rdi + (8 * 16)]
    push qword [rdi + (8 * 11)]
    push qword [rdi + (8 * 1)]

    ; Switch address spaces.
    mov rax, qword [new_cr3]
    mov cr3, rax
    
    ; Switch DS and SS.
    pop rax
    mov ds, ax
    mov ss, ax

    pop rdi
    pop rax
    
    ; Return to the new context.
    iretq
