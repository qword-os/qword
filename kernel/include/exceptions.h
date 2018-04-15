#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

/* Assembly routines */
void exc_div0_handler(void);
void exc_debug_handler(void);
void exc_nmi_handler(void);
void exc_breakpoint_handler(void);
void exc_overflow_handler(void);
void exc_bound_range_handler(void);
void exc_inv_opcode_handler(void);
void exc_no_dev_handler(void);
void exc_double_fault_handler(void);
void exc_inv_tss_handler(void);
void exc_no_segment_handler(void);
void exc_ss_fault_handler(void);
void exc_gpf_handler(void);
void exc_page_fault_handler(void);
void exc_x87_fp_handler(void);
void exc_alignment_check_handler(void);
void exc_machine_check_handler(void);
void exc_simd_fp_handler(void);
void exc_virt_handler(void);
void exc_security_handler(void);

void div0_handler(void);
void debug_handler(void);
void nmi_handler(void);
void breakpoint_handler(void);
void overflow_handler(void);
void bound_range_handler(void);
void inv_opcode_handler(void);
void no_dev_handler(void);
void double_fault_handler(void);
void inv_tss_handler(void);
void no_segment_handler(void);
void ss_fault_handler(void);
void gpf_handler(void);
void page_fault_handler(void);
void x87_fp_handler(void);
void alignment_check_handler(void);
void machine_check_handler(void);
void simd_fp_handler(void);
void virt_handler(void);
void security_handler(void);

#endif
