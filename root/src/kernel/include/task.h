#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <lock.h>

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024
#define MAX_TASKS (MAX_PROCESSES*16)
#define MAX_FILE_HANDLES 256

#define CURRENT_PROCESS cpu_locals[current_cpu].current_process
#define CURRENT_THREAD cpu_locals[current_cpu].current_thread

#define fxsave(PTR) ({ \
    asm volatile ("fxsave [rbx];" : : "b" (PTR)); \
})

#define fxrstor(PTR) ({ \
    asm volatile ("fxrstor [rbx];" : : "b" (PTR)); \
})

#define cr3_load(NEW_CR3) ({ \
    asm volatile ("mov cr3, rax;" : : "a" (NEW_CR3)); \
})

struct ctx_t {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

typedef int32_t pid_t;
typedef int32_t tid_t;
typedef int32_t uid_t;
typedef int32_t gid_t;

struct thread_t {
    tid_t tid;
    pid_t process;
    lock_t lock;
    uint64_t yield_target;
    int active_on_cpu;
    size_t kstack;
    size_t ustack;
    struct ctx_t ctx;
    uint8_t fxstate[512] __attribute__((aligned(16)));
};

struct process_t {
    pid_t pid;
    int priority;
    struct pagemap_t *pagemap;
    struct thread_t **threads;
    char *cwd;
    int *file_handles;
};

extern lock_t scheduler_lock;

extern struct process_t **process_table;

void init_sched(void);

tid_t task_tcreate(pid_t, void *(*)(void *), void *);
pid_t task_pcreate(struct pagemap_t *);
int task_tkill(pid_t, tid_t);

#endif
