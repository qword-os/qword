#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <sys/smp.h>
#include <user/task.h>
#include <lib/lock.h>
#include <fd/vfs/vfs.h>
#include <fd/pipe/pipe.h>
#include <fd/perfmon/perfmon.h>
#include <user/task.h>
#include <mm/mm.h>
#include <lib/time.h>
#include <lib/errno.h>
#include <misc/tty.h>

static inline int privilege_check(size_t base, size_t len) {
    if ( base & (size_t)0x800000000000
     || (base + len) & (size_t)0x800000000000)
        return 1;
    else
        return 0;
}

void enter_syscall() {
    spinlock_acquire(&scheduler_lock);
    pid_t current_task = cpu_locals[current_cpu].current_task;
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct thread_t *thread = task_table[current_task];
    struct process_t *process = process_table[current_process];

    /* Account thread statistics since last syscall. */
    int64_t cputime_delta = thread->total_cputime - thread->accounted_cputime;
    thread->accounted_cputime = thread->total_cputime;
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->cpu_time, cputime_delta);
    spinlock_release(&process->perfmon_lock);

    thread->syscall_entry_time = uptime_raw;
}

void leave_syscall() {
    spinlock_acquire(&scheduler_lock);
    pid_t current_task = cpu_locals[current_cpu].current_task;
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct thread_t *thread = task_table[current_task];
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->usage_lock);
    time_t syscall_time = uptime_raw - thread->syscall_entry_time;
    process->own_usage.ru_stime.tv_sec += syscall_time / 1000;
    process->own_usage.ru_stime.tv_nsec += ((syscall_time -
                    process->own_usage.ru_stime.tv_sec) * 1000);
    spinlock_release(&process->usage_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->syscall_time,
                uptime_raw - thread->syscall_entry_time);
    spinlock_release(&process->perfmon_lock);
}

/* Prototype syscall: int syscall_name(struct ctx_t *ctx) */

/* Conventional argument passing: rdi, rsi, rdx, r10, r8, r9 */

int syscall_getrusage(struct ctx_t *ctx) {
    /* rdi: who
     * rsi: usage
     */
    if (privilege_check(ctx->rsi, sizeof(struct rusage_t))) {
        errno = EFAULT;
        return -1;
    }

    struct rusage_t *usage = (struct rusage_t *)ctx->rsi;
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);
    spinlock_acquire(&process->usage_lock);

    switch (ctx->rdi) {
        case RUSAGE_SELF:
            *usage = process->own_usage;
            break;
        case RUSAGE_CHILDREN:
            *usage = process->child_usage;
            break;
        default:
            spinlock_release(&process->usage_lock);
            errno = ENOSYS;
            return -1;
    }

    spinlock_release(&process->usage_lock);
    return 0;
}

int syscall_clock_gettime(struct ctx_t *ctx) {
    /* rdi: clk_id
     * rsi: timespec
     */
    if (privilege_check(ctx->rsi, sizeof(struct timespec))) {
        errno = EFAULT;
        return -1;
    }

    struct timespec *tp = (struct timespec *)ctx->rsi;
    tp->tv_sec = unix_epoch;
    tp->tv_nsec = 0;
    return 0;
}

int syscall_tcgetattr(struct ctx_t *ctx) {
    /* rdi: fd
     * rsi struct termios*
     */
    if (privilege_check(ctx->rsi, sizeof(struct termios_t))) {
        errno = EFAULT;
        return -1;
    }

    spinlock_acquire(&termios_lock);
    struct termios_t *buf = (struct termios_t *)ctx->rsi;
    *buf = termios;
    spinlock_release(&termios_lock);
    return 0;
}

int syscall_tcsetattr(struct ctx_t *ctx) {
    /* rdi: fd
     * rsi: optional_actions
     * rdx: struct termios*
     */
    if (privilege_check(ctx->rdx, sizeof(struct termios_t))) {
        errno = EFAULT;
        return -1;
    }

    struct termios_t *new_termios = (struct termios_t*) ctx->rdx;
    return tty_tcsetattr(ctx->rsi, new_termios);
}

