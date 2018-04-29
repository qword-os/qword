#ifndef __TASK_H__
#define __TASK_H__

#define MAX_PROCESSES 65536
#define MAX_THREADS 1024
#define KRNL_STACK_SIZE 16384

#include <stdint.h>
#include <ctx.h>
#include <mm.h>

typedef struct {
    ctx_t *ctx;
    uint16_t tid;
} thread_t;

typedef struct {
    pt_entry_t *pagemap;
    thread_t **threads;
    uint16_t pid;
} process_t;

extern process_t **task_table;

void init_task_table(void);

#endif
