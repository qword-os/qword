#include <fd/fd.h>
#include "socket.h"

public_dynarray_new(struct socket_t, sockets);

int socket_close(int fd) {
    // get and remove it so no one can find it
    struct socket_t* sock = dynarray_getelem(struct socket_t, sockets, fd);
    if(sock == NULL) {
        errno = EBADF;
        return -1;
    }
    dynarray_remove(sockets, fd);

    spinlock_acquire(&sock->buffers_lock);

    // free left buffers
    struct socket_buffer_t* buf = sock->buffers;
    while (buf != NULL) {
        kfree(buf->buf);
        kfree(buf);

        buf = buf->next;
    }
    sock->buffers = NULL;

    // TODO: protocol related stuff to closing the socket

    // TODO: clear any events on people who wait on the socket

    spinlock_release(&sock->buffers_lock);

    // remove it once we done
    dynarray_unref(sockets, fd);
    return 0;
}

int socket(int domain, int type, int protocol) {
    // check the domain
    if (domain != AF_INET && domain != AF_PACKET) {
        errno = EINVAL;
        return -1;
    }

    // check the type
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        errno = EINVAL;
        return -1;
    }

    // ipv4/ipv6 + dgram = udp
    if (protocol == 0) {
        if ((domain == AF_INET) && type == SOCK_DGRAM) {
            protocol = PROTO_UDP;

            // ipv4/ipv6 + stream = tcp
        } else if ((domain == AF_INET) && type == SOCK_STREAM) {
            protocol = PROTO_TCP;
        }
    }

    // TODO: check the protocol type properly

    if (domain != AF_PACKET && protocol == 0) {
        errno = EINVAL;
        return -1;
    }

    // create the socket
    struct socket_t new_socket = {
        .domain = domain,
        .type = type,
        .proto = protocol,
        .buffers_lock = new_lock,
        .buffer_offset = 0
    };
    int i = dynarray_add(struct socket_t, sockets, &new_socket);

    // setup the handler
    struct file_descriptor_t descriptor = {0};
    descriptor.intern_fd = i;
    descriptor.fd_handler = default_fd_handler;
    descriptor.fd_handler.close = socket_close;
    return fd_create(&descriptor);
}
