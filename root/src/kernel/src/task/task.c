#include <stdint.h>
#include <stddef.h>
#include <task.h>
#include <mm.h>
#include <klib.h>
#include <panic.h>
#include <smp.h>
#include <lock.h>
#include <acpi/madt.h>
#include <apic.h>
#include <ipi.h>
#include <fs.h>

#define SMP_TIMESLICE_MS 5

lock_t scheduler_lock = 0;

struct process_t **process_table;

static int64_t task_count = 0;

/* These represent the default new-thread register contexts for kernel space and
 * userspace. See kernel/include/ctx.h for the register order. */
static struct ctx_t default_krnl_ctx = {0x10,0x10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x08,0x202,0,0x10};
static struct ctx_t default_usr_ctx = {0x1b,0x1b,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x23,0x202,0,0x1b};

static uint8_t default_fxstate[512];

void init_sched(void) {
    fxsave(&default_fxstate);

    kprint(KPRN_INFO, "sched: Initialising process table...");
    /* Make room for task table */
    if ((process_table = kalloc(MAX_PROCESSES * sizeof(struct process_t *))) == 0) {
        panic("sched: Unable to allocate process table.", 0, 0);
    }

    /* Now make space for PID 0 */
    kprint(KPRN_INFO, "sched: Creating PID 0");
    if ((process_table[0] = kalloc(sizeof(struct process_t))) == 0) {
        panic("sched: Unable to allocate space for kernel task", 0, 0);
    }
    if ((process_table[0]->threads = kalloc(MAX_THREADS * sizeof(struct thread_t *))) == 0) {
        panic("sched: Unable to allocate space for kernel threads.", 0, 0);
    }
    process_table[0]->pagemap = &kernel_pagemap;
    process_table[0]->pid = 0;

    kprint(KPRN_INFO, "sched: Init done.");

    return;
}

/* Search for a new task to run */
static void task_get_next(pid_t *process, pid_t *thread, size_t step) {
    size_t step_counter = 0;
    struct thread_t *next_thread;
    struct process_t *next_process;

    if (!step) {
        /* Step is 0, return the current process-thread if valid, else find first valid one */
        next_thread = process_table[*process]->threads[*thread];
        /* Confirm that thread is inactive and valid */
        if (next_thread && next_thread != (void *)(-1))
            return;
    }

get_next_thread:
    (*thread)++;
get_next_thread_noincrement:
    /* Now check if this thread is the last thread in the process. If it is,
     * skip to the next process in the process table */
    next_thread = process_table[*process]->threads[*thread];
    /* Same as above, check if inactive and valid */
    if (next_thread && next_thread != (void *)(-1)) {
        if (++step_counter >= step)
            return;
        else
            goto get_next_thread;
    }
    if (!next_thread) {
        goto get_next_process;
    } else {
        goto get_next_thread;
    }

get_next_process:
    *thread = 0;
    next_process = process_table[++(*process)];
    /* If the selected process is valid, continue searching for
     * threads in this new process */
    if (next_process && next_process != (void *)(-1)) {
        goto get_next_thread_noincrement;
    }
    if (!next_process) {
        /* We reached the end of tasks, start from the beginning and exit */
        *process = 0;
        *thread = 0;
        task_get_next(process, thread, current_cpu);
        return;
    } else {
        /* If we reached this point, we still need to find a new process. Do that thing */
        goto get_next_process;
    }

    panic("something weird happened", 0, 0);

}

void task_spinup(void *);

static lock_t switched_cpus = 0;

void task_resched(struct ctx_t *ctx) {
    if (task_count <= current_cpu) {
        cpu_locals[current_cpu].reset_scheduler = 0;
        cpu_locals[current_cpu].current_process = -1;
        cpu_locals[current_cpu].current_thread = -1;
        spinlock_inc(&switched_cpus);
        while (spinlock_read(&switched_cpus) < smp_cpu_count);
        if (!current_cpu) {
            spinlock_release(&scheduler_lock);
        }
        return;
    }

    pid_t current_process = cpu_locals[current_cpu].current_process;
    pid_t last_process = current_process;
    tid_t current_thread = cpu_locals[current_cpu].current_thread;

    if (current_process != -1 && current_thread != -1) {
        /* Save current context */
        process_table[current_process]->threads[current_thread]->active_on_cpu = -1;
        process_table[current_process]->threads[current_thread]->ctx = *ctx;
        /* Save FPU context */
        fxsave(&process_table[current_process]->threads[current_thread]->fxstate);
        /* Save user rsp */
        process_table[current_process]->threads[current_thread]->ustack = cpu_locals[current_cpu].thread_ustack;
        if (cpu_locals[current_cpu].reset_scheduler)
            goto reset_scheduler;
        task_get_next(&current_process, &current_thread, smp_cpu_count);
    } else {
reset_scheduler:
        cpu_locals[current_cpu].reset_scheduler = 0;
        current_process = 0;
        current_thread = 0;
        task_get_next(&current_process, &current_thread, current_cpu);
    }

    cpu_locals[current_cpu].current_process = current_process;
    cpu_locals[current_cpu].current_thread = current_thread;

    cpu_locals[current_cpu].thread_kstack = process_table[current_process]->threads[current_thread]->kstack;
    cpu_locals[current_cpu].thread_ustack = process_table[current_process]->threads[current_thread]->ustack;

    process_table[current_process]->threads[current_thread]->active_on_cpu = current_cpu;

    /* Swap cr3, if necessary */
    if (current_process != last_process) {
        cr3_load((size_t)process_table[current_process]->pagemap->pagemap - MEM_PHYS_OFFSET);
    }

    /* Restore FPU context */
    fxrstor(&process_table[current_process]->threads[current_thread]->fxstate);

    spinlock_inc(&switched_cpus);
    while (spinlock_read(&switched_cpus) < smp_cpu_count);
    if (!current_cpu) {
        spinlock_release(&scheduler_lock);
    }

    /* Return to the thread */
    task_spinup(&process_table[current_process]->threads[current_thread]->ctx);

}

