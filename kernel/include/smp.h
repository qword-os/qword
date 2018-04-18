#ifndef __SMP_H__
#define __SMP_H__

#include <stddef.h>

typedef struct {
    size_t cpu_number;
    size_t kernel_stack;
} cpu_local_t;

void init_smp(void);
void smp_init_cpu0_local(void *);
void *smp_prepare_trampoline(void *, void *, void *, void *);
int smp_check_ap_flag(void);
size_t smp_get_cpu_number(void);
size_t smp_get_cpu_kernel_stack(void);

extern int smp_cpu_count;

#endif
