#include <lib/errno.h>
#include <fd/fd.h>
#include <fd/perfmon/perfmon.h>
#include <lib/klib.h>
#include <sys/panic.h>
#include <lib/time.h>
#include <user/task.h>

dynarray_new(struct perfmon_t, perfmons);

void perfmon_ref(struct perfmon_t *perfmon) {
    int refs;
    atomic_fetch_add_int(&perfmon->refcount, &refs, 1);
    panic_unless(refs > 0 && "Object is already dead");
}

void perfmon_unref(struct perfmon_t *perfmon) {
    int refs;
    atomic_fetch_add_int(&perfmon->refcount, &refs, -1);
    panic_unless(refs > 0 && "Object is already dead");
    if (refs == 1)
        kfree(perfmon);
}

int perfmon_attach(int fd) {
    int intern_fd = file_descriptors[fd]->data->intern_fd;
    struct perfmon_t *perfmon = perfmons[intern_fd]->data;
    perfmon_ref(perfmon);

    spinlock_acquire(&scheduler_lock);
    struct process_t *process = process_table[CURRENT_PROCESS];
    spinlock_release(&scheduler_lock);

    spinlock_acquire(&process->perfmon_lock);
    if (process->active_perfmon) {
        spinlock_release(&process->perfmon_lock);
        perfmon_unref(perfmon);
        errno = EINVAL;
        return -1;
    }

    process->active_perfmon = perfmon;
    spinlock_release(&process->perfmon_lock);
    return 0;
}

void perfmon_timer_start(struct perfmon_timer_t *timer) {
    panic_unless(!timer->start_time && "perfmon_timer_t has already been started");
    timer->start_time = uptime_raw;
}

void perfmon_timer_stop(struct perfmon_timer_t *timer) {
    panic_unless(timer->start_time && "perfmon_timer_t has not been started yet");
    timer->elapsed = uptime_raw - timer->start_time;
    timer->start_time = 0;
}

// Userspace ABI, has to match mlibc.
struct perfstats {
    uint64_t syscall_time;
    uint64_t mman_time;
    uint64_t io_time;
};

static int perfmon_read(int fd, void *buf, size_t count) {
    struct perfmon_t *perfmon = perfmons[fd]->data;

    if (count < sizeof(struct perfstats)) {
        errno = EINVAL;
        return -1;
    }

    struct perfstats ps;
    kmemset(&ps, 0, sizeof(struct perfstats));
    ps.syscall_time = perfmon->syscall_time;
    ps.mman_time = perfmon->mman_time;
    ps.io_time = perfmon->io_time;

    kmemcpy(buf, &ps, sizeof(struct perfstats));
    return sizeof(struct perfstats);
}

static int perfmon_dup(int fd) {
    struct perfmon_t *perfmon = perfmons[fd]->data;
    perfmon_ref(perfmon);
    return fd;
}

static int perfmon_readdir(int fd, struct dirent *buf) {
    (void)fd;
    (void)buf;

    errno = ENOTDIR;
    return -1;
}

static int perfmon_write(int fd, const void *buf, size_t len) {
    (void)fd;
    (void)buf;
    (void)len;

    errno = EINVAL;
    return -1;
}

static int perfmon_close(int fd) {
    struct perfmon_t *perfmon = perfmons[fd]->data;
    perfmon_unref(perfmon);
    return 0;
}

static int perfmon_lseek(int fd, off_t offset, int type) {
    (void)fd;
    (void)offset;
    (void)type;

    errno = EINVAL;
    return -1;
}

static int perfmon_fstat(int fd, struct stat *st) {
    (void)fd;
    (void)st;

    errno = EINVAL;
    return -1;
}

static struct fd_handler_t perfmon_functions = {
    perfmon_close,
    perfmon_fstat,
    perfmon_read,
    perfmon_write,
    perfmon_lseek,
    perfmon_dup,
    perfmon_readdir
};

int perfmon_create(void) {
    struct perfmon_t perfmon = {0};
    perfmon.refcount = 1;

    int x = dynarray_add(struct perfmon_t, perfmons, &perfmon);
    if (x == -1)
        return -1;

    struct file_descriptor_t fd = {0};

    fd.intern_fd = x;
    fd.fd_handler = perfmon_functions;

    int glob_fd = fd_create(&fd);
    perfmons[x]->data->glob_fd = glob_fd;

    return glob_fd;
}
