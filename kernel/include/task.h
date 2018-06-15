#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <lock.h>

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024

#define STS_RUNNING 0
#define STS_READY 1
#define STS_BLOCKED 2

typedef struct {
    uint64_t es;
    uint64_t ds;
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
} ctx_t;

typedef size_t pid_t;
typedef size_t tid_t;

typedef struct {
    tid_t tid;
    int priority;
    int sts;
    int active_on_cpu;
    ctx_t ctx;
    size_t *stk;
} thread_t;

typedef struct {
    pid_t pid;
    int priority;
    pagemap_t *pagemap;
    thread_t **threads;
} process_t;

extern int scheduler_ready;
extern lock_t scheduler_lock;

extern process_t **process_table;

void init_sched(void);

tid_t thread_create(pid_t, void *, void *(*)(void *), void *);
pid_t process_create(pagemap_t *);

#endif