int syscall_getcwd(struct ctx_t *ctx) {
    if (privilege_check(ctx->rdi, ctx->rsi)) {
        errno = EFAULT;
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    char *buf = (char *)ctx->rdi;
    size_t limit = (size_t)ctx->rsi;

    spinlock_acquire(&process->cwd_lock);
    if (kstrlen(process->cwd) + 1 > limit) {
        spinlock_release(&process->cwd_lock);
        errno = ERANGE;
        return -1;
    }

    kstrcpy(buf, process->cwd);
    spinlock_release(&process->cwd_lock);

    return 0;
}

int syscall_readdir(struct ctx_t *ctx) {
    struct perfmon_timer_t io_timer = PERFMON_TIMER_INITIALIZER;

    int fd = (int)ctx->rdi;
    struct dirent *buf = (struct dirent *)ctx->rsi;

    if (privilege_check(ctx->rsi, sizeof(struct dirent))) {
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[fd] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    perfmon_timer_start(&io_timer);
    size_t ret = readdir(process->file_handles[fd], buf);
    perfmon_timer_stop(&io_timer);

    spinlock_release(&process->file_handles_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->io_time, io_timer.elapsed);
    spinlock_release(&process->perfmon_lock);

    return ret;
}

int syscall_chdir(struct ctx_t *ctx) {
    char *new_path = (char *)ctx->rdi;

    if (privilege_check(ctx->rdi, kstrlen(new_path) + 1))
        return -1;

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    char abs_path[2048];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, new_path, process->cwd);
    spinlock_release(&process->cwd_lock);

    int fd = open(abs_path, O_RDONLY);
    if (fd == -1)
        /* errno is propagated from open() */
        return -1;

    struct stat st;
    fstat(fd, &st);

    close(fd);

    if (!(st.st_mode & S_IFDIR)) {
        errno = ENOTDIR;
        return -1;
    }

    spinlock_acquire(&process->cwd_lock);
    kstrcpy(process->cwd, abs_path);
    spinlock_release(&process->cwd_lock);
    return 0;
}

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
                if (status)
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

        task_await_event(&process->child_event);
    }
}

int syscall_exit(struct ctx_t *ctx) {
    pid_t current_process = cpu_locals[current_cpu].current_process;

    exit_send_request(current_process, ctx->rdi);

    for (;;) { yield(1000); }
}

int syscall_execve(struct ctx_t *ctx) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    lock_t *err_lock;
    int *err;

    /* FIXME check if filename and argv/envp are in userspace */

    char *path = (char *)ctx->rdi;

    char abs_path[2048];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, path, process->cwd);
    spinlock_release(&process->cwd_lock);

    execve_send_request(current_process,
        abs_path,
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
    struct perfmon_timer_t mm_timer = PERFMON_TIMER_INITIALIZER;

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

    perfmon_timer_start(&mm_timer);
    struct pagemap_t *new_pagemap = fork_address_space(old_process->pagemap);
    perfmon_timer_stop(&mm_timer);

    struct process_t *new_process = process_table[new_pid];

    new_process->ppid = current_process;

    pmm_free((void *)new_process->pagemap->pml4 - MEM_PHYS_OFFSET, 1);
    kfree(new_process->pagemap);

    new_process->pagemap = new_pagemap;

    spinlock_acquire(&old_process->perfmon_lock);
    if (old_process->active_perfmon) {
        perfmon_ref(old_process->active_perfmon);
        new_process->active_perfmon = old_process->active_perfmon;
    }
    spinlock_release(&old_process->perfmon_lock);

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

    spinlock_acquire(&old_process->perfmon_lock);
    if (old_process->active_perfmon)
        atomic_add_uint64_relaxed(&old_process->active_perfmon->mman_time, mm_timer.elapsed);
    spinlock_release(&old_process->perfmon_lock);

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
    struct perfmon_timer_t mm_timer = PERFMON_TIMER_INITIALIZER;

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

    perfmon_timer_start(&mm_timer);
    void *ptr = pmm_allocz(ctx->rsi);
    if (!ptr) {
        errno = ENOMEM;
        return (void *)0;
    }
    for (size_t i = 0; i < ctx->rsi; i++) {
        if (map_page(process->pagemap, (size_t)ptr + i * PAGE_SIZE, base_address + i * PAGE_SIZE, 0x07)) {
            pmm_free(ptr, ctx->rsi);
            errno = ENOMEM;
            return (void *)0;
        }
    }
    perfmon_timer_stop(&mm_timer);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->mman_time, mm_timer.elapsed);
    spinlock_release(&process->perfmon_lock);

    return (void *)base_address;
}

