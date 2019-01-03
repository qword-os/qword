#include <stdint.h>
#include <stddef.h>
#include <klib.h>
#include <smp.h>
#include <task.h>
#include <lock.h>
#include <fs.h>
#include <task.h>
#include <mm.h>
#include <time.h>

static inline int privilege_check(size_t base, size_t len) {
    if ( base & (size_t)0x800000000000
     || (base + len) & (size_t)0x800000000000)
        return 1;
    else
        return 0;
}

/* Prototype syscall: int syscall_name(struct ctx_t *ctx) */

/* Conventional argument passing: rdi, rsi, rdx, r10, r8, r9 */

// Macros from mlibc: options/posix/include/sys/wait.h
#define WCONTINUED 1
#define WNOHANG 2
#define WUNTRACED 4

int syscall_waitpid(struct ctx_t *ctx) {
    pid_t pid = (pid_t)ctx->rdi;
    int *status = (int *)ctx->rsi;
    int flags = (int)ctx->rdx;

    if (privilege_check(ctx->rsi, sizeof(int))) {
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    for (;;) {
        spinlock_acquire(&process->child_event_lock);
        for (size_t i = 0; i < process->child_event_i; i++) {
            if (process->child_events[i].pid == pid || pid == -1) {
                // found our event
                pid_t child_pid = process->child_events[i].pid;
                struct process_t *child_process = process_table[child_pid];
                *status = process->child_events[i].status;
                process->child_event_i--;
                for (size_t j = i; j < process->child_event_i; j++)
                    process->child_events[j] = process->child_events[j + 1];
                process->child_events = krealloc(process->child_events,
                    sizeof(struct child_event_t) * process->child_event_i);
                spinlock_release(&process->child_event_lock);
                spinlock_acquire(&scheduler_lock);
                kfree(child_process);
                process_table[child_pid] = (void *)(-1);
                spinlock_release(&scheduler_lock);
                return child_pid;
            }
        }
        spinlock_release(&process->child_event_lock);

        // event not found

        if (flags & WNOHANG) {
            return 0;
        }

        yield(50);
    }
}

int syscall_exit(struct ctx_t *ctx) {
    pid_t current_process = cpu_locals[current_cpu].current_process;

    exit_send_request(current_process, ctx->rdi);

    for (;;) { yield(1000); }
}

int syscall_execve(struct ctx_t *ctx) {
    pid_t current_process = cpu_locals[current_cpu].current_process;

    lock_t *err_lock;
    int *err;

    /* FIXME check if filename and argv/envp are in userspace */

    execve_send_request(current_process,
        (void *)ctx->rdi,
        (void *)ctx->rsi,
        (void *)ctx->rdx,
        &err_lock,
        &err);

    for (;;) {
        yield(10);
        spinlock_acquire(&scheduler_lock);
        spinlock_acquire(err_lock);
        if (*err)
            break;
        spinlock_release(err_lock);
        spinlock_release(&scheduler_lock);
    }

    spinlock_release(&scheduler_lock);
    kprint(0, "execve failed");

    /* error occurred */
    kfree((void *)err_lock);
    kfree(err);

    return -1;
}

int syscall_fork(struct ctx_t *ctx) {
    spinlock_acquire(&scheduler_lock);

    pid_t current_task = cpu_locals[current_cpu].current_task;

    struct thread_t *calling_thread = task_table[current_task];

    pid_t current_process = cpu_locals[current_cpu].current_process;

    struct process_t *old_process = process_table[current_process];

    spinlock_release(&scheduler_lock);
    pid_t new_pid = task_pcreate();
    if (new_pid == -1)
        return -1;
    spinlock_acquire(&scheduler_lock);

    struct pagemap_t *new_pagemap = fork_address_space(old_process->pagemap);

    struct process_t *new_process = process_table[new_pid];

    new_process->ppid = current_process;

    pmm_free((void *)new_process->pagemap->pml4 - MEM_PHYS_OFFSET, 1);
    kfree(new_process->pagemap);

    new_process->pagemap = new_pagemap;

    /* Copy relevant metadata over */
    kstrcpy(new_process->cwd, old_process->cwd);
    new_process->cur_brk = old_process->cur_brk;

    /* Duplicate all file handles */
    for (size_t i = 0; i < MAX_FILE_HANDLES; i++) {
        if (old_process->file_handles[i] == -1)
            continue;
        new_process->file_handles[i] = dup(old_process->file_handles[i]);
    }

    new_process->threads[0] = kalloc(sizeof(struct thread_t));
    struct thread_t *new_thread = new_process->threads[0];

    /* Search for free global task ID */
    tid_t new_task_id;
    for (new_task_id = 0; new_task_id < MAX_TASKS; new_task_id++) {
        if (!task_table[new_task_id] || task_table[new_task_id] == (void *)(-1))
            goto found_new_task_id;
    }
    //goto err;

found_new_task_id:
    task_table[new_task_id] = new_thread;

    new_thread->tid = 0;
    new_thread->task_id = new_task_id;
    new_thread->process = new_pid;
    new_thread->lock = 1;
    new_thread->yield_target = 0;
    new_thread->active_on_cpu = -1;
    /* TODO: fix this */
    new_thread->kstack = (size_t)kalloc(32768) + 32768;
    new_thread->fs_base = calling_thread->fs_base;
    new_thread->ctx = *ctx;
    new_thread->ctx.rax = 0;
    fxsave(&new_thread->fxstate);

    task_count++;

    spinlock_release(&scheduler_lock);

    return new_pid;
}

int syscall_set_fs_base(struct ctx_t *ctx) {
    // rdi: new fs base

    spinlock_acquire(&scheduler_lock);
    pid_t current_task = cpu_locals[current_cpu].current_task;
    struct thread_t *thread = task_table[current_task];

    thread->fs_base = ctx->rdi;
    load_fs_base(ctx->rdi);

    spinlock_release(&scheduler_lock);

    return 0;
}

void *syscall_alloc_at(struct ctx_t *ctx) {
    // rdi: virtual address / 0 for sbrk-like allocation
    // rsi: page count

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    size_t base_address;
    if (ctx->rdi) {
        base_address = ctx->rdi;
        if (privilege_check(base_address, ctx->rsi * PAGE_SIZE))
            return (void *)0;
    } else {
        spinlock_acquire(&process->cur_brk_lock);
        base_address = process->cur_brk;
        if (privilege_check(base_address, ctx->rsi * PAGE_SIZE)) {
            spinlock_release(&process->cur_brk_lock);
            return (void *)0;
        }
        process->cur_brk += ctx->rsi * PAGE_SIZE;
        spinlock_release(&process->cur_brk_lock);
    }

    for (size_t i = 0; i < ctx->rsi; i++) {
        void *ptr = pmm_alloc(1);
        if (!ptr)
            return (void *)0;
        if (map_page(process->pagemap, (size_t)ptr, base_address + i * PAGE_SIZE, 0x07)) {
            pmm_free(ptr, 1);
            return (void *)0;
        }
    }

    return (void *)base_address;
}

int syscall_debug_print(struct ctx_t *ctx) {
    // rdi: print type
    // rsi: string

    // Make sure the type isn't invalid
    if (ctx->rdi > KPRN_MAX_TYPE)
        return -1;

    // Make sure we're not trying to print memory that doesn't belong to us
    if (privilege_check(ctx->rsi, kstrlen((const char *)ctx->rsi)))
        return -1;

    kprint(ctx->rdi, "[%u:%u:%u] %s",
           cpu_locals[current_cpu].current_process,
           cpu_locals[current_cpu].current_thread,
           current_cpu,
           ctx->rsi);

    return 0;
}

pid_t syscall_getpid(void) {
    return cpu_locals[current_cpu].current_process;
}

pid_t syscall_getppid(void) {
    spinlock_acquire(&scheduler_lock);
    pid_t ret = process_table[CURRENT_PROCESS]->ppid;
    spinlock_release(&scheduler_lock);
    return ret;
}

int syscall_open(struct ctx_t *ctx) {
    // rdi: path
    // rsi: mode
    // rdx: perms

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(ctx->rdi, kstrlen((const char *)ctx->rdi)))
        return -1;

    spinlock_acquire(&process->file_handles_lock);

    int local_fd;

    for (local_fd = 0; process->file_handles[local_fd] != -1; local_fd++)
        if (local_fd + 1 == MAX_FILE_HANDLES) {
            spinlock_release(&process->file_handles_lock);
            return -1;
        }

    char abs_path[2048];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, (const char *)ctx->rdi, process->cwd);
    spinlock_release(&process->cwd_lock);
    int fd = open(abs_path, ctx->rsi, ctx->rdx);
    if (fd < 0) {
        spinlock_release(&process->file_handles_lock);
        return fd;
    }

    process->file_handles[local_fd] = fd;

    spinlock_release(&process->file_handles_lock);
    return local_fd;
}

