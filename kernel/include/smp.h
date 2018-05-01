#ifndef __SMP_H__
#define __SMP_H__

#include <stddef.h>

typedef struct {
    size_t process_idx;
    size_t thread_idx;
} thread_identifier_t;

typedef struct {
    size_t cpu_number;
    size_t kernel_stack;
    /* The index into the task table */
    size_t current_process;
    /* The index into the current processes thread array
     * that represents the current thread of execution on
     * a given processor */
    size_t current_thread;
    thread_identifier_t run_queue[1024];
} cpu_local_t;


void init_smp(void);
void smp_init_cpu0_local(void *, void *);
void *smp_prepare_trampoline(void *, void *, void *, void *, void *);
int smp_check_ap_flag(void);
size_t smp_get_cpu_number(void);
size_t smp_get_cpu_kernel_stack(void);
size_t smp_get_cpu_current_process(void);
size_t smp_get_cpu_current_thread(void);
void smp_set_cpu_current_process(size_t);
void smp_set_cpu_current_thread(size_t);

extern int smp_cpu_count;

#endif
