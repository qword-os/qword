#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>

process_t **task_table;

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
