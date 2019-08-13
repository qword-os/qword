#ifndef __EVENT_H__
#define __EVENT_H__

#include <lib/lock.h>
#include <sys/cpu.h>
#include <proc/task.h>
#include <lib/types.h>

__attribute__((always_inline)) __attribute__((unused)) static inline int event_await(event_t *event) {
    if (locked_read(event_t, event)) {
        locked_dec(event);
        return 0;
    } else {
        struct thread_t *current_thread = task_table[cpu_locals[current_cpu].current_task];
        locked_write(event_t *, &current_thread->event_ptr, event);
        yield();
        if (locked_read(int, &current_thread->event_abrt))
            return -1;
        return 0;
    }
}

__attribute__((always_inline)) __attribute__((unused)) static inline void event_trigger(event_t *event) {
    locked_inc(event);
}

#endif
