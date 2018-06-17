#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>
#include <smp.h>
#include <lock.h>
#include <acpi/madt.h>
#include <apic.h>
#include <ipi.h>

lock_t scheduler_lock = 1;
int scheduler_ready = 0;

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

/* Recursively search for a new task to run */
static inline void task_get_next(pid_t *process, tid_t *thread, int limit) {
    /* Find next task to run for the current CPU */
    if (!limit) {
        thread_t *next_thread = process_table[*process]->threads[*thread];
        if (next_thread && next_thread != -1)
            return;
    }

    (*thread)++;

    for (size_t i = 0;;) {
        if (*thread == MAX_THREADS)
            goto next_process;
        thread_t *next_thread = process_table[*process]->threads[*thread];
        if (next_thread && next_thread != -1) {
            if (++i >= limit)
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
                task_get_next(process, thread, fsr(&global_cpu_local->cpu_number));
                break;
            } else {
                goto next_process;
            }
        } else {
            (*thread)++;
        }
    }

    return;
}

void task_spinup(void *, void *);

static lock_t smp_sched_lock = 1;

void task_resched(ctx_t *ctx) {
    spinlock_acquire(&smp_sched_lock);

    if (!spinlock_test_and_acquire(&scheduler_lock)) {
        goto out_locked;
    }

    if (task_count <= fsr(&global_cpu_local->cpu_number)) {
        fsw(&global_cpu_local->reset_scheduler, 0);
        fsw(&global_cpu_local->current_process, -1);
        fsw(&global_cpu_local->current_thread, -1);
        goto out;
    }

    pid_t current_process = fsr(&global_cpu_local->current_process);
    tid_t current_thread = fsr(&global_cpu_local->current_thread);

    if (current_process != -1 && current_thread != -1) {
        /* Save current context */
        process_table[current_process]->threads[current_thread]->active_on_cpu = -1;
        process_table[current_process]->threads[current_thread]->ctx = *ctx;
        find_next_task(&current_process, &current_thread, smp_cpu_count);
        if (fsr(&global_cpu_local->reset_scheduler))
            goto reset_scheduler;
    } else {
reset_scheduler:
        fsw(&global_cpu_local->reset_scheduler, 0);
        current_process = 0;
        current_thread = 0;
        task_get_next(&current_process, &current_thread, fsr(&global_cpu_local->cpu_number));
    }

    fsw(&global_cpu_local->current_process, current_process);
    fsw(&global_cpu_local->current_thread, current_thread);

    process_table[current_process]->threads[current_thread]->active_on_cpu = fsr(&global_cpu_local->cpu_number);

    spinlock_release(&scheduler_lock);
    spinlock_release(&smp_sched_lock);

    /* raise scheduler IPI on the next processor */
    if (smp_cpu_count - 1 > fsr(&global_cpu_local->cpu_number)) {
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[fsr(&global_cpu_local->cpu_number) + 1].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_RESCHED);
    }

    task_spinup(&process_table[current_process]->threads[current_thread]->ctx,
                (void *)((size_t)process_table[current_process]->pagemap->pagemap - MEM_PHYS_OFFSET));

out:
    spinlock_release(&scheduler_lock);
out_locked:
    spinlock_release(&smp_sched_lock);
    return;
}

static int pit_ticks = 0;

void task_resched_bsp(ctx_t *ctx) {
    if (!scheduler_ready) {
        return;
    }

    if (pit_ticks++ == 5) {
        pit_ticks = 0;
    } else {
        return;
    }
    
    /* Call task_scheduler on the BSP */
    task_resched(ctx);

    return;
}

/* Create process */
/* Returns process ID, -1 on failure */
pid_t task_pcreate(pagemap_t *pagemap) {
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

static void task_reset_sched(void) {
    for (int i = 0; i < smp_cpu_count; i++) {
        cpu_locals[i].reset_scheduler = 1;
    }
    return;
}

/* Kill a thread in a given process */
/* Return -1 on failure */
int task_tkill(pid_t pid, tid_t tid) {
    if (!process_table[pid]->threads[tid] || process_table[pid]->threads[tid] == -1) {
        return -1;
    }

    int active_on_cpu = process_table[pid]->threads[tid]->active_on_cpu;

    if (active_on_cpu != -1 && active_on_cpu != fsr(&global_cpu_local->cpu_number)) {
        /* Send abort execution IPI */
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[active_on_cpu].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_ABORTEXEC);
    }

    kfree(process_table[pid]->threads[tid]);
    kfree(process_table[pid]->threads[tid]->stk);

    process_table[pid]->threads[tid] = -1;

    task_count--;

    task_reset_sched();

    if (active_on_cpu != -1) {
        cpu_locals[active_on_cpu].current_process = -1;
        cpu_locals[active_on_cpu].current_thread = -1;
        if (active_on_cpu == fsr(&global_cpu_local->cpu_number)) {
            asm volatile (
                "mov rsp, %0;"
                "xor rax, rax;"
                "inc rax;"
                "lock xchg qword ptr ds:[scheduler_lock], rax;"
                "1: "
                "hlt;"
                "jmp 1b;"
                :
                : "r" (fsr(&global_cpu_local->kernel_stack))
            );
        }
    }

    return 0;
}

/* Create thread from function pointer */
/* Returns thread ID, -1 on failure */
tid_t task_tcreate(pid_t pid, void *stack, void *(*entry)(void *), void *arg) {
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

    new_thread->active_on_cpu = -1;
    
    /* Set registers to defaults */
    if (pid)
        new_thread->ctx = default_usr_ctx;
    else
        new_thread->ctx = default_krnl_ctx;

    /* Set instruction pointer to entry point, prepare RSP, and set first argument to arg */
    new_thread->ctx.rip = (size_t)entry;
    new_thread->ctx.rsp = (size_t)stack;
    new_thread->ctx.rdi = (size_t)arg;

    new_thread->tid = new_tid;

    task_count++;
    task_reset_sched();

    return new_tid;
}