int syscall_close(struct ctx_t *ctx) {
    // rdi: fd

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    int ret = close(process->file_handles[ctx->rdi]);
    if (ret < 0) {
        spinlock_release(&process->file_handles_lock);
        return ret;
    }

    process->file_handles[ctx->rdi] = -1;

    spinlock_release(&process->file_handles_lock);
    return 0;
}

int syscall_lseek(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: offset
    // rdx: type

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    size_t ret = lseek(process->file_handles[ctx->rdi], ctx->rsi, ctx->rdx);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

int syscall_fstat(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: struct stat

    if (privilege_check(ctx->rsi, sizeof(struct stat))) {
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    size_t ret = fstat(process->file_handles[ctx->rdi], (struct stat *)ctx->rsi);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

#define SYSCALL_IO_CAP 8192     // cap reads and writes at 8k at a time

int syscall_read(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(ctx->rsi, ctx->rdx)) {
        return -1;
    }

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    size_t ptr = 0;
    while (ptr < ctx->rdx) {
        size_t step;
        if (ptr + SYSCALL_IO_CAP > ctx->rdx)
            step = ctx->rdx % SYSCALL_IO_CAP;
        else
            step = SYSCALL_IO_CAP;
        size_t ret = read(process->file_handles[ctx->rdi], (void *)(ctx->rsi + ptr), step);
        ptr += ret;
        if (ret < step)
            break;
    }

    spinlock_release(&process->file_handles_lock);
    return ptr;
}

int syscall_write(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(ctx->rsi, ctx->rdx)) {
        return -1;
    }

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    size_t ptr = 0;
    while (ptr < ctx->rdx) {
        size_t step;
        if (ptr + SYSCALL_IO_CAP > ctx->rdx)
            step = ctx->rdx % SYSCALL_IO_CAP;
        else
            step = SYSCALL_IO_CAP;
        size_t ret = write(process->file_handles[ctx->rdi], (void *)(ctx->rsi + ptr), step);
        ptr += ret;
        if (ret < step)
            break;
    }

    spinlock_release(&process->file_handles_lock);
    return ptr;
}
