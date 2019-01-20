; Exception handlers.
global exc_div0_handler
global exc_debug_handler
global exc_nmi_handler
global exc_breakpoint_handler
global exc_overflow_handler
global exc_bound_range_handler
global exc_inv_opcode_handler
global exc_no_dev_handler
global exc_double_fault_handler
global exc_inv_tss_handler
global exc_no_segment_handler
global exc_ss_fault_handler
global exc_gpf_handler
global exc_page_fault_handler
global exc_x87_fp_handler
global exc_alignment_check_handler
global exc_machine_check_handler
global exc_simd_fp_handler
global exc_virt_handler
global exc_security_handler

extern exception_handler

; IRQs
global irq0_handler
global irq1_handler
global pic0_generic
global pic1_generic
global apic_nmi
global apic_spurious

extern pit_handler
extern pic0_generic_handler
extern pic1_generic_handler
extern apic_nmi_handler
extern apic_spurious_handler

; IPIs
global ipi_abort
global ipi_resched
global ipi_abortexec

; Misc.
extern dummy_int_handler
global int_handler
extern task_resched_bsp
extern task_resched
extern task_trigger_resched
global syscall_entry
extern kbd_handler
extern lapic_eoi_ptr

; Common handler that saves registers, calls a common function, restores registers and then returns.
%macro common_handler 1
    pusham

    call %1

    popam

    iretq
%endmacro

%macro except_handler_err_code 1
    push qword [rsp+5*8]
    push qword [rsp+5*8]
    push qword [rsp+5*8]
    push qword [rsp+5*8]
    push qword [rsp+5*8]
    pusham
    mov rdi, %1
    mov rsi, rsp
    mov rdx, qword [rsp+20*8]
    call exception_handler
    popam
    iretq
%endmacro

%macro except_handler 1
    pusham
    mov rdi, %1
    mov rsi, rsp
    xor rdx, rdx
    call exception_handler
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

; this doesn't pop rax which is the return register for syscalls
%macro popams 0
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
%endmacro

section .text
bits 64

; Exception handlers
exc_div0_handler:
    except_handler 0x0
exc_debug_handler:
    except_handler 0x1
exc_nmi_handler:
    except_handler 0x2
exc_breakpoint_handler:
    except_handler 0x3
exc_overflow_handler:
    except_handler 0x4
exc_bound_range_handler:
    except_handler 0x5
exc_inv_opcode_handler:
    except_handler 0x6
exc_no_dev_handler:
    except_handler 0x7
exc_double_fault_handler:
    except_handler_err_code 0x8
exc_inv_tss_handler:
    except_handler_err_code 0xa
exc_no_segment_handler:
    except_handler_err_code 0xb
exc_ss_fault_handler:
    except_handler_err_code 0xc
exc_gpf_handler:
    except_handler_err_code 0xd
exc_page_fault_handler:
    except_handler_err_code 0xe
exc_x87_fp_handler:
    except_handler 0x10
exc_alignment_check_handler:
    except_handler_err_code 0x11
exc_machine_check_handler:
    except_handler 0x12
exc_simd_fp_handler:
    except_handler 0x13
exc_virt_handler:
    except_handler 0x14
exc_security_handler:
    except_handler_err_code 0x1e

; IRQs
int_handler:
    common_handler dummy_int_handler

irq0_handler:
    pusham

    call pit_handler

    mov rax, qword [lapic_eoi_ptr]
    mov dword [rax], 0

    mov rdi, rsp

    call task_resched_bsp

    popam
    iretq

ipi_abortexec:
    mov rsp, qword [gs:0008]
    extern abort_thread_exec
    xor rdi, rdi
    call abort_thread_exec
  .wait:
    hlt
    jmp .wait

ipi_resched:
    pusham

    mov rax, qword [lapic_eoi_ptr]
    mov dword [rax], 0

    mov rdi, rsp

    call task_resched

    popam
    iretq

invalid_syscall:
    mov rax, -1
    ret

section .data

syscall_count equ ((syscall_table.end - syscall_table) / 8)

align 16
syscall_table:
    extern syscall_debug_print
    dq syscall_debug_print ;0
    extern syscall_open
    dq syscall_open ;1
    extern syscall_close
    dq syscall_close ;2
    extern syscall_read
    dq syscall_read ;3
    extern syscall_write
    dq syscall_write ;4
    extern syscall_getpid
    dq syscall_getpid ;5
    extern syscall_alloc_at
    dq syscall_alloc_at ;6
    extern syscall_set_fs_base
    dq syscall_set_fs_base ;7
    extern syscall_lseek
    dq syscall_lseek ;8
    extern syscall_fstat
    dq syscall_fstat ;9
    extern syscall_fork
    dq syscall_fork ;10
    extern syscall_execve
    dq syscall_execve ;11
    extern syscall_exit
    dq syscall_exit ;12
    extern syscall_waitpid
    dq syscall_waitpid ;13
    extern syscall_getppid
    dq syscall_getppid ;14
    extern syscall_chdir
    dq syscall_chdir ;15
    extern syscall_dup2
    dq syscall_dup2 ;16
    extern syscall_readdir
    dq syscall_readdir ;17
    extern syscall_fcntl
    dq syscall_fcntl ;18
    extern syscall_pipe
    dq syscall_pipe ;19
    extern syscall_getcwd
    dq syscall_getcwd ;20
    extern syscall_perfmon_create
    dq syscall_perfmon_create ;21
    extern syscall_perfmon_attach
    dq syscall_perfmon_attach ;22
  .end:

section .text

syscall_entry:
    mov qword [gs:0024], rsp ; save the user stack
    mov rsp, qword [gs:0016] ; switch to the kernel space stack for the thread

    sti

    push 0x1b            ; ss
    push qword [gs:0024] ; rsp
    push r11             ; rflags
    push 0x23            ; cs
    push rcx             ; rip

    pusham

    mov rdi, rsp

    cmp rax, syscall_count   ; is syscall_number too big?
    jae .err

    call [syscall_table + rax * 8]

  .out:
    popams
    mov rdx, qword [gs:0032] ; return errno in rdx

    cli

    mov rsp, qword [gs:0024] ; restore the user stack

    o64 sysret

  .err:
    mov rax, -1
    jmp .out

pic0_generic:
    common_handler pic0_generic_handler
pic1_generic:
    common_handler pic1_generic_handler
irq1_handler:
    pusham
    xor rax, rax
    in al, 0x60
    mov rdi, rax
    call kbd_handler
    mov rax, qword [lapic_eoi_ptr]
    mov dword [rax], 0
    popam
    iretq
; IPIs
ipi_abort:
    cli
  .hlt:
    hlt
    jmp .hlt

; APIC NMI + Spurious interrupts
apic_nmi:
    common_handler apic_nmi_handler
apic_spurious:
    common_handler apic_spurious_handler
