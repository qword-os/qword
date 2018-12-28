extern lapic_eoi

global int_handler

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

extern div0_handler
extern debug_handler
extern nmi_handler
extern breakpoint_handler
extern overflow_handler
extern bound_range_handler
extern inv_opcode_handler
extern no_dev_handler
extern double_fault_handler
extern inv_tss_handler
extern no_segment_handler
extern ss_fault_handler
extern gpf_handler
extern page_fault_handler
extern x87_fp_handler
extern alignment_check_handler
extern machine_check_handler
extern simd_fp_handler
extern virt_handler
extern security_handler

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

; Common handler that saves registers, calls a common function, restores registers and then returns.
%macro common_handler 1
    pusham

    call %1

    popam

    iretq
%endmacro

%macro except_handler_err_code 1
    ; Since GPRs get trashed by an exception anyway we don't need to save them.
    pop rdx
    pop rsi
    pop rdi

    call %1

    iretq
%endmacro

%macro except_handler 1
    pop rsi
    pop rdi

    call %1

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

int_handler:
    common_handler dummy_int_handler
exc_div0_handler:
    except_handler div0_handler
exc_debug_handler:
    except_handler debug_handler
exc_nmi_handler:
    except_handler nmi_handler
exc_breakpoint_handler:
    except_handler breakpoint_handler
exc_overflow_handler:
    except_handler overflow_handler
exc_bound_range_handler:
    except_handler bound_range_handler
exc_inv_opcode_handler:
    except_handler inv_opcode_handler
exc_no_dev_handler:
    except_handler no_dev_handler
exc_double_fault_handler:
    except_handler_err_code double_fault_handler
exc_inv_tss_handler:
    except_handler_err_code inv_tss_handler
exc_no_segment_handler:
    except_handler_err_code no_segment_handler
exc_ss_fault_handler:
    except_handler_err_code ss_fault_handler
exc_gpf_handler:
    except_handler_err_code gpf_handler
exc_page_fault_handler:
    except_handler_err_code page_fault_handler
exc_x87_fp_handler:
    except_handler x87_fp_handler
exc_alignment_check_handler:
    except_handler_err_code alignment_check_handler
exc_machine_check_handler:
    except_handler machine_check_handler
exc_simd_fp_handler:
    except_handler simd_fp_handler
exc_virt_handler:
    except_handler virt_handler
exc_security_handler:
    except_handler_err_code security_handler

; IRQs
irq0_handler:
    pusham

    call pit_handler

    mov rdi, rsp

    call task_resched_bsp

    call lapic_eoi

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

    mov rdi, rsp

    mov rax, qword [gs:0000]
    test rax, rax
    jz .is_bsp
    call task_resched
  .is_bsp:
    call task_trigger_resched

    ; ** EXECUTION SHOULD NEVER REACH THIS POINT **
  .halt:
    hlt
    jmp .halt

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
    dq invalid_syscall
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
    call lapic_eoi
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
