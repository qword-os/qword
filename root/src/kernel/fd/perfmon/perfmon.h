#ifndef __PERFMON_H__
#define __PERFMON_H__

int perfmon_create(void);
int perfmon_attach(int);

struct perfmon_t {
    int refcount;
    int glob_fd;
    uint64_t cpu_time;
    uint64_t syscall_time;
    uint64_t mman_time;
    uint64_t io_time;
};

struct perfmon_timer_t {
    uint64_t start_time;
    uint64_t elapsed;
};

#define PERFMON_TIMER_INITIALIZER {0, 0}

void perfmon_ref(struct perfmon_t *);
void perfmon_unref(struct perfmon_t *);

void perfmon_timer_start(struct perfmon_timer_t *);
void perfmon_timer_stop(struct perfmon_timer_t *);

#endif // __PERFMON_H__
