#include <stdint.h>
#include <stddef.h>
#include <fs.h>
#include <klib.h>
#include <task.h>

struct pipe_t *pipe_open(void) {
    struct pipe_t *new_pipe = kalloc(sizeof(struct pipe_t));
    if (!new_pipe)
        return NULL;

    new_pipe->lock = 1;

    return new_pipe;
}

int pipe_close(struct pipe_t *pipe) {
    spinlock_acquire(&pipe->lock);
    pipe->refcount--;
    if (pipe->refcount) {
        pipe->term = 1;
        task_trigger_event(&pipe->event);
        spinlock_release(&pipe->lock);
        return 0;
    }
    if (pipe->size)
        kfree(pipe->buffer);
    kfree(pipe);
    return 0;
}

int pipe_read(struct pipe_t *pipe, void *buf, size_t count, int block) {
    spinlock_acquire(&pipe->lock);

    while (count > pipe->size) {
        if (block) {
            // block until there's enough data available
            spinlock_release(&pipe->lock);
            task_await_event(&pipe->event);
            spinlock_acquire(&pipe->lock);
            if (pipe->term) {
                count = pipe->size;
                break;
            }
        } else {
            count = pipe->size;
            break;
        }
    }

    size_t pipe_size_in_steps = (pipe->size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;
    size_t new_pipe_size = pipe->size - count;
    size_t new_pipe_size_in_steps = (new_pipe_size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;

    kmemcpy(buf, pipe->buffer, count);

    kmemmove(pipe->buffer, pipe->buffer + count, count);

    if (new_pipe_size_in_steps < pipe_size_in_steps)
        pipe->buffer = krealloc(pipe->buffer, new_pipe_size_in_steps);

    pipe->size -= count;

    spinlock_release(&pipe->lock);
    return count;
}

int pipe_write(struct pipe_t *pipe, const void *buf, size_t count, int block) {
    (void)block;

    spinlock_acquire(&pipe->lock);

    size_t pipe_size_in_steps = (pipe->size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;
    size_t new_pipe_size = pipe->size + count;
    size_t new_pipe_size_in_steps = (new_pipe_size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;

    if (new_pipe_size_in_steps > pipe_size_in_steps)
        pipe->buffer = krealloc(pipe->buffer, new_pipe_size_in_steps);

    kmemcpy(pipe->buffer + pipe->size, buf, count);

    pipe->size += count;

    task_trigger_event(&pipe->event);

    spinlock_release(&pipe->lock);
    return count;
}
