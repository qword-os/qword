#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <lib/errno.h>
#include <proc/task.h>
#include <fd/fd.h>
#include <lib/event.h>

#define PIPE_BUFFER_STEP    32768

struct pipe_t {
    lock_t lock;
    int flflags;
    void *buffer;
    size_t size;
    event_t event;
    int refcount;
};

dynarray_new(struct pipe_t, pipes);

static int pipe_getflflags(int fd) {
    struct pipe_t *pipe = dynarray_getelem(struct pipe_t, pipes, fd);

    spinlock_acquire(&pipe->lock);
    int ret = pipe->flflags;
    spinlock_release(&pipe->lock);

    dynarray_unref(pipes, fd);
    return ret;
}

static int pipe_setflflags(int fd, int flflags) {
    struct pipe_t *pipe = dynarray_getelem(struct pipe_t, pipes, fd);

    spinlock_acquire(&pipe->lock);
    pipe->flflags = flflags;
    spinlock_release(&pipe->lock);

    dynarray_unref(pipes, fd);
    return 0;
}

static int pipe_close(int fd) {
    struct pipe_t *pipe = dynarray_getelem(struct pipe_t, pipes, fd);

    spinlock_acquire(&pipe->lock);
    pipe->refcount--;
    if (pipe->refcount) {
        event_trigger(&pipe->event);
        spinlock_release(&pipe->lock);
        dynarray_unref(pipes, fd);
        return 0;
    }
    if (pipe->size)
        kfree(pipe->buffer);

    dynarray_unref(pipes, fd);
    dynarray_remove(pipes, fd);
    return 0;
}

static int pipe_read(int fd, void *buf, size_t count) {
    struct pipe_t *pipe = dynarray_getelem(struct pipe_t, pipes, fd);

    spinlock_acquire(&pipe->lock);

    while (count > pipe->size) {
        if (pipe->flflags & O_NONBLOCK) {
            count = pipe->size;
            break;
        } else {
            if (pipe->refcount == 1) {
                count = pipe->size;
                break;
            }
            // block until there's enough data available
            spinlock_release(&pipe->lock);
            if (event_await(&pipe->event)) {
                // signal is aborting us, bail
                errno = EINTR;
                return -1;
            }
            spinlock_acquire(&pipe->lock);
        }
    }

    size_t pipe_size_in_steps = (pipe->size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;
    size_t new_pipe_size = pipe->size - count;
    size_t new_pipe_size_in_steps = (new_pipe_size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;

    memcpy(buf, pipe->buffer, count);

    memmove(pipe->buffer, pipe->buffer + count, count);

    if (new_pipe_size_in_steps < pipe_size_in_steps)
        pipe->buffer = krealloc(pipe->buffer, new_pipe_size_in_steps);

    pipe->size = new_pipe_size;

    spinlock_release(&pipe->lock);
    dynarray_unref(pipes, fd);
    return count;
}

static int pipe_write(int fd, const void *buf, size_t count) {
    struct pipe_t *pipe = dynarray_getelem(struct pipe_t, pipes, fd);

    spinlock_acquire(&pipe->lock);

    size_t pipe_size_in_steps = (pipe->size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;
    size_t new_pipe_size = pipe->size + count;
    size_t new_pipe_size_in_steps = (new_pipe_size + PIPE_BUFFER_STEP - 1) / PIPE_BUFFER_STEP;

    if (new_pipe_size_in_steps > pipe_size_in_steps)
        pipe->buffer = krealloc(pipe->buffer, new_pipe_size_in_steps);

    memcpy(pipe->buffer + pipe->size, buf, count);

    pipe->size = new_pipe_size;

    event_trigger(&pipe->event);

    spinlock_release(&pipe->lock);
    dynarray_unref(pipes, fd);
    return count;
}

static int pipe_lseek(int fd, off_t offset, int type) {
    (void)fd;
    (void)offset;
    (void)type;

    errno = ESPIPE;
    return -1;
}

static int pipe_dup(int fd) {
    struct pipe_t *pipe = dynarray_getelem(struct pipe_t, pipes, fd);
    spinlock_acquire(&pipe->lock);
    pipe->refcount++;
    spinlock_release(&pipe->lock);
    dynarray_unref(pipes, fd);
    return fd;
}

static int pipe_fstat(int fd, struct stat *st) {
    (void)fd;
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_nlink = 0;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    st->st_atim.tv_sec = unix_epoch;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = unix_epoch;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = unix_epoch;
    st->st_ctim.tv_nsec = 0;
    st->st_mode = 0;
    st->st_mode |= S_IFIFO;
    return 0;
}

int pipe(int *pipefd) {
    struct pipe_t new_pipe = {0};
    new_pipe.refcount = 2;
    new_pipe.lock = new_lock;

    int fd = dynarray_add(struct pipe_t, pipes, &new_pipe);
    if (fd == -1)
        return -1;

    struct fd_handler_t pipe_functions = default_fd_handler;
    pipe_functions.close = pipe_close;
    pipe_functions.fstat = pipe_fstat;
    pipe_functions.read = pipe_read;
    pipe_functions.write = pipe_write;
    pipe_functions.lseek = pipe_lseek;
    pipe_functions.dup = pipe_dup;
    pipe_functions.getflflags = pipe_getflflags;
    pipe_functions.setflflags = pipe_setflflags;

    struct file_descriptor_t fd_read = {0};
    struct file_descriptor_t fd_write = {0};

    fd_read.intern_fd = fd;
    fd_read.fd_handler = pipe_functions;

    fd_write.intern_fd = fd;
    fd_write.fd_handler = pipe_functions;

    pipefd[0] = fd_create(&fd_read);
    pipefd[1] = fd_create(&fd_write);

    return 0;
}
