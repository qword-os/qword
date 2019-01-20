
#include <errno.h>
#include <fs.h>
#include <klib.h>
#include <panic.h>
#include <time.h>

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
    uint64_t mman_time;
    uint64_t io_time;
};

int perfmon_read(struct perfmon_t *perfmon, void *buf, size_t count) {
    if (count < sizeof(struct perfstats)) {
        errno = EINVAL;
        return -1;
    }

    struct perfstats ps;
    kmemset(&ps, 0, sizeof(struct perfstats));
    ps.mman_time = perfmon->mman_time;
    ps.io_time = perfmon->io_time;

    kmemcpy(buf, &ps, sizeof(struct perfstats));
    return sizeof(struct perfstats);
}

