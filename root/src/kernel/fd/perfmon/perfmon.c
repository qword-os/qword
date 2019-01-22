#include <lib/errno.h>
#include <fd/vfs/vfs.h>
#include <lib/klib.h>
#include <sys/panic.h>
#include <lib/time.h>

int perfmon_create(void) {/*
    struct vfs_handle_t fd;
    struct perfmon_t *perfmon = kalloc(sizeof(struct perfmon_t));
    perfmon->refcount = 1;

    fd.used = 1;
    fd.type = FD_PERFMON;
    fd.perfmon = perfmon;

    return create_fd(&fd);
*/}

int perfmon_attach(int fd) {/*
    if (file_descriptors[fd].type != FD_PERFMON) {
        errno = EINVAL;
        return -1;
    }

    struct perfmon_t *perfmon = file_descriptors[fd].perfmon;
    perfmon_ref(file_descriptors[fd].perfmon);

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
*/}

void perfmon_ref(struct perfmon_t *perfmon) {/*
    int refs;
    atomic_fetch_add_int(&perfmon->refcount, &refs, 1);
    panic_unless(refs > 0 && "Object is already dead");
*/}

void perfmon_unref(struct perfmon_t *perfmon) {/*
    int refs;
    atomic_fetch_add_int(&perfmon->refcount, &refs, -1);
    panic_unless(refs > 0 && "Object is already dead");
    if (refs == 1)
        kfree(perfmon);
*/}

void perfmon_timer_start(struct perfmon_timer_t *timer) {/*
    panic_unless(!timer->start_time && "perfmon_timer_t has already been started");
    timer->start_time = uptime_raw;
*/}

void perfmon_timer_stop(struct perfmon_timer_t *timer) {/*
    panic_unless(timer->start_time && "perfmon_timer_t has not been started yet");
    timer->elapsed = uptime_raw - timer->start_time;
    timer->start_time = 0;
*/}

// Userspace ABI, has to match mlibc.
struct perfstats {
    uint64_t syscall_time;
    uint64_t mman_time;
    uint64_t io_time;
};

int perfmon_read(struct perfmon_t *perfmon, void *buf, size_t count) {/*
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
*/}
