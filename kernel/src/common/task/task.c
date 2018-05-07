#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>
#include <smp.h>
#include <ctx.h>

process_t **process_table;

/* These represent the default new-thread register contexts for kernel space and
 * userspace. */
#ifdef __X86_64__
    ctx_t default_krnl_ctx = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x08,0x202,0,0x10};
    ctx_t default_usr_ctx = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1b,0x202,0,0x23};
#endif
#ifdef __I386__
    ctx_t default_krnl_ctx = {0,0,0,0,0,0,0,0,0x08,0x202,0,0x10};
    ctx_t default_usr_ctx = {0,0,0,0,0,0,0,0,0x1b,0x202,0,0x23};
#endif

void init_sched(void) {
    kprint(KPRN_INFO, "sched: Initialising process table...");
    /* Make room for task table */
    if ((process_table = kalloc(MAX_PROCESSES * sizeof(process_t *))) == 0) {
        panic("sched: Unable to allocate process table.", 0, 0);
    }

    /* Now make space for PID 0 */
    kprint(KPRN_INFO, "sched: Creating PID 0");
    if ((process_table[0] = kalloc(sizeof(process_t))) == 0) {
        panic("sched: Unable to allocate space for kernel task", 0, 0);
    }
    if ((process_table[0]->threads = kalloc(MAX_THREADS * sizeof(thread_t *))) == 0) {
        panic("sched: Unable to allocate space for kernel threads.", 0, 0);
    }

    process_table[0]->pagemap = &kernel_pagemap;
    process_table[0]->pid = 0;
    
    kprint(KPRN_INFO, "sched: Init done.");

    return;
}

/* Called from assembly */
void task_resched(ctx_t *prev) {
    /* Search for a new thread to run 
     * 1: Look for new thread identifier in run queue of CPU 
     * which the scheduler is currently running on.
     * 2: Use data from thread identifier to lookup thread in global task table
     * 3: Spinup thread context *
    TODO: Implement load balancing */
    return; 
}

/* Create kernel task from function pointer 
 * TODO: Make this a special case of spawning a thread, passing 0 as a parameter
 * to some more generic function which will spawn the thread in the kernel process */
int spawn_kthread(void (*entry)(void)) {
    size_t *stack = kalloc(KRNL_STACK_SIZE);

    if (!stack) {
        return -1;
    }

    /* Search for free thread ID */
    size_t new_tid;
    for (new_tid = 0; new_tid < MAX_THREADS; new_tid++) {
        if (!process_table[0]->threads[new_tid])
            goto found_new_tid;
    }
    return -1;

found_new_tid:
    /* Try to make space for this new task */
    if ((process_table[0]->threads[new_tid] = kalloc(sizeof(thread_t))) == 0) {
        return -1;
    }

    thread_t *new_thread = process_table[0]->threads[new_tid];
    
    /* Set registers to defaults */
    new_thread->ctx = default_krnl_ctx;

    #ifdef __X86_64__
        new_thread->ctx.rip = (size_t)(entry);
        new_thread->ctx.rsp = (size_t)&stack[KRNL_STACK_SIZE - 1];
    #endif
    #ifdef __I386__
        new_thread->ctx.eip = (size_t)(entry);
        new_thread->ctx.esp = (size_t)&stack[KRNL_STACK_SIZE - 1];
    #endif

    stack[KRNL_STACK_SIZE - 1] = (size_t)(void *)(thread_return);
    new_thread->stk = stack;

    new_thread->tid = new_tid;

    return 0;
}

void thread_return(void) {
    /* Should clear interrupts */
    asm volatile ("cli");

    /* Get thread and process indices */
    size_t pid = fsr(&global_cpu_local->current_process);
    size_t tid = fsr(&global_cpu_local->current_thread);
    
    thread_t *prev = process_table[pid]->threads[tid];

    /* Free thread data and metadata */
    kfree(prev->stk);
    kfree(prev);

    process_table[pid]->threads[tid] = 0;

    /* Should call the scheduler. Return here makes no sense */
    /* TODO */
}