int syscall_debug_print(struct ctx_t *ctx) {
    // rdi: print type
    // rsi: string

    // Make sure the type isn't invalid
    if (ctx->rdi > KPRN_MAX_TYPE)
        return -1;

    // Make sure we're not trying to print memory that doesn't belong to us
    if (privilege_check(ctx->rsi, kstrlen((const char *)ctx->rsi) + 1))
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

int syscall_pipe(struct ctx_t *ctx) {
    int *pipefd = (int *)ctx->rdi;

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(ctx->rdi, sizeof(int) * 2))
        return -1;

    spinlock_acquire(&process->file_handles_lock);

    int sys_pipefd[2];
    pipe(sys_pipefd);

    int local_fd_read;
    for (local_fd_read = 0; process->file_handles[local_fd_read] != -1; local_fd_read++)
        if (local_fd_read + 1 == MAX_FILE_HANDLES) {
            close(sys_pipefd[0]);
            close(sys_pipefd[1]);
            spinlock_release(&process->file_handles_lock);
            return -1;
        }
    process->file_handles[local_fd_read] = sys_pipefd[0];

    int local_fd_write;
    for (local_fd_write = 0; process->file_handles[local_fd_write] != -1; local_fd_write++)
        if (local_fd_write + 1 == MAX_FILE_HANDLES) {
            close(sys_pipefd[0]);
            close(sys_pipefd[1]);
            spinlock_release(&process->file_handles_lock);
            return -1;
        }
    process->file_handles[local_fd_write] = sys_pipefd[1];

    pipefd[0] = local_fd_read;
    pipefd[1] = local_fd_write;

    spinlock_release(&process->file_handles_lock);
    return 0;
}

int syscall_open(struct ctx_t *ctx) {
    // rdi: path
    // rsi: mode
    struct perfmon_timer_t io_timer = PERFMON_TIMER_INITIALIZER;

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(ctx->rdi, kstrlen((const char *)ctx->rdi) + 1))
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

    perfmon_timer_start(&io_timer);
    int fd = open(abs_path, ctx->rsi);
    perfmon_timer_stop(&io_timer);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->io_time, io_timer.elapsed);
    spinlock_release(&process->perfmon_lock);

    if (fd < 0) {
        spinlock_release(&process->file_handles_lock);
        return fd;
    }

    process->file_handles[local_fd] = fd;
    //file_descriptors[fd].fdflags = (int)ctx->rsi;

    spinlock_release(&process->file_handles_lock);
    return local_fd;
}

// constants from mlibc: options/posix/include/fcntl.h
#define F_DUPFD 1
#define F_DUPFD_CLOEXEC 2
#define F_GETFD 3
#define F_SETFD 4
#define F_GETFL 5
#define F_SETFL 6
#define F_GETLK 7
#define F_SETLK 8
#define F_SETLKW 9
#define F_GETOWN 10
#define F_SETOWN 11

static int fcntl_dupfd(int fd, int lowest_fd, int cloexec) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    int old_fd_sys = process->file_handles[fd];
    spinlock_release(&process->file_handles_lock);

    if (old_fd_sys == -1) {
        errno = EBADF;
        return -1;
    }

    int new_fd;

    spinlock_acquire(&process->file_handles_lock);
    for (new_fd = lowest_fd; new_fd < MAX_FILE_HANDLES; new_fd++) {
        if (process->file_handles[new_fd] == -1)
            goto fnd;
    }

    // free handle not found
    spinlock_release(&process->file_handles_lock);
    errno = EINVAL;
    return -1;

fnd:;
    int new_fd_sys = dup(old_fd_sys);
    process->file_handles[new_fd] = new_fd_sys;
    spinlock_release(&process->file_handles_lock);
    return new_fd;
}

static int fcntl_getfd(int fd) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    int fd_sys = process->file_handles[fd];
    spinlock_release(&process->file_handles_lock);

    if (fd_sys == -1) {
        errno = EBADF;
        return -1;
    }

    //int ret = file_descriptors[fd_sys].fdflags;

    //return ret;
    return 0;
}

static int fcntl_setfd(int fd, int fdflags) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    int fd_sys = process->file_handles[fd];
    spinlock_release(&process->file_handles_lock);

    if (fd_sys == -1) {
        errno = EBADF;
        return -1;
    }

    //file_descriptors[fd_sys].fdflags = fdflags;

    return 0;
}

