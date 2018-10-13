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

void task_spinup(void *, size_t);

lock_t scheduler_lock = 0;

struct process_t **process_table;

struct thread_t **task_table;
static int64_t task_count = 0;

static lock_t switched_cpus = 0;

/* These represent the default new-thread register contexts for kernel space and
 * userspace. See kernel/include/ctx.h for the register order. */
static struct ctx_t default_krnl_ctx = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x08,0x202,0,0x10};
static struct ctx_t default_usr_ctx = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x23,0x202,0,0x1b};

static uint8_t default_fxstate[512];

void init_sched(void) {
    fxsave(&default_fxstate);

    kprint(KPRN_INFO, "sched: Initialising process table...");

    /* Make room for task table */
    if ((task_table = kalloc(MAX_TASKS * sizeof(struct thread_t *))) == 0) {
        panic("sched: Unable to allocate task table.", 0, 0);
    }
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
static inline void task_get_next(tid_t *taskptr) {
    tid_t current_task = *taskptr+1;

    for (;;) {
        struct thread_t *thread = task_table[current_task];
        if (!thread) {
            current_task = 0;
            continue;
        } else if (thread == (void *)(-1)) {
            if (++current_task == MAX_TASKS)
                current_task = 0;
            continue;
        }
        if (!spinlock_test_and_acquire(&thread->lock)) {
            if (++current_task == MAX_TASKS)
                current_task = 0;
            continue;
        }
        *taskptr = current_task;
        return;
    }

}

static inline void idle(void) {
    if (task_count <= current_cpu) {
        cpu_locals[current_cpu].current_task = -1;
        cpu_locals[current_cpu].current_thread = -1;
        cpu_locals[current_cpu].current_process = -1;
        spinlock_inc(&switched_cpus);
        while (spinlock_read(&switched_cpus) < smp_cpu_count);
        if (!current_cpu) {
            spinlock_release(&scheduler_lock);
        }
        asm volatile (
            "mov rsp, qword ptr fs:[8];"
            "call lapic_eoi;"
            "sti;"
            "1: "
            "hlt;"
            "jmp 1b;"
        );
    }
}

void task_resched(struct ctx_t *ctx) {
    pid_t current_task = cpu_locals[current_cpu].current_task;
    pid_t last_task = current_task;

    if (current_task != -1) {
        struct thread_t *current_thread = task_table[current_task];
        /* Save current context */
        current_thread->active_on_cpu = -1;
        current_thread->ctx = *ctx;
        /* Save FPU context */
        fxsave(&current_thread->fxstate);
        /* Save user rsp */
        current_thread->ustack = cpu_locals[current_cpu].thread_ustack;
        /* Release lock on this thread */
        spinlock_release(&current_thread->lock);
    } else {
        current_task = 0;
    }

    /* Idle check */
    idle();
    /* Get to the next task */
    task_get_next(&current_task);

    struct cpu_local_t *cpu_local = &cpu_locals[current_cpu];
    struct thread_t *thread = task_table[current_task];

    cpu_local->current_task = current_task;
    cpu_local->current_thread = thread->tid;
    cpu_local->current_process = thread->process;

    cpu_local->thread_kstack = thread->kstack;
    cpu_local->thread_ustack = thread->ustack;

    thread->active_on_cpu = current_cpu;

    /* Restore FPU context */
    fxrstor(&thread->fxstate);

    spinlock_inc(&switched_cpus);
    while (spinlock_read(&switched_cpus) < smp_cpu_count);
    if (!current_cpu) {
        spinlock_release(&scheduler_lock);
    }

    /* Swap cr3, if necessary */
    if (task_table[last_task]->process != thread->process) {
        /* Switch cr3 and return to the thread */
        task_spinup(&thread->ctx, (size_t)process_table[thread->process]->pagemap->pagemap - MEM_PHYS_OFFSET);
    } else {
        /* Don't switch cr3 and return to the thread */
        task_spinup(&thread->ctx, 0);
    }
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

#define STACK_LOCATION_TOP ((size_t)0x0000700000000000)
#define STACK_SIZE ((size_t)32768)

/* Create thread from function pointer */
/* Returns thread ID, -1 on failure */
tid_t task_tcreate(pid_t pid, void *(*entry)(void *), void *arg) {
    /* Search for free thread ID in the process */
    tid_t new_tid;
    for (new_tid = 0; new_tid < MAX_THREADS; new_tid++) {
        if (!process_table[pid]->threads[new_tid] || process_table[pid]->threads[new_tid] == (void *)(-1))
            goto found_new_tid;
    }
    return -1;
found_new_tid:;

    /* Search for free global task ID */
    tid_t new_task_id;
    for (new_task_id = 0; new_task_id < MAX_TASKS; new_task_id++) {
        if (!task_table[new_task_id] || task_table[new_tid] == (void *)(-1))
            goto found_new_task_id;
    }
    return -1;
found_new_task_id:;

    /* Try to make space for this new thread */
    struct thread_t *new_thread;
    if ((new_thread = kalloc(sizeof(struct thread_t))) == 0) {
        return -1;
    }

    process_table[pid]->threads[new_tid] = new_thread;
    task_table[new_task_id] = new_thread;

    new_thread->active_on_cpu = -1;

    /* Set registers to defaults */
    if (pid)
        new_thread->ctx = default_usr_ctx;
    else
        new_thread->ctx = default_krnl_ctx;

    /* Set up a user stack for the thread */
    if (1) {
        size_t stack_guardpage = STACK_LOCATION_TOP -
                                 (STACK_SIZE + PAGE_SIZE/*guard page*/) * (new_tid + 1);
        size_t stack_bottom = stack_guardpage + PAGE_SIZE;
        for (size_t i = 0; i < STACK_SIZE / PAGE_SIZE; i++) {
            void *ptr = pmm_alloc(1);
            if (!ptr) {
                kfree(process_table[pid]->threads[new_tid]);
                process_table[pid]->threads[new_tid] = EMPTY;
                return -1;
            }
            map_page(process_table[pid]->pagemap,
                     (size_t)ptr,
                     (size_t)(stack_bottom + (i * PAGE_SIZE)),
                     pid ? 0x07 : 0x03);
        }
        /* Add a guard page */
        unmap_page(process_table[pid]->pagemap, stack_guardpage);
        new_thread->ctx.rsp = stack_bottom + STACK_SIZE;
    }

    /* Set up a kernel stack for the thread */
    if (pid) {
        size_t kstack_guardpage = KSTACK_LOCATION_TOP -
                                  (KSTACK_SIZE + PAGE_SIZE/*guard page*/) * (new_tid + 1);
        size_t kstack_bottom = kstack_guardpage + PAGE_SIZE;
        for (size_t i = 0; i < KSTACK_SIZE / PAGE_SIZE; i++) {
            void *ptr = pmm_alloc(1);
            if (!ptr) {
                kfree(process_table[pid]->threads[new_tid]);
                process_table[pid]->threads[new_tid] = EMPTY;
                return -1;
            }
            map_page(process_table[pid]->pagemap,
                     (size_t)ptr,
                     (size_t)(kstack_bottom + (i * PAGE_SIZE)),
                     0x03);
        }
        /* Add a guard page */
        unmap_page(process_table[pid]->pagemap, kstack_guardpage);
        new_thread->kstack = kstack_bottom + KSTACK_SIZE;
    }

    /* Set instruction pointer to entry point, and set first argument to arg */
    new_thread->ctx.rip = (size_t)entry;
    new_thread->ctx.rdi = (size_t)arg;

    kmemcpy(new_thread->fxstate, default_fxstate, 512);

    new_thread->tid = new_tid;
    new_thread->process = pid;
    spinlock_release(&new_thread->lock);

    task_count++;

    return new_tid;
}
