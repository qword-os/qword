#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>
#include <smp.h>
#include <ctx.h>
#include <lock.h>
#include <acpi/madt.h>

int scheduler_ready = 0;

size_t find_process(void);
size_t find_thread(size_t);

lock_t process_table_lock = 1;
process_t **process_table;

/* These represent the default new-thread register contexts for kernel space and
 * userspace. See kernel/include/ctx.h for the register order. */
ctx_t default_krnl_ctx = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x08,0x202,0,0x10};
ctx_t default_usr_ctx = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1b,0x202,0,0x23};

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

    scheduler_ready = 1;
    
    kprint(KPRN_INFO, "sched: Init done.");

    return;
}

/* Find a new task to run */
void task_resched(ctx_t *prev, uint64_t *pagemap) {
    int loads[smp_cpu_count];
    cpu_local_t *cpu;
    
    /* Store each CPU's respective load */
    for (int i = 0; i < smp_cpu_count; i++) {
        cpu_local_t *check = &cpu_locals[i];
        loads[i] = (int)check->load;
    }

    kqsort(loads, 0, smp_cpu_count);

    /* Save context */
    size_t prev_pid = fsr(&global_cpu_local->current_process);
    size_t prev_tid = fsr(&global_cpu_local->current_thread);
    process_table[prev_pid]->threads[prev_tid]->ctx = *(prev);
    process_table[prev_pid]->pagemap->pagemap = pagemap;

    size_t next_proc = find_process();
    if (!next_proc)
        return;
    else {
        /* Now decide upon a thread to run */
        size_t next_thread = find_thread(next_proc);
        thread_identifier_t t = {
            next_proc,
            next_thread 
        };
        
        /* Since we have sorted the list of CPU 
         * loads, we can just pick the lowest load 
         * from the lowest index of this list */
        int load = loads[0];
        for (size_t i = 0; i < smp_cpu_count; i++) {
            cpu_local_t *check = &cpu_locals[i];
            if (check->load == load) {
                cpu = check;
                break;
            } else {
                continue;
            }
        }
    

        /* TODO Find free thread in CPU's run queue, add
         * resched IPI */
        return;
    }
}

size_t find_thread(size_t proc) {
    process_t *next_proc  = process_table[proc];

    for (size_t i = 0; i < MAX_THREADS; i++) {
        if (next_proc->threads[i]->sts == STS_READY) {
            return i;
        } else {
            continue;
        }
    }
    
    return 0;
}


/* Return the index into the process table of the next process to be run */
size_t find_process(void) {
    /* TODO */
    return 1;
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

    /* Set instruction pointer to entry point and set stack pointer to a catch-all 
     * function that will ensure a process is killed properly upon process return */
    new_thread->ctx.rip = (size_t)(entry);
    new_thread->ctx.rsp = (size_t)&stack[KRNL_STACK_SIZE - 1];

    stack[KRNL_STACK_SIZE - 1] = (size_t)(void *)(thread_return);
    new_thread->stk = stack;

    new_thread->tid = new_tid;

    return 0;
}

void thread_return(void) {
    asm volatile ("cli");

    /* Get thread and process indices */
    size_t pid = fsr(&global_cpu_local->current_process);
    size_t tid = fsr(&global_cpu_local->current_thread);
    
    thread_t *prev = process_table[pid]->threads[tid];

    /* Free thread data and metadata */
    kfree(prev->stk);
    kfree(prev);

    process_table[pid]->threads[tid] = 0;
    task_resched(&prev->ctx, process_table[pid]->pagemap->pagemap);
}
