global task_spinup
global force_resched

extern scheduler_lock
extern resched_lock

extern task_resched

section .data

signal_trampoline_size equ signal_trampoline.end - signal_trampoline
global signal_trampoline_size
global signal_trampoline
signal_trampoline:
    mov rax, rdi
    shr rdi, 48  ; signum
    mov rdx, 0x0000ffffffffffff
    and rax, rdx
    call rax ; handler address
    mov rax, 28
    syscall     ; end of signal syscall
  .end:

section .text

task_spinup:
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

    ; release relevant locks
    lock inc qword [scheduler_lock]
    lock inc qword [resched_lock]

    pop rax

    iretq

force_resched:
    cli

    mov rax, rsp

    push 0x10
    push rax
    push 0x202
    push 0x08
    mov rax, .done
    push rax
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

    ; release relevant locks
    lock inc qword [scheduler_lock]

    mov rdi, rsp
  .retry:
    call task_resched
    jmp .retry

  .done:
    ret
