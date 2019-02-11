#ifndef __EVENT_H__
#define __EVENT_H__

#include <lib/lock.h>
#include <sys/cpu.h>
#include <proc/task.h>
#include <lib/types.h>

__attribute__((always_inline)) __attribute__((unused)) static inline void event_await(event_t *event) {
    spinlock_acquire(&scheduler_lock);
    if (locked_read(event_t, event)) {
        locked_dec(event);
        spinlock_release(&scheduler_lock);
        return;
    } else {
        struct thread_t *current_thread = task_table[cpu_locals[current_cpu].current_task];
        current_thread->event_ptr = event;
        force_resched();
    }
}

__attribute__((always_inline)) __attribute__((unused)) static inline void event_trigger(event_t *event) {
    locked_inc(event);
    return;
}

#endif
