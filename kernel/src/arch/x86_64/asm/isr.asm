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

; IRQs.

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

; 
exc_div0_handler:
    common_handler div0_handler
exc_debug_handler:
    common_handler debug_handler
exc_nmi_handler:
    common_handler nmi_handler
exc_breakpoint_handler:
    common_handler breakpoint_handler
exc_overflow_handler:
    common_handler overflow_handler
exc_bound_range_handler:
    common_handler bound_range_handler
exc_inv_opcode_handler:
    common_handler inv_opcode_handler
exc_no_dev_handler:
    common_handler no_dev_handler
exc_double_fault_handler:
    common_handler double_fault_handler
exc_inv_tss_handler:
    common_handler inv_tss_handler
exc_no_segment_handler:
    common_handler no_segment_handler
exc_ss_fault_handler:
    common_handler ss_fault_handler
exc_gpf_handler:
    common_handler gpf_handler
exc_page_fault_handler:
    common_handler page_fault_handler
exc_x87_fp_handler:
    common_handler x87_fp_handler
exc_alignment_check_handler:
    common_handler alignment_check_handler
exc_machine_check_handler:
    common_handler machine_check_handler
exc_simd_fp_handler:
    common_handler simd_fp_handler
exc_virt_handler:
    common_handler virt_handler
exc_security_handler:
    common_handler security_handler
