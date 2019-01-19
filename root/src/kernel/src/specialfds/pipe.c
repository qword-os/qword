
#include <fs.h>
#include <klib.h>

#define PIPE_BUFFER_STEP    32768

int pipe_close(struct pipe_t *pipe) {
    spinlock_acquire(&pipe->lock);
    pipe->refcount--;
    if (pipe->refcount) {
        spinlock_release(&pipe->lock);
        return 0;
    }
    if (pipe->size)
        kfree(pipe->buffer);
    kfree(pipe);
    return 0;
}

int pipe_read(struct pipe_t *pipe, void *buf, size_t count) {
    spinlock_acquire(&pipe->lock);

    size_t pipe_size_in_steps = (pipe->size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;
    size_t new_pipe_size = pipe->size - count;
    size_t new_pipe_size_in_steps = (new_pipe_size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;

    if (count > pipe->size)
        count = pipe->size;

    kmemcpy(buf, pipe->buffer, count);

    kmemmove(pipe->buffer, pipe->buffer + count, count);

    if (new_pipe_size_in_steps < pipe_size_in_steps)
        pipe->buffer = krealloc(pipe->buffer, new_pipe_size_in_steps);

    pipe->size -= count;

    spinlock_release(&pipe->lock);
    return count;
}

int pipe_write(struct pipe_t *pipe, const void *buf, size_t count) {
    spinlock_acquire(&pipe->lock);

    size_t pipe_size_in_steps = (pipe->size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;
    size_t new_pipe_size = pipe->size + count;
    size_t new_pipe_size_in_steps = (new_pipe_size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;

    if (new_pipe_size_in_steps > pipe_size_in_steps)
        pipe->buffer = krealloc(pipe->buffer, new_pipe_size_in_steps);

    kmemcpy(pipe->buffer + pipe->size, buf, count);

    pipe->size += count;

    spinlock_release(&pipe->lock);
    return count;
}
