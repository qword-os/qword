#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>
#include <smp.h>
#include <ctx.h>
#include <lock.h>
#include <acpi/madt.h>
#include <apic.h>
#include <ipi.h>

lock_t scheduler_lock = 1;
int scheduler_ready = 0;

size_t find_process(void);
size_t find_thread(size_t);

lock_t process_table_lock = 1;
process_t **process_table;

static size_t task_count = 0;

/* These represent the default new-thread register contexts for kernel space and
 * userspace. See kernel/include/ctx.h for the register order. */
ctx_t default_krnl_ctx = {0x10,0x10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x08,0x202,0,0x10};
ctx_t default_usr_ctx = {0x23,0x23,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x1b,0x202,0,0x23};

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
            next_thread,
            0
        };
        
        /* Since we have sorted the list of CPU 
         * loads, we can just pick the lowest load 
         * from the lowest index of this list */
        int load = loads[0];
        for (size_t i = 0; i < (size_t)smp_cpu_count; i++) {
            cpu_local_t *check = &cpu_locals[i];
            if ((int)check->load == load) {
                cpu = check;
                break;
            } else {
                continue;
            }
        }
     
        for (size_t i = 0; i < MAX_THREADS; i++) {
            if (cpu->run_queue[i].is_free) {
                cpu->run_queue[i] = t;
                break;
            } else {
                continue;
            }
        }
        
        if (cpu->cpu_number == 0) {
            thread_t *next = process_table[next_proc]->threads[next_thread];
            pt_entry_t *pagemap = process_table[next_proc]->pagemap->pagemap;
            
            ctx_switch((uint64_t *)&next->ctx, pagemap);
        } else {
            // lapic_send_ipi(IPI_RESCHED, cpu->lapic_id);
        }

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
    return 0;
}

static inline void find_next_task(pid_t *process, tid_t *thread, int limit) {
//kprint(KPRN_DBG, "process: %U thread: %U", *process, *thread);

    /* Find next task to run for the current CPU */
    if (!limit)
        return;

    (*thread)++;

    for (size_t i = 0;;) {
        if (*thread == MAX_THREADS)
            goto next_process;
        thread_t *next_thread = process_table[*process]->threads[*thread];
        if (next_thread && next_thread != -1) {
            if (++i == limit)
                break;
            (*thread)++;
            continue;
        }
        if (!next_thread) {
next_process:
            *thread = 0;
            process_t *next_process = process_table[++(*process)];
            if (next_process && next_process != -1) {
                continue;
            }
            if (!next_process) {
                /* end of tasks, find first task and exit */
                *process = 0;
                find_next_task(process, thread, fsr(&global_cpu_local->cpu_number));
                break;
            } else {
                goto next_process;
            }
        } else {
            (*thread)++;
        }
    }

//kprint(KPRN_DBG, "process: %U thread: %U", *process, *thread);

    return;
}

void task_spinup(void *, void *);

static lock_t smp_sched_lock = 1;

void task_scheduler(ctx_t *ctx) {
    spinlock_acquire(&smp_sched_lock);

    if (!spinlock_test_and_acquire(&scheduler_lock)) {
        goto out_locked;
    }

    if (task_count <= fsr(&global_cpu_local->cpu_number)) {
        fsw(&global_cpu_local->current_process, -1);
        fsw(&global_cpu_local->current_thread, -1);
        goto out;
    }

    pid_t current_process = fsr(&global_cpu_local->current_process);
    tid_t current_thread = fsr(&global_cpu_local->current_thread);

    if (current_process != -1 && current_thread != -1) {
        /* Save current context */
        process_table[current_process]->threads[current_thread]->ctx = *ctx;
        find_next_task(&current_process, &current_thread, smp_cpu_count);
    } else {
        current_process = 0;
        current_thread = 0;
        find_next_task(&current_process, &current_thread, fsr(&global_cpu_local->cpu_number));
    }

    fsw(&global_cpu_local->current_process, current_process);
    fsw(&global_cpu_local->current_thread, current_thread);

    spinlock_release(&scheduler_lock);
    spinlock_release(&smp_sched_lock);

    task_spinup(&process_table[current_process]->threads[current_thread]->ctx,
                (void *)((size_t)process_table[current_process]->pagemap->pagemap - MEM_PHYS_OFFSET));

out:
    spinlock_release(&scheduler_lock);
out_locked:
    spinlock_release(&smp_sched_lock);
    return;
}

static int pit_ticks = 0;

void task_scheduler_bsp(ctx_t *ctx) {

    if (!scheduler_ready) {
        return;
    }

    if (pit_ticks++ == 5) {
        pit_ticks = 0;
    } else {
        return;
    }

    /* raise scheduler IPI for all APs */
    lapic_write(APICREG_ICR0, IPI_SCHEDULER | (1 << 18) | (1 << 19));

    task_scheduler(ctx);

    return;

}

/* Create process */
/* Returns process ID, -1 on failure */
pid_t process_create(pagemap_t *pagemap) {
    /* Search for free process ID */
    pid_t new_pid;
    for (new_pid = 0; new_pid < MAX_PROCESSES - 1; new_pid++) {
        if (!process_table[new_pid] || process_table[new_pid] == -1)
            goto found_new_pid;
    }
    return -1;

found_new_pid:
    /* Try to make space for this new task */
    if ((process_table[new_pid] = kalloc(sizeof(process_t))) == 0) {
        process_table[new_pid] = -1;
        return -1;
    }

    process_t *new_process = process_table[new_pid];

    if ((new_process->threads = kalloc(MAX_THREADS * sizeof(thread_t *))) == 0) {
        kfree(new_process);
        process_table[new_pid] = -1;
        return -1;
    }

    new_process->pagemap = pagemap;
    new_process->pid = new_pid;

    return new_pid;
}

/* Create thread from function pointer */
/* Returns thread ID, -1 on failure */
tid_t thread_create(pid_t pid, void *stk, void *(*entry)(void *), void *arg) {
    size_t *stack = stk;

    /* Search for free thread ID */
    tid_t new_tid;
    for (new_tid = 0; new_tid < MAX_THREADS; new_tid++) {
        if (!process_table[pid]->threads[new_tid] || process_table[pid]->threads[new_tid] == -1)
            goto found_new_tid;
    }
    return -1;

found_new_tid:
    /* Try to make space for this new task */
    if ((process_table[pid]->threads[new_tid] = kalloc(sizeof(thread_t))) == 0) {
        process_table[pid]->threads[new_tid] = -1;
        return -1;
    }

    thread_t *new_thread = process_table[pid]->threads[new_tid];
    
    /* Set registers to defaults */
    if (pid)
        new_thread->ctx = default_usr_ctx;
    else
        new_thread->ctx = default_krnl_ctx;

    /* Set instruction pointer to entry point, prepare RSP, and set first argument to arg */
    new_thread->ctx.rip = (size_t)entry;
    new_thread->ctx.rsp = (size_t)&stack[KRNL_STACK_SIZE - 1];
    new_thread->ctx.rdi = (size_t)arg;

    new_thread->stk = stack;

    new_thread->tid = new_tid;

    task_count++;

    return new_tid;
}
