#ifndef __SMP_H__
#define __SMP_H__

#include <stddef.h>

#ifdef __X86_64__
#define fsr(offset) ({ \
    size_t value; \
    asm volatile ("mov rax, qword ptr fs:[rbx]" : "=a"(value) : "b"(offset) :); \
    value; \
})

#define fsw(offset, value) ({ \
    asm volatile ("mov qword ptr, fs:[rbx], rax", : : "a"(value), "b"(offset) :); \
})
#endif

#ifdef __I386__
#define fsr(offset) ({ \
    size_t value; \
    asm volatile ("mov eax, dword ptr fs:[ebx]" : "=a"(value) : "b"(offset) :); \
    value; \
})

#define fsw(offset, value) ({ \
    asm volatile ("mov dword ptr fs:[ebx], eax" : "a"(value), "b"(offset) : ); \
})
#endif

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

/* Hack for using sub-structs with CPU local */
#define global_cpu_local ((cpu_local_t *)0)

void init_smp(void);
void smp_init_cpu0_local(void *, void *);
void *smp_prepare_trampoline(void *, void *, void *, void *, void *);
int smp_check_ap_flag(void);
size_t smp_get_cpu_number(void);
size_t smp_get_cpu_kernel_stack(void);
size_t smp_get_cpu_current_process(void);
size_t smp_get_cpu_current_thread(void);

extern int smp_cpu_count;

#endif