static int pit_ticks = 0;

void task_resched_bsp(struct ctx_t *ctx) {
    /* Assert lock on the scheduler */
    if (!spinlock_test_and_acquire(&scheduler_lock)) {
        return;
    }

    if (++pit_ticks == SMP_TIMESLICE_MS) {
        pit_ticks = 0;
    } else {
        spinlock_release(&scheduler_lock);
        return;
    }

    spinlock_test_and_acquire(&switched_cpus);

    for (int i = 1; i < smp_cpu_count; i++) {
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_RESCHED);
    }

    /* Call task_scheduler on the BSP */
    task_resched(ctx);
}

/* Create process */
/* Returns process ID, -1 on failure */
pid_t task_pcreate(struct pagemap_t *pagemap) {
    /* Search for free process ID */
    pid_t new_pid;
    for (new_pid = 0; new_pid < MAX_PROCESSES - 1; new_pid++) {
        if (!process_table[new_pid] || process_table[new_pid] == (void *)(-1))
            goto found_new_pid;
    }
    return -1;

found_new_pid:
    /* Try to make space for this new task */
    if ((process_table[new_pid] = kalloc(sizeof(struct process_t))) == 0) {
        process_table[new_pid] = EMPTY;
        return -1;
    }

    struct process_t *new_process = process_table[new_pid];

    if ((new_process->threads = kalloc(MAX_THREADS * sizeof(struct thread_t *))) == 0) {
        kfree(new_process);
        process_table[new_pid] = EMPTY;
        return -1;
    }

    if ((new_process->file_handles = kalloc(MAX_FILE_HANDLES * sizeof(int))) == 0) {
        kfree(new_process->threads);
        kfree(new_process);
        process_table[new_pid] = EMPTY;
        return -1;
    }

    /* Initially, mark all file handles as unused */
    for (size_t i = 0; i < MAX_FILE_HANDLES; i++) {
        process_table[new_pid]->file_handles[i] = -1;
    }

    /* Map the higher half into the process */
    for (size_t i = 256; i < 512; i++) {
        pagemap->pagemap[i] = process_table[0]->pagemap->pagemap[i];
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
    if (!process_table[pid]->threads[tid] || process_table[pid]->threads[tid] == (void *)(-1)) {
        return -1;
    }

    int active_on_cpu = process_table[pid]->threads[tid]->active_on_cpu;

    if (active_on_cpu != -1 && active_on_cpu != current_cpu) {
        /* Send abort execution IPI */
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[active_on_cpu].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_ABORTEXEC);
    }

    kfree(process_table[pid]->threads[tid]);

    process_table[pid]->threads[tid] = EMPTY;

    task_count--;

    task_reset_sched();

    if (active_on_cpu != -1) {
        cpu_locals[active_on_cpu].current_process = -1;
        cpu_locals[active_on_cpu].current_thread = -1;
        if (active_on_cpu == current_cpu) {
            panic("thread killing self isn't allowed", 0, 0);
        }
    }

    return 0;
}

#define KSTACK_LOCATION_TOP ((size_t)0x0000800000000000)
#define KSTACK_SIZE ((size_t)32768)

/* Create thread from function pointer */
/* Returns thread ID, -1 on failure */
tid_t task_tcreate(pid_t pid, void *stack, void *(*entry)(void *), void *arg) {
    /* Search for free thread ID */
    tid_t new_tid;
    for (new_tid = 0; new_tid < MAX_THREADS; new_tid++) {
        if (!process_table[pid]->threads[new_tid] || process_table[pid]->threads[new_tid] == (void *)(-1))
            goto found_new_tid;
    }
    return -1;

found_new_tid:
    /* Try to make space for this new task */
    if ((process_table[pid]->threads[new_tid] = kalloc(sizeof(struct thread_t))) == 0) {
        process_table[pid]->threads[new_tid] = EMPTY;
        return -1;
    }

    struct thread_t *new_thread = process_table[pid]->threads[new_tid];

    new_thread->active_on_cpu = -1;

    /* Set up a kernel stack for the thread */
    size_t kstack_bottom = KSTACK_LOCATION_TOP - KSTACK_SIZE * (new_tid + 1);
    void *kstack = pmm_alloc(KSTACK_SIZE / PAGE_SIZE);
    if (!kstack) {
        kfree(process_table[pid]->threads[new_tid]);
        process_table[pid]->threads[new_tid] = EMPTY;
        return -1;
    }
    for (size_t i = 0; i < KSTACK_SIZE / PAGE_SIZE; i++) {
        map_page(process_table[pid]->pagemap, (size_t)(kstack + (i * PAGE_SIZE)),
                    (size_t)(kstack_bottom + (i * PAGE_SIZE)), 0x03);
    }
    new_thread->kstack = kstack_bottom + KSTACK_SIZE;

    /* Set registers to defaults */
    if (pid)
        new_thread->ctx = default_usr_ctx;
    else
        new_thread->ctx = default_krnl_ctx;

    /* Set instruction pointer to entry point, prepare RSP, and set first argument to arg */
    new_thread->ctx.rip = (size_t)entry;
    new_thread->ctx.rsp = (size_t)stack;
    new_thread->ctx.rdi = (size_t)arg;

    kmemcpy(new_thread->fxstate, default_fxstate, 512);

    new_thread->tid = new_tid;

    task_count++;
    task_reset_sched();

    return new_tid;
}
