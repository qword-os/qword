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
#include <time.h>
#include <pit.h>

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

void yield(uint64_t ms) {
    spinlock_acquire(&scheduler_lock);

    uint64_t yield_target = (uptime_raw + (ms * (PIT_FREQUENCY / 1000))) + 1;

    tid_t current_task = cpu_locals[current_cpu].current_task;
    task_table[current_task]->yield_target = yield_target;

    /* Send resched IPI to CPU 0 to trigger a reschedule */
    lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[0].lapic_id) << 24);
    lapic_write(APICREG_ICR0, IPI_RESCHED);

    /* Paranoia: make sure the IPI goes through, 1ms should be more than plenty */
    ksleep(1);
}

/* Search for a new task to run */
static inline tid_t task_get_next(tid_t current_task) {
    if (current_task != -1) {
        current_task++;
    } else {
        current_task = 0;
    }

    for (int64_t i = 0; i < task_count; ) {
        struct thread_t *thread = task_table[current_task];
        if (!thread) {
            /* End of task table, rewind */
            current_task = 0;
            thread = task_table[current_task];
        }
        if (thread == EMPTY) {
            /* This is an empty thread, skip */
            goto skip;
        }
        if (thread->yield_target > uptime_raw) {
            goto next;
        }
        if (!spinlock_test_and_acquire(&thread->lock)) {
            /* If unable to acquire the thread's lock, skip */
            goto next;
        }
        return current_task;
        next:
        i++;
        skip:
        if (++current_task == MAX_TASKS)
            current_task = 0;
    }

    return -1;
}

__attribute__((noinline)) static void _idle(void) {
    cpu_locals[current_cpu].current_task = -1;
    cpu_locals[current_cpu].current_thread = -1;
    cpu_locals[current_cpu].current_process = -1;
    spinlock_inc(&switched_cpus);
    while (spinlock_read(&switched_cpus) < smp_cpu_count);
    if (!current_cpu) {
        spinlock_release(&scheduler_lock);
    }
    asm volatile (
        "call lapic_eoi;"
        "sti;"
        "1: "
        "hlt;"
        "jmp 1b;"
    );
}

__attribute__((noinline)) static void idle(void) {
    /* This idle function swaps cr3 and rsp then calls _idle for technical reasons */
    asm volatile (
        "mov rbx, cr3;"
        "cmp rax, rbx;"
        "je 1f;"
        "mov cr3, rax;"
        "1: "
        "mov rsp, qword ptr gs:[8];"
        "call _idle;"
        :
        : "a" ((size_t)kernel_pagemap.pml4 - MEM_PHYS_OFFSET)
    );
    /* Dead call so GCC doesn't garbage collect _idle */
    _idle();
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
    }

    /* Get to the next task */
    current_task = task_get_next(current_task);
    /* If there's nothing to do, idle */
    if (current_task == -1)
        idle();

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

    /* Restore thread FS base */
    load_fs_base(thread->fs_base);

    spinlock_inc(&switched_cpus);
    while (spinlock_read(&switched_cpus) < smp_cpu_count);
    if (!current_cpu) {
        spinlock_release(&scheduler_lock);
    }

    /* Swap cr3, if necessary */
    if (task_table[last_task]->process != thread->process) {
        /* Switch cr3 and return to the thread */
        task_spinup(&thread->ctx, (size_t)process_table[thread->process]->pagemap->pml4 - MEM_PHYS_OFFSET);
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

void task_trigger_resched(struct ctx_t *ctx) {
    spinlock_test_and_acquire(&switched_cpus);

    for (int i = 1; i < smp_cpu_count; i++) {
        lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[i].lapic_id) << 24);
        lapic_write(APICREG_ICR0, IPI_RESCHED);
    }

    /* Call task_scheduler on the BSP */
    task_resched(ctx);
}

#define BASE_BRK_LOCATION ((size_t)0x0000780000000000)

