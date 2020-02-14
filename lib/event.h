#ifndef __EVENT_H__
#define __EVENT_H__

#include <lib/lock.h>
#include <sys/cpu.h>
#include <proc/task.h>
#include <lib/types.h>
#include <sys/pit.h>

static inline int events_await(event_t **event, int *out_events, int n) {
    int wake = 0;

    for(int i = 0; i < n; i++) {
        if(locked_read(event_t, event[i])) {
            wake = 1;
            locked_dec(event[i]);
            out_events[i] = 1;
        }
    }
    if(wake) {
        return 0;
    }

    struct thread_t *current_thread = task_table[cpu_locals[current_cpu].current_task];
    current_thread->event_num = n;
    current_thread->out_event_ptr = out_events;
    locked_write(event_t **, &current_thread->event_ptr, event);
    yield();
    if (locked_read(int, &current_thread->event_abrt))
        return -1;
    return 0;
}

static inline int events_await_timeout(event_t **event, int *out_events, int n, size_t timeout) {
    uint64_t yield_target = (uptime_raw + (timeout * (PIT_FREQUENCY_HZ / 1000))) + 1;
    struct thread_t *current_thread = task_table[cpu_locals[current_cpu].current_task];
    current_thread->event_timeout = yield_target;

    int ret = events_await(event, out_events, n);
    if(ret == -1) {
        return -1;
    } else {
        return current_thread->event_timeout == 0;
    }
}

static inline int event_await_timeout(event_t *event, size_t timeout) {
    event_t *evts[1] = {event};
    event_t evts_out[1] = {0};
    return events_await_timeout(evts, evts_out, 1, timeout);
}

static inline int event_await(event_t *event) {
    event_t *evts[1] = {event};
    event_t out_evts[1] = {0};
    return events_await(evts, out_evts, 1);
}

static inline void event_trigger(event_t *event) {
    locked_inc(event);
}

#endif
