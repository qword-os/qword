#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>
#include <smp.h>
#include <ctx.h>

process_t **task_table;
/* Hack for using sub-structs with CPU local */
#define cpu_local ((cpu_local_t *)0)

void init_task_table(void) {
    kprint(KPRN_INFO, "sched: Initialising task table...");
    /* Make room for task table */
    if ((task_table = kalloc(MAX_PROCESSES * sizeof(process_t *))) == 0) {
        panic("sched: Unable to allocate task table.", 0, 0);
    }

    /* Now make space for PID 0 */
    kprint(KPRN_INFO, "sched: Creating PID 0");
    if ((task_table[0] = kalloc(sizeof(process_t *))) == 0) {
        panic("sched: Unable to allocate space for kernel task", 0, 0);
    }
    if ((task_table[0]->threads = kalloc(MAX_THREADS * sizeof(thread_t *))) == 0) {
        panic("sched: Unable to allocate space for kernel threads.", 0, 0);
    }

    task_table[0]->pagemap = &kernel_pagemap;
    task_table[0]->pid = 0;
    
    kprint(KPRN_INFO, "sched: Init done.");

    return;
}

/* Called from assembly */
void task_resched(ctx_t *prev) {
    /* Search for a new thread to run 
     * 1: Look for new thread identifier in run queue of CPU 
     * which the scheduler is currently running on.
     * 2: Use data from thread identifier to lookup thread in global task table
     * 3: Spinup thread context */
    /* TODO */
    return; 
}

/* Create kernel task from function pointer 
 * TODO: Make this a special case of spawning a thread, passing 0 as a parameter
 * to some more generic function which will spawn the thread in the kernel process */
int spawn_kthread(void *entry(void)) {
    size_t *stack = kalloc(KRNL_STACK_SIZE);
    thread_t *new_task = {0};

    /* Search for free thread ID */
    size_t new_tid;
    for (new_tid = 0; new_tid++; new_tid++) {
        if ((!task_table[0]->threads[new_tid]) || (task_table[0]->threads[new_tid] == (thread_t *)-1)) 
            break;
    }

    if (new_tid == MAX_THREADS)
        return -1;
    
    /* Try to make space for this new task */
    if ((task_table[0]->threads[new_tid] = kalloc(sizeof(thread_t *))) == 0) {
        task_table[0]->threads[new_tid] = (thread_t *)-1;
        return -1;
    }    
    
    /* Set registers to defaults */
    set_ctx_krnl(new_task->ctx);

    new_task->ctx->rip = (size_t)(entry); 
    stack[KRNL_STACK_SIZE - 1] = (size_t)(void *)(thread_return);
    new_task->ctx->rsp = stack[KRNL_STACK_SIZE - 1];
    new_task->stk = stack;
    /* TODO task status */

    return 0;
}

void thread_return(void) {
    /* TODO Kill process */
    return;
}