int syscall_fcntl(struct ctx_t *ctx) {
    int fd = (int)ctx->rdi;
    int cmd = (int)ctx->rsi;

    switch (cmd) {
        case F_DUPFD:
            kprint(KPRN_DBG, "fcntl(%d, F_DUPFD, %d);",
                    fd, (int)ctx->rdx);
            return fcntl_dupfd(fd, (int)ctx->rdx, 0);
        case F_DUPFD_CLOEXEC:
            kprint(KPRN_DBG, "fcntl(%d, F_DUPFD_CLOEXEC, %d);",
                    fd, (int)ctx->rdx);
            return fcntl_dupfd(fd, (int)ctx->rdx, 1);
        case F_GETFD:
            kprint(KPRN_DBG, "fcntl(%d, F_GETFD, %d);",
                    fd, (int)ctx->rdx);
            return fcntl_getfd(fd);
        case F_SETFD:
            kprint(KPRN_DBG, "fcntl(%d, F_SETFD, %d);",
                    fd, (int)ctx->rdx);
            return fcntl_setfd(fd, (int)ctx->rdx);
        case F_GETFL:
            kprint(KPRN_DBG, "fcntl(%d, F_GETFL, %d);",
                    fd, (int)ctx->rdx);
            break;
        case F_SETFL:
            kprint(KPRN_DBG, "fcntl(%d, F_SETFL, %d);",
                    fd, (int)ctx->rdx);
            break;
        case F_GETLK:
            kprint(KPRN_DBG, "fcntl(%d, F_GETLK, %d);",
                    fd, (int)ctx->rdx);
            break;
        case F_SETLK:
            kprint(KPRN_DBG, "fcntl(%d, F_SETLK, %d);",
                    fd, (int)ctx->rdx);
            break;
        case F_SETLKW:
            kprint(KPRN_DBG, "fcntl(%d, F_SETLKW, %d);",
                    fd, (int)ctx->rdx);
            break;
        case F_GETOWN:
            kprint(KPRN_DBG, "fcntl(%d, F_GETOWN, %d);",
                    fd, (int)ctx->rdx);
            break;
        case F_SETOWN:
            kprint(KPRN_DBG, "fcntl(%d, F_SETOWN, %d);",
                    fd, (int)ctx->rdx);
            break;
        default:
            break;
    }

    kprint(KPRN_ERR, "unsupported fcntl");
    errno = ENOSYS;
    return -1;
}

int syscall_dup2(struct ctx_t *ctx) {
    int old_fd = (int)ctx->rdi;
    int new_fd = (int)ctx->rsi;

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    int old_fd_sys = process->file_handles[old_fd];
    int new_fd_sys = process->file_handles[new_fd];
    spinlock_release(&process->file_handles_lock);

    if (old_fd_sys == -1) {
        errno = EBADF;
        return -1;
    }

    if (old_fd_sys == new_fd_sys) {
        return new_fd;
    }

    if (new_fd_sys != -1) {
        close(new_fd_sys);
    }

    new_fd_sys = dup(old_fd_sys);

    spinlock_acquire(&process->file_handles_lock);
    process->file_handles[new_fd] = new_fd_sys;
    spinlock_release(&process->file_handles_lock);

    return new_fd;
}

int syscall_close(struct ctx_t *ctx) {
    // rdi: fd

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (ctx->rdi >= MAX_FILE_HANDLES) {
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    int ret = close(process->file_handles[ctx->rdi]);
    if (ret == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
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

    if (ctx->rdi >= MAX_FILE_HANDLES) {
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
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
        errno = EBADF;
        return -1;
    }

    size_t ret = fstat(process->file_handles[ctx->rdi], (struct stat *)ctx->rsi);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

#define SYSCALL_IO_CAP 16777216     // cap reads and writes at 16M at a time

int syscall_read(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len
    struct perfmon_timer_t io_timer = PERFMON_TIMER_INITIALIZER;

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
        errno = EBADF;
        return -1;
    }

    perfmon_timer_start(&io_timer);
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
    perfmon_timer_stop(&io_timer);

    spinlock_release(&process->file_handles_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->io_time, io_timer.elapsed);
    spinlock_release(&process->perfmon_lock);

    return ptr;
}

int syscall_write(struct ctx_t *ctx) {
    // rdi: fd
    // rsi: buf
    // rdx: len
    struct perfmon_timer_t io_timer = PERFMON_TIMER_INITIALIZER;

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
        errno = EBADF;
        return -1;
    }

    perfmon_timer_start(&io_timer);
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
    perfmon_timer_stop(&io_timer);

    spinlock_release(&process->file_handles_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon)
        atomic_add_uint64_relaxed(&process->active_perfmon->io_time, io_timer.elapsed);
    spinlock_release(&process->perfmon_lock);

    return ptr;
}

int syscall_perfmon_create(struct ctx_t *ctx) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    int sys_fd = perfmon_create();
    if (sys_fd == -1)
        return -1;

    spinlock_acquire(&process->file_handles_lock);

    int local_fd;
    for (local_fd = 0; process->file_handles[local_fd] != -1; local_fd++)
        if (local_fd + 1 == MAX_FILE_HANDLES) {
            close(sys_fd);
            spinlock_release(&process->file_handles_lock);
            return -1;
        }
    process->file_handles[local_fd] = sys_fd;

    spinlock_release(&process->file_handles_lock);
    return local_fd;
}

int syscall_perfmon_attach(struct ctx_t *ctx) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[ctx->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    if (perfmon_attach(process->file_handles[ctx->rdi])) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    spinlock_release(&process->file_handles_lock);
    return 0;
}
