#ifndef __PIPE_H__
#define __PIPE_H__

#include <stdint.h>
#include <stddef.h>
#include <lib/lock.h>
#include <user/task.h>

#define PIPE_BUFFER_STEP    32768

struct pipe_t {
    lock_t lock;
    void *buffer;
    size_t size;
    event_t event;
    int term;
    int refcount;
};

struct pipe_t *pipe_open(void);
int pipe_close(struct pipe_t *);
int pipe_read(struct pipe_t *, void *, size_t, int);
int pipe_write(struct pipe_t *, const void *, size_t, int);

#endif