/* Create process */
/* Returns process ID, -1 on failure */
pid_t task_pcreate(struct pagemap_t *pagemap) {
    spinlock_acquire(&scheduler_lock);

    /* Search for free process ID */
    pid_t new_pid;
    for (new_pid = 0; new_pid < MAX_PROCESSES - 1; new_pid++) {
        if (!process_table[new_pid] || process_table[new_pid] == (void *)(-1))
            goto found_new_pid;
    }
    goto err;

found_new_pid:
    /* Try to make space for this new task */
    if ((process_table[new_pid] = kalloc(sizeof(struct process_t))) == 0) {
        process_table[new_pid] = EMPTY;
        goto err;
    }

    struct process_t *new_process = process_table[new_pid];

    if ((new_process->threads = kalloc(MAX_THREADS * sizeof(struct thread_t *))) == 0) {
        kfree(new_process);
        process_table[new_pid] = EMPTY;
        goto err;
    }

    if ((new_process->file_handles = kalloc(MAX_FILE_HANDLES * sizeof(int))) == 0) {
        kfree(new_process->threads);
        kfree(new_process);
        process_table[new_pid] = EMPTY;
        goto err;
    }

    /* Initially, mark all file handles as unused */
    for (size_t i = 0; i < MAX_FILE_HANDLES; i++) {
        process_table[new_pid]->file_handles[i] = -1;
    }

    new_process->file_handles_lock = 1;

    /* Map the higher half into the process */
    for (size_t i = 256; i < 512; i++) {
        pagemap->pml4[i] = process_table[0]->pagemap->pml4[i];
    }

    kstrcpy(new_process->cwd, "/");
    new_process->cwd_lock = 1;

    new_process->cur_brk = BASE_BRK_LOCATION;
    new_process->cur_brk_lock = 1;

    new_process->pagemap = pagemap;
    new_process->pid = new_pid;

    spinlock_release(&scheduler_lock);
    return new_pid;

err:
    spinlock_release(&scheduler_lock);
    return -1;
}

