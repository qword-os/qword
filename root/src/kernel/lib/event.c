#include <lib/event.h>
#include <lib/lock.h>
#include <proc/task.h>
#include <sys/smp.h>

void force_resched(void);

void event_await(event_t *event) {
    spinlock_acquire(&scheduler_lock);
    if (spinlock_read(event)) {
        spinlock_dec(event);
        spinlock_release(&scheduler_lock);
        return;
    } else {
        struct thread_t *current_thread = task_table[cpu_locals[current_cpu].current_task];
        current_thread->event_ptr = event;
        spinlock_release(&scheduler_lock);
        force_resched();
    }
}

void event_trigger(event_t *event) {
    spinlock_inc(event);

    return;
}
