#ifndef __TASK_H__
#define __TASK_H__

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024
#define KRNL_STACK_SIZE 2048

#define STS_RUNNING 0
#define STS_READY 1
#define STS_BLOCKED 2

#include <stdint.h>
#include <ctx.h>
#include <mm.h>

typedef struct {
    ctx_t ctx;
    uint16_t tid;
    size_t *stk;
    uint8_t sts; 
} thread_t;

typedef struct {
    pagemap_t *pagemap;
    thread_t **threads;
    uint16_t pid;
    uint8_t sts;
    uint8_t priority;
} process_t;

extern int scheduler_ready;

extern process_t **process_table;
extern lock_t process_table_lock;

void init_sched(void);
void thread_spinup(size_t, size_t);
void thread_return(void);
void task_resched(ctx_t *, uint64_t *);

#endif
