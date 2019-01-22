#include <stdint.h>
#include <stddef.h>
#include <lib/klib.h>
#include <lib/lock.h>
#include <lib/errno.h>
#include <user/task.h>
#include <fd/fd.h>

#define PIPE_BUFFER_STEP    32768

struct pipe_t {
    lock_t lock;
    int fdflags;
    void *buffer;
    size_t size;
    event_t event;
    int term;
    int refcount;
};

dynarray_new(struct pipe_t, pipes);

static int pipe_close(int fd) {
    spinlock_acquire(&pipes_lock);
    struct pipe_t *pipe = pipes[fd];
    spinlock_release(&pipes_lock);

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
    pipes[fd] = 0;
    return 0;
}

static int pipe_read(int fd, void *buf, size_t count) {
    spinlock_acquire(&pipes_lock);
    struct pipe_t *pipe = pipes[fd];
    spinlock_release(&pipes_lock);

    spinlock_acquire(&pipe->lock);

    while (count > pipe->size) {
        if (pipe->fdflags & O_NONBLOCK) {
            count = pipe->size;
            break;
        } else {
            // block until there's enough data available
            spinlock_release(&pipe->lock);
            task_await_event(&pipe->event);
            spinlock_acquire(&pipe->lock);
            if (pipe->term) {
                count = pipe->size;
                break;
            }
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

static int pipe_write(int fd, const void *buf, size_t count) {
    spinlock_acquire(&pipes_lock);
    struct pipe_t *pipe = pipes[fd];
    spinlock_release(&pipes_lock);

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

static int pipe_lseek(int fd, off_t offset, int type) {
    (void)fd;
    (void)offset;
    (void)type;

    errno = ESPIPE;
    return -1;
}

static int pipe_dup(int fd) {
    spinlock_acquire(&pipes_lock);
    struct pipe_t *pipe = pipes[fd];
    spinlock_release(&pipes_lock);
    spinlock_acquire(&pipe->lock);
    pipe->refcount++;
    spinlock_release(&pipe->lock);
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
    st->st_atim.tv_sec = 0;
    st->st_atim.tv_nsec = 0;
    st->st_mtim.tv_sec = 0;
    st->st_mtim.tv_nsec = 0;
    st->st_ctim.tv_sec = 0;
    st->st_ctim.tv_nsec = 0;
    st->st_mode = 0;
    st->st_mode |= S_IFIFO;
    return 0;
}

static int pipe_readdir(int fd, struct dirent *buf) {
    (void)fd;
    (void)buf;

    errno = ENOTDIR;
    return -1;
}

static struct fd_handler_t pipe_functions = {
    pipe_close,
    pipe_fstat,
    pipe_read,
    pipe_write,
    pipe_lseek,
    pipe_dup,
    pipe_readdir
};

int pipe(int *pipefd) {
    struct pipe_t new_pipe;
    new_pipe.refcount = 2;
    new_pipe.lock = 1;

    int fd = dynarray_add(struct pipe_t, pipes, &new_pipe);
    if (fd == -1)
        return -1;

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
