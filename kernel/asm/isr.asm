extern pic_send_eoi
extern kernel_cr3

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
extern ipi_abort_handler

; Misc.
extern dummy_int_handler
global int_handler

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
    mov rsi, cr3
    mov rdi, rsp
    
    call pit_handler

    call pic_send_eoi
    popam
    iretq

pic0_generic:
    common_handler pic0_generic_handler
pic1_generic:
    common_handler pic1_generic_handler

; IPIs
ipi_abort:
    common_handler ipi_abort_handler

; APIC NMI + Spurious interrupts
apic_nmi:
    common_handler apic_nmi_handler
apic_spurious:
    common_handler apic_spurious_handler
