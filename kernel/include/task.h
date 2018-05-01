#ifndef __TASK_H__
#define __TASK_H__

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024
#define KRNL_STACK_SIZE 2048

#include <stdint.h>
#include <ctx.h>
#include <mm.h>

typedef struct {
    ctx_t *ctx;
    uint16_t tid;
    size_t *stk; 
} thread_t;

typedef struct {
    pagemap_t *pagemap;
    thread_t **threads;
    uint16_t pid;
} process_t;

extern process_t **task_table;

void init_task_table(void);
void thread_spinup(size_t, size_t);
void thread_return(void);

#endif
