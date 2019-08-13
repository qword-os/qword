#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <sys/smp.h>
#include <proc/task.h>
#include <lib/lock.h>
#include <fd/vfs/vfs.h>
#include <fd/pipe/pipe.h>
#include <proc/task.h>
#include <mm/mm.h>
#include <lib/time.h>
#include <lib/errno.h>
#include <lib/event.h>
#include <devices/term/tty/tty.h>
#include <sys/urm.h>
#include <net/hostname.h>

static inline int privilege_check(size_t base, size_t len) {
    if ( base & (size_t)0x800000000000
     || (base + len) & (size_t)0x800000000000)
        return 1;
    else
        return 0;
}

void enter_syscall(int syscall) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_task = cpu_locals[current_cpu].current_task;
    struct thread_t *thread = task_table[current_task];

    while (locked_read(int, &thread->event_abrt)) {
        spinlock_release(&scheduler_lock);
        yield();
        spinlock_acquire(&scheduler_lock);
    }

    locked_write(int, &thread->in_syscall, 1);
    thread->last_syscall = syscall;

    spinlock_release(&scheduler_lock);
}

void leave_syscall(void) {
    spinlock_acquire(&scheduler_lock);
    pid_t current_task = cpu_locals[current_cpu].current_task;
    struct thread_t *thread = task_table[current_task];

    int *in_syscall_ptr = &thread->in_syscall;

    spinlock_release(&scheduler_lock);

    locked_write(int, in_syscall_ptr, 0);
}

/* Prototype syscall: int syscall_name(struct regs_t *regs) */

/* Conventional argument passing: rdi, rsi, rdx, r10, r8, r9 */

int syscall_gethostname(struct regs_t *regs) {
    char *buf = (char *)regs->rdi;
    size_t len = (size_t)regs->rsi;

    if (privilege_check((size_t)buf, len)) {
        errno = EFAULT;
        return -1;
    }

    if (len > MAX_HOSTNAME_LEN)
        len = MAX_HOSTNAME_LEN;

    strncpy(buf, hostname, len);
    buf[len-1] = 0;

    size_t hostname_len = strlen(hostname);
    if (hostname_len < len)
        buf[hostname_len] = 0;

    return 0;
}

int syscall_futex_wait(struct regs_t *regs) {
    int *ptr = (int *)regs->rdi;
    int expected = (int)regs->rsi;

    kprint(0, "futex_wait(%X, %d);", ptr, expected);
    return 0;
}

int syscall_futex_wake(struct regs_t *regs) {
    int *ptr = (int *)regs->rdi;

    kprint(0, "futex_wake(%X);", ptr);
    return 0;
}

int syscall_sigaction(struct regs_t *regs) {
    int signum = (int)regs->rdi;
    struct sigaction *act = (void *)regs->rsi;
    struct sigaction *oldact = (void *)regs->rdx;

    spinlock_acquire(&scheduler_lock);
    pid_t pid = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[pid];

    if (oldact)
        *oldact = process->signal_handlers[signum];
    if (act)
        process->signal_handlers[signum] = *act;

    spinlock_release(&scheduler_lock);
    return 0;
}

int syscall_return_from_signal(void) {
    kprint(KPRN_INFO, "kernel: return from signal");
    task_tkill(CURRENT_PROCESS, CURRENT_THREAD);
}

int syscall_kill(struct regs_t *regs) {
    // rdi: pid
    // rsi: signal
    pid_t pid = (pid_t)regs->rdi;
    int signal = (int)regs->rsi;

    return kill(pid, signal);
}

