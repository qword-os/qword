#ifndef __PERFMON_H__
#define __PERFMON_H__

void perfmon_ref(struct perfmon_t *);
void perfmon_unref(struct perfmon_t *);

int perfmon_create();
int perfmon_attach(int);
int perfmon_read(struct perfmon_t *, void *, size_t);

struct perfmon_timer_t {
    uint64_t start_time;
    uint64_t elapsed;
};

#define PERFMON_TIMER_INITIALIZER {0, 0}

void perfmon_timer_start(struct perfmon_timer_t *);
void perfmon_timer_stop(struct perfmon_timer_t *);

#endif // __PERFMON_H__