/* Kill a thread in a given process */
/* Return -1 on failure */
int task_tkill(pid_t pid, tid_t tid) {
    spinlock_acquire(&scheduler_lock);

    if (!process_table[pid]->threads[tid] || process_table[pid]->threads[tid] == (void *)(-1)) {
        spinlock_release(&scheduler_lock);
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

    /* Send resched IPI to CPU 0 to trigger a reschedule */
    lapic_write(APICREG_ICR1, ((uint32_t)cpu_locals[0].lapic_id) << 24);
    lapic_write(APICREG_ICR0, IPI_RESCHED);

    /* Paranoia: make sure the IPI goes through, 1ms should be more than plenty */
    ksleep(1);

    return 0;
}

#define KSTACK_LOCATION_TOP ((size_t)0x0000800000000000)
#define KSTACK_SIZE ((size_t)32768)

#define STACK_LOCATION_TOP ((size_t)0x0000700000000000)
#define STACK_SIZE ((size_t)32768)

/* Create thread from function pointer */
/* Returns thread ID, -1 on failure */
tid_t task_tcreate(pid_t pid, enum tcreate_abi abi, const void *opaque_data) {
    spinlock_acquire(&scheduler_lock);

    /* Search for free thread ID in the process */
    tid_t new_tid;
    for (new_tid = 0; new_tid < MAX_THREADS; new_tid++) {
        if (!process_table[pid]->threads[new_tid] || process_table[pid]->threads[new_tid] == (void *)(-1))
            goto found_new_tid;
    }
    goto err;
found_new_tid:;

    /* Search for free global task ID */
    tid_t new_task_id;
    for (new_task_id = 0; new_task_id < MAX_TASKS; new_task_id++) {
        if (!task_table[new_task_id] || task_table[new_tid] == (void *)(-1))
            goto found_new_task_id;
    }
    goto err;
found_new_task_id:;

    /* Try to make space for this new thread */
    struct thread_t *new_thread;
    if ((new_thread = kalloc(sizeof(struct thread_t))) == 0) {
        goto err;
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
        /* Virtual addresses of the stack. */
        size_t stack_guardpage = STACK_LOCATION_TOP -
                                 (STACK_SIZE + PAGE_SIZE/*guard page*/) * (new_tid + 1);
        size_t stack_bottom = stack_guardpage + PAGE_SIZE;

        /* Allocate physical memory for the stack and initialize it. */
        char *stack_pm = pmm_alloc(STACK_SIZE / PAGE_SIZE);
        if (!stack_pm) {
            kfree(process_table[pid]->threads[new_tid]);
            process_table[pid]->threads[new_tid] = EMPTY;
            goto err;
        }

        size_t *sbase = (size_t *)(stack_pm + STACK_SIZE + MEM_PHYS_OFFSET);
        panic_unless(!((size_t)sbase & 0xF) && "Stack base must be 16-byte aligned");

        size_t *sp;
        if (abi == tcreate_elf_exec) {
            const struct tcreate_elf_exec_data *data = opaque_data;

            /* Push all strings onto the stack. */
            char *strp = (char *)sbase;
            size_t nenv = 0;
            for (char **elem = data->envp; *elem; elem++) {
                kprint(KPRN_INFO, "Push envp %s", *elem);
                strp -= kstrlen(*elem) + 1;
                kstrcpy(strp, *elem);
                nenv++;
            }
            size_t nargs = 0;
            for (char **elem = data->argv; *elem; elem++) {
                kprint(KPRN_INFO, "Push argv %s", *elem);
                strp -= kstrlen(*elem) + 1;
                kstrcpy(strp, *elem);
                nargs++;
            }

            /* Align strp to 16-byte so that the following calculation becomes easier. */
            strp -= (size_t)strp & 0xF;

            /* Make sure the *final* stack pointer is 16-byte aligned.
                - The auxv takes a multiple of 16-bytes; ignore that.
                - There are 2 markers that each take 8-byte; ignore that, too.
                - Then, there is argc and (nargs + nenv)-many pointers to args/environ.
                  Those are what we *really* care about. */
            sp = (size_t *)strp;
            if ((nargs + nenv + 1) & 1)
                --sp;

            *(--sp) = 0; *(--sp) = 0; /* Zero auxiliary vector entry */
            sp -= 2; *sp = AT_ENTRY;    *(sp + 1) = data->auxval->at_entry;
            sp -= 2; *sp = AT_PHDR;     *(sp + 1) = data->auxval->at_phdr;
            sp -= 2; *sp = AT_PHENT;    *(sp + 1) = data->auxval->at_phent;
            sp -= 2; *sp = AT_PHNUM;    *(sp + 1) = data->auxval->at_phnum;

            size_t sa = (size_t)(stack_bottom + STACK_SIZE);
            *(--sp) = 0; /* Marker for end of environ */
            sp -= nenv;
            for (size_t i = 0; i < nenv; i++) {
                sa -= kstrlen(data->envp[i]) + 1;
                sp[i] = sa;
            }

            *(--sp) = 0; /* Marker for end of argv */
            sp -= nargs;
            for (size_t i = 0; i < nargs; i++) {
                sa -= kstrlen(data->argv[i]) + 1;
                sp[i] = sa;
            }
            *(--sp) = nargs; /* argc */
        }else{
            /* Do not push anything onto the stack. */
            sp = sbase;
        }
        panic_unless(!((size_t)sp & 0xF) && "Stack must be 16-byte aligned on x86_64");

        /* Map the stack */
        for (size_t i = 0; i < STACK_SIZE / PAGE_SIZE; i++) {
            map_page(process_table[pid]->pagemap,
                     (size_t)(stack_pm + (i * PAGE_SIZE)),
                     (size_t)(stack_bottom + (i * PAGE_SIZE)),
                     pid ? 0x07 : 0x03);
        }
        /* Add a guard page */
        unmap_page(process_table[pid]->pagemap, stack_guardpage);
        new_thread->ctx.rsp = stack_bottom + STACK_SIZE - ((sbase - sp) * sizeof(size_t));
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
                goto err;
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

    /* Set instruction pointer to entry point */
    if (abi == tcreate_fn_call) {
        const struct tcreate_fn_call_data *data = opaque_data;
        new_thread->ctx.rip = (size_t)data->fn;
        new_thread->ctx.rdi = (size_t)data->arg;
    } else {
        panic_unless(abi == tcreate_elf_exec);
        const struct tcreate_elf_exec_data *data = opaque_data;
        new_thread->ctx.rip = (size_t)data->entry;
    }

    kmemcpy(new_thread->fxstate, default_fxstate, 512);

    new_thread->fs_base = 0;

    new_thread->tid = new_tid;
    new_thread->process = pid;
    spinlock_release(&new_thread->lock);

    task_count++;

    spinlock_release(&scheduler_lock);
    return new_tid;

err:
    spinlock_release(&scheduler_lock);
    return -1;
}
