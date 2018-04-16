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

void div0_handler(size_t, size_t);
void debug_handler(size_t, size_t);
void nmi_handler(size_t, size_t);
void breakpoint_handler(size_t, size_t);
void overflow_handler(size_t, size_t);
void bound_range_handler(size_t, size_t);
void inv_opcode_handler(size_t, size_t);
void no_dev_handler(size_t, size_t);
void double_fault_handler(size_t, size_t, size_t);
void inv_tss_handler(size_t, size_t, size_t);
void no_segment_handler(size_t, size_t, size_t);
void ss_fault_handler(size_t, size_t, size_t);
void gpf_handler(size_t, size_t, size_t);
void page_fault_handler(size_t, size_t, size_t);
void x87_fp_handler(size_t, size_t);
void alignment_check_handler(size_t, size_t, size_t);
void machine_check_handler(size_t, size_t);
void simd_fp_handler(size_t, size_t);
void virt_handler(size_t, size_t);
void security_handler(size_t, size_t, size_t);

#endif
