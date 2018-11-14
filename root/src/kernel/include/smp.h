#ifndef __SMP_H__
#define __SMP_H__

#include <stdint.h>
#include <stddef.h>
#include <task.h>

#define MAX_CPUS 128

#define current_cpu ({ \
    int cpu_number; \
    asm volatile ("mov rax, qword ptr gs:[0]" : "=a" (cpu_number)); \
    cpu_number; \
})

struct cpu_local_t {
    /* DO NOT MOVE THESE MEMBERS FROM THESE LOCATIONS */
    /* DO NOT CHANGE THEIR TYPES */
    size_t cpu_number;
    size_t kernel_stack;
    size_t thread_kstack;
    size_t thread_ustack;
    /* Feel free to move every other member, and use any type as you see fit */
    tid_t current_task;
    pid_t current_process;
    tid_t current_thread;
    uint8_t lapic_id;
};

extern struct cpu_local_t cpu_locals[MAX_CPUS];

void init_smp(void);

extern int smp_cpu_count;

#endif
