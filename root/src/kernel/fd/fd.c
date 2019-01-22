#include <stdint.h>
#include <stddef.h>
#include <fd/fd.h>
#include <lib/lock.h>

dynarray_new(struct file_descriptor_t, file_descriptors);

int fd_create(struct file_descriptor_t *fd) {
    return dynarray_add(struct file_descriptor_t, file_descriptors, fd);
}

int dup(int fd) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    int new_intern_fd = file_descriptors[fd]->fd_handler.dup(intern_fd);
    if (new_intern_fd == -1)
        return -1;

    struct file_descriptor_t new_fd = {0};

    new_fd.intern_fd = new_intern_fd;
    new_fd.fd_handler = file_descriptors[fd]->fd_handler;

    return fd_create(&new_fd);
}

int readdir(int fd, struct dirent *buf) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    return file_descriptors[fd]->fd_handler.readdir(intern_fd, buf);
}

int read(int fd, void *buf, size_t len) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    return file_descriptors[fd]->fd_handler.read(intern_fd, buf, len);
}

int write(int fd, const void *buf, size_t len) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    return file_descriptors[fd]->fd_handler.write(intern_fd, buf, len);
}

int close(int fd) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    if (file_descriptors[fd]->fd_handler.close(intern_fd))
        return -1;
    spinlock_acquire(&file_descriptors_lock);
    kfree(file_descriptors[fd]);
    file_descriptors[fd] = 0;
    spinlock_release(&file_descriptors_lock);
    return 0;
}

int lseek(int fd, off_t offset, int type) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    return file_descriptors[fd]->fd_handler.lseek(intern_fd, offset, type);
}

int fstat(int fd, struct stat *st) {
    int intern_fd = file_descriptors[fd]->intern_fd;
    return file_descriptors[fd]->fd_handler.fstat(intern_fd, st);
}