int syscall_getrusage(struct regs_t *regs) {
    /* rdi: who
     * rsi: usage
     */
    if (privilege_check(regs->rsi, sizeof(struct rusage_t))) {
        errno = EFAULT;
        return -1;
    }

    struct rusage_t *usage = (struct rusage_t *)regs->rsi;
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);
    spinlock_acquire(&process->usage_lock);

    switch (regs->rdi) {
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

int syscall_clock_gettime(struct regs_t *regs) {
    /* rdi: clk_id
     * rsi: timespec
     */
    if (privilege_check(regs->rsi, sizeof(struct timespec))) {
        errno = EFAULT;
        return -1;
    }

    struct timespec *tp = (struct timespec *)regs->rsi;
    tp->tv_sec = unix_epoch;
    tp->tv_nsec = 0;
    return 0;
}

int syscall_tcgetattr(struct regs_t *regs) {
    /* rdi: fd
     * rsi struct termios*
     */
    if (privilege_check(regs->rsi, sizeof(struct termios))) {
        errno = EFAULT;
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (regs->rdi >= MAX_FILE_HANDLES) {
        errno = EBADF;
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    struct termios *new_termios = (struct termios *)regs->rsi;
    size_t ret = tcgetattr(process->file_handles[regs->rdi], new_termios);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

int syscall_tcsetattr(struct regs_t *regs) {
    /* rdi: fd
     * rsi: optional_actions
     * rdx: struct termios*
     */
    if (privilege_check(regs->rdx, sizeof(struct termios))) {
        errno = EFAULT;
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (regs->rdi >= MAX_FILE_HANDLES) {
        errno = EBADF;
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    struct termios *new_termios = (struct termios *)regs->rdx;
    size_t ret = tcsetattr(process->file_handles[regs->rdi], regs->rsi, new_termios);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

int syscall_tcflow(struct regs_t *regs) {
    /* rdi: fd
     * rsi: action
     */
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (regs->rdi >= MAX_FILE_HANDLES) {
        errno = EBADF;
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    int ret = tcflow(process->file_handles[regs->rdi], regs->rsi);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

int syscall_isatty(struct regs_t *regs) {
    /* rdi: fd
     */
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (regs->rdi >= MAX_FILE_HANDLES) {
        errno = EBADF;
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    int ret = isatty(process->file_handles[regs->rdi]);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

int syscall_getcwd(struct regs_t *regs) {
    if (privilege_check(regs->rdi, regs->rsi)) {
        errno = EFAULT;
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    char *buf = (char *)regs->rdi;
    size_t limit = (size_t)regs->rsi;

    spinlock_acquire(&process->cwd_lock);
    if (strlen(process->cwd) + 1 > limit) {
        spinlock_release(&process->cwd_lock);
        errno = ERANGE;
        return -1;
    }

    strcpy(buf, process->cwd);
    spinlock_release(&process->cwd_lock);

    return 0;
}

int syscall_readdir(struct regs_t *regs) {
    int fd = (int)regs->rdi;
    struct dirent *buf = (struct dirent *)regs->rsi;

    if (privilege_check(regs->rsi, sizeof(struct dirent))) {
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

    size_t ret = readdir(process->file_handles[fd], buf);

    spinlock_release(&process->file_handles_lock);

    return ret;
}

int syscall_chdir(struct regs_t *regs) {
    char *new_path = (char *)regs->rdi;

    if (privilege_check(regs->rdi, strlen(new_path) + 1))
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
    strcpy(process->cwd, abs_path);
    spinlock_release(&process->cwd_lock);
    return 0;
}

int syscall_waitpid(struct regs_t *regs) {
    pid_t pid = (pid_t)regs->rdi;
    int *status = (int *)regs->rsi;
    int flags = (int)regs->rdx;

    if (privilege_check(regs->rsi, sizeof(int))) {
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
                /* the child has been waited for so we need to add the usage */
                add_usage(&process->child_usage, &child_process->own_usage);
                add_usage(&process->child_usage, &child_process->child_usage);
                spinlock_release(&scheduler_lock);
                return child_pid;
            }
        }
        spinlock_release(&process->child_event_lock);

        // event not found

        if (flags & WNOHANG) {
            return 0;
        }

        if (event_await(&process->child_event)) {
            // signal is aborting us, bail
            errno = EINTR;
            return -1;
        }
    }
}

int syscall_exit(struct regs_t *regs) {
    pid_t current_process = cpu_locals[current_cpu].current_process;

    exit_send_request(current_process, regs->rdi, 0);

    locked_write(int, &task_table[CURRENT_TASK]->in_syscall, 0);

    for (;;) asm volatile ("hlt");
}

int syscall_execve(struct regs_t *regs) {
    /* FIXME check if filename and argv/envp are in userspace */

    spinlock_acquire(&scheduler_lock);
    int _current_cpu = current_cpu;
    pid_t current_process = cpu_locals[_current_cpu].current_process;
    tid_t current_thread = cpu_locals[_current_cpu].current_thread;
    tid_t current_task = cpu_locals[_current_cpu].current_task;
    struct process_t *process = process_table[current_process];
    struct thread_t *thread = task_table[current_task];
    spinlock_release(&scheduler_lock);

    char *path = (char *)regs->rdi;

    char abs_path[1024];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, path, process->cwd);
    spinlock_release(&process->cwd_lock);

    event_t *execve_error;
    int *call_errno;

    execve_send_request(
        current_process,
        current_thread,
        abs_path,
        (void *)regs->rsi,
        (void *)regs->rdx,
        &execve_error,
        &call_errno);

    while (event_await(execve_error) == -1) {
        locked_write(int, &thread->in_syscall, 0);
        errno = EIO;
    }

    /* error occurred */
    kprint(0, "execve failed");

    errno = *call_errno;

    kfree(call_errno);
    kfree(execve_error);

    return -1;
}

int syscall_fork(struct regs_t *regs) {
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

    free_address_space(new_process->pagemap);

    new_process->pagemap = new_pagemap;
    memset(&new_process->own_usage, 0, sizeof(struct rusage_t));
    memset(&new_process->child_usage, 0, sizeof(struct rusage_t));

    /* Copy relevant metadata over */
    strcpy(new_process->cwd, old_process->cwd);
    new_process->cur_brk = old_process->cur_brk;

    /* Duplicate all file handles */
    for (size_t i = 0; i < MAX_FILE_HANDLES; i++) {
        if (old_process->file_handles[i] == -1)
            continue;
        new_process->file_handles[i] = dup(old_process->file_handles[i]);
    }

    /* Copy signal handlers */
    for (size_t i = 0; i < SIGNAL_MAX; i++)
        new_process->signal_handlers[i].sa_handler =
            old_process->signal_handlers[i].sa_handler;
    new_process->sigmask = old_process->sigmask;

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
    new_thread->lock = new_lock;
    new_thread->yield_target = 0;
    new_thread->active_on_cpu = -1;
    /* TODO: fix this */
    new_thread->kstack = (size_t)kalloc(32768) + 32768;
    new_thread->fs_base = calling_thread->fs_base;
    new_thread->ctx.regs = *regs;
    new_thread->ctx.regs.rax = 0;
    fxsave(&new_thread->ctx.fxstate);

    task_count++;

    spinlock_release(&scheduler_lock);

    return new_pid;
}

int syscall_set_fs_base(struct regs_t *regs) {
    // rdi: new fs base

    spinlock_acquire(&scheduler_lock);
    pid_t current_task = cpu_locals[current_cpu].current_task;
    struct thread_t *thread = task_table[current_task];

    thread->fs_base = regs->rdi;
    load_fs_base(regs->rdi);

    spinlock_release(&scheduler_lock);

    return 0;
}

void *syscall_alloc_at(struct regs_t *regs) {
    // rdi: virtual address / 0 for sbrk-like allocation
    // rsi: page count
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    size_t base_address;
    if (regs->rdi) {
        base_address = regs->rdi;
        if (privilege_check(base_address, regs->rsi * PAGE_SIZE))
            return (void *)0;
    } else {
        spinlock_acquire(&process->cur_brk_lock);
        base_address = process->cur_brk;
        if (privilege_check(base_address, regs->rsi * PAGE_SIZE)) {
            spinlock_release(&process->cur_brk_lock);
            return (void *)0;
        }
        process->cur_brk += regs->rsi * PAGE_SIZE;
        spinlock_release(&process->cur_brk_lock);
    }

    void *ptr = pmm_allocz(regs->rsi);
    if (!ptr) {
        errno = ENOMEM;
        return (void *)0;
    }
    for (size_t i = 0; i < regs->rsi; i++) {
        if (map_page(process->pagemap, (size_t)ptr + i * PAGE_SIZE,
            base_address + i * PAGE_SIZE, 0x07, VMM_ATTR_REG)) {
            pmm_free(ptr, regs->rsi);
            errno = ENOMEM;
            return (void *)0;
        }
    }

    return (void *)base_address;
}

int syscall_debug_print(struct regs_t *regs) {
    // rdi: print type
    // rsi: string

    // Make sure the type isn't invalid
    if (regs->rdi > KPRN_MAX_TYPE)
        return -1;

    // Make sure we're not trying to print memory that doesn't belong to us
    if (privilege_check(regs->rsi, strlen((const char *)regs->rsi) + 1))
        return -1;

    kprint(regs->rdi, "[%u:%u:%u] %s",
           cpu_locals[current_cpu].current_process,
           cpu_locals[current_cpu].current_thread,
           current_cpu,
           regs->rsi);

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

int syscall_pipe(struct regs_t *regs) {
    int *pipefd = (int *)regs->rdi;
    int flflags = (int)regs->rsi;

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(pipefd, sizeof(int) * 2))
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
            errno = EMFILE;
            return -1;
        }
    process->file_handles[local_fd_read] = sys_pipefd[0];

    int local_fd_write;
    for (local_fd_write = 0; process->file_handles[local_fd_write] != -1; local_fd_write++)
        if (local_fd_write + 1 == MAX_FILE_HANDLES) {
            close(sys_pipefd[0]);
            close(sys_pipefd[1]);
            process->file_handles[local_fd_read] = -1;
            spinlock_release(&process->file_handles_lock);
            errno = EMFILE;
            return -1;
        }
    process->file_handles[local_fd_write] = sys_pipefd[1];

    pipefd[0] = local_fd_read;
    pipefd[1] = local_fd_write;

    spinlock_release(&process->file_handles_lock);

    if (flflags) {
        setflflags(sys_pipefd[0], flflags);
        setflflags(sys_pipefd[1], flflags);
    } else {
        setflflags(sys_pipefd[0], 0);
        setflflags(sys_pipefd[1], 0);
    }

    return 0;
}

int syscall_unlink(struct regs_t *regs) {
    // rdi: path

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    const char *path = (const char *)regs->rdi;

    if (privilege_check(regs->rdi, strlen(path) + 1)) {
        errno = EFAULT;
        return -1;
    }

    char abs_path[2048];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, path, process->cwd);
    spinlock_release(&process->cwd_lock);

    int fd = open(abs_path, O_RDONLY);
    if (fd < 0)
        return -1;

    unlink(fd);
    close(fd);
    return 0;
}

int syscall_mkdir(struct regs_t *regs) {
    // rdi: path

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    char abs_path[2048];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, (const char *)regs->rdi, process->cwd);
    spinlock_release(&process->cwd_lock);

    return mkdir(abs_path);
}

int syscall_open(struct regs_t *regs) {
    // rdi: path
    // rsi: mode
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(regs->rdi, strlen((const char *)regs->rdi) + 1)) {
        errno = EFAULT;
        return -1;
    }

    spinlock_acquire(&process->file_handles_lock);

    int local_fd;

    for (local_fd = 0; process->file_handles[local_fd] != -1; local_fd++)
        if (local_fd + 1 == MAX_FILE_HANDLES) {
            spinlock_release(&process->file_handles_lock);
            return -1;
        }

    char abs_path[2048];
    spinlock_acquire(&process->cwd_lock);
    vfs_get_absolute_path(abs_path, (const char *)regs->rdi, process->cwd);
    spinlock_release(&process->cwd_lock);

    int fd = open(abs_path, regs->rsi);

    if (fd < 0) {
        spinlock_release(&process->file_handles_lock);
        return fd;
    }

    process->file_handles[local_fd] = fd;
    //file_descriptors[fd].fdflags = (int)regs->rsi;

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

    return getfdflags(fd_sys);
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

    return setfdflags(fd_sys, fdflags);
}

static int fcntl_getfl(int fd) {
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

    return getflflags(fd_sys);
}

static int fcntl_setfl(int fd, int flflags) {
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

    return setflflags(fd_sys, flflags);
}

int syscall_fcntl(struct regs_t *regs) {
    int fd = (int)regs->rdi;
    int cmd = (int)regs->rsi;

    switch (cmd) {
        case F_DUPFD:
            kprint(KPRN_DBG, "fcntl(%d, F_DUPFD, %d);",
                    fd, (int)regs->rdx);
            return fcntl_dupfd(fd, (int)regs->rdx, 0);
        case F_DUPFD_CLOEXEC:
            kprint(KPRN_DBG, "fcntl(%d, F_DUPFD_CLOEXEC, %d);",
                    fd, (int)regs->rdx);
            return fcntl_dupfd(fd, (int)regs->rdx, 1);
        case F_GETFD:
            kprint(KPRN_DBG, "fcntl(%d, F_GETFD, %d);",
                    fd, (int)regs->rdx);
            return fcntl_getfd(fd);
        case F_SETFD:
            kprint(KPRN_DBG, "fcntl(%d, F_SETFD, %d);",
                    fd, (int)regs->rdx);
            return fcntl_setfd(fd, (int)regs->rdx);
        case F_GETFL:
            kprint(KPRN_DBG, "fcntl(%d, F_GETFL, %d);",
                    fd, (int)regs->rdx);
            return fcntl_getfl(fd);
        case F_SETFL:
            kprint(KPRN_DBG, "fcntl(%d, F_SETFL, %d);",
                    fd, (int)regs->rdx);
            return fcntl_setfl(fd, (int)regs->rdx);
        case F_GETLK:
            kprint(KPRN_DBG, "fcntl(%d, F_GETLK, %d);",
                    fd, (int)regs->rdx);
            break;
        case F_SETLK:
            kprint(KPRN_DBG, "fcntl(%d, F_SETLK, %d);",
                    fd, (int)regs->rdx);
            break;
        case F_SETLKW:
            kprint(KPRN_DBG, "fcntl(%d, F_SETLKW, %d);",
                    fd, (int)regs->rdx);
            break;
        case F_GETOWN:
            kprint(KPRN_DBG, "fcntl(%d, F_GETOWN, %d);",
                    fd, (int)regs->rdx);
            break;
        case F_SETOWN:
            kprint(KPRN_DBG, "fcntl(%d, F_SETOWN, %d);",
                    fd, (int)regs->rdx);
            break;
        default:
            break;
    }

    kprint(KPRN_ERR, "unsupported fcntl");
    errno = ENOSYS;
    return -1;
}

int syscall_dup2(struct regs_t *regs) {
    int old_fd = (int)regs->rdi;
    int new_fd = (int)regs->rsi;

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

int syscall_close(struct regs_t *regs) {
    // rdi: fd

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (regs->rdi >= MAX_FILE_HANDLES) {
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    int ret = close(process->file_handles[regs->rdi]);
    if (ret == -1) {
        spinlock_release(&process->file_handles_lock);
        return -1;
    }

    process->file_handles[regs->rdi] = -1;

    spinlock_release(&process->file_handles_lock);
    return 0;
}

int syscall_lseek(struct regs_t *regs) {
    // rdi: fd
    // rsi: offset
    // rdx: type

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (regs->rdi >= MAX_FILE_HANDLES) {
        return -1;
    }
    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    size_t ret = lseek(process->file_handles[regs->rdi], regs->rsi, regs->rdx);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

int syscall_fstat(struct regs_t *regs) {
    // rdi: fd
    // rsi: struct stat

    if (privilege_check(regs->rsi, sizeof(struct stat))) {
        return -1;
    }

    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    size_t ret = fstat(process->file_handles[regs->rdi], (struct stat *)regs->rsi);

    spinlock_release(&process->file_handles_lock);
    return ret;
}

#define SYSCALL_IO_CAP 16777216     // cap reads and writes at 16M at a time

int syscall_read(struct regs_t *regs) {
    // rdi: fd
    // rsi: buf
    // rdx: len
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(regs->rsi, regs->rdx)) {
        return -1;
    }

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    size_t ptr = 0;
    while (ptr < regs->rdx) {
        size_t step;
        if (ptr + SYSCALL_IO_CAP > regs->rdx)
            step = regs->rdx % SYSCALL_IO_CAP;
        else
            step = SYSCALL_IO_CAP;
        int ret = read(process->file_handles[regs->rdi], (void *)(regs->rsi + ptr), step);
        ptr += ret;
        if (ret < step)
            break;
    }

    spinlock_release(&process->file_handles_lock);

    return ptr;
}

int syscall_write(struct regs_t *regs) {
    // rdi: fd
    // rsi: buf
    // rdx: len
    spinlock_acquire(&scheduler_lock);
    pid_t current_process = cpu_locals[current_cpu].current_process;
    struct process_t *process = process_table[current_process];
    spinlock_release(&scheduler_lock);

    if (privilege_check(regs->rsi, regs->rdx)) {
        return -1;
    }

    spinlock_acquire(&process->file_handles_lock);
    if (process->file_handles[regs->rdi] == -1) {
        spinlock_release(&process->file_handles_lock);
        errno = EBADF;
        return -1;
    }

    size_t ptr = 0;
    while (ptr < regs->rdx) {
        size_t step;
        if (ptr + SYSCALL_IO_CAP > regs->rdx)
            step = regs->rdx % SYSCALL_IO_CAP;
        else
            step = SYSCALL_IO_CAP;
        int ret = write(process->file_handles[regs->rdi], (void *)(regs->rsi + ptr), step);
        ptr += ret;
        if (ret < step)
            break;
    }

    spinlock_release(&process->file_handles_lock);

    return ptr;
}
