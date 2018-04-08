global int_handler
extern dummy_int_handler

; Common handler that saves registers, calls a common function, restores registers and then returns.
%macro common_handler 1
    pusham

    call %1

    popam

    iretq

%endmacro

; Save registers.
%macro pusham 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popam 0
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
%endmacro

int_handler:
    common_handler dummy_int_handler
