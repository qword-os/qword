#include <fd/fd.h>
#include <net/proto/ipv4.h>
#include <lib/cmem.h>
#include <sys/panic.h>
#include <net/proto/ether.h>
#include <lib/event.h>
#include <net/socket.h>
#include <net/net.h>

public_dynarray_new(struct socket_t, sockets);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static ssize_t socket_recv(int fd, void* buf, size_t len, int flags) {
    ssize_t ret;

    struct socket_t* sock = dynarray_getelem(struct socket_t, sockets, fd);
    if(sock == NULL) {
        errno = EBADF;
        return -1;
    }

    // TODO: check if connected for TCP

    while (1) {
        spinlock_acquire(&sock->buffers_lock);

        // if there is no buffer then just release the spinlock and
        // await for one. if nowait is set then will not await and just
        // return -1
        if (sock->buffers == NULL) {
            spinlock_release(&sock->buffers_lock);

            // if has this flag just exit with errno that
            // indicates a block
            if (flags & MSG_DONTWAIT) {
                errno = EWOULDBLOCK;
                dynarray_unref(sockets, fd);
                return -1;
            }

            // await till has a buffer
            event_await(&sock->has_buffers);
            continue;
        }

        switch (sock->type) {
            // for dgram sockets get the min len and copy it to the buffer
            // also set that the returned amount of bytes
            case SOCK_DGRAM:
                ret = len > sock->buffers->len ? sock->buffers->len : len;
                memcpy(buf, sock->buffers->buf, ret);

                // only remove if not peek
                if (!(flags & MSG_PEEK)) {
                    struct socket_buffer_t* buf = sock->buffers;
                    sock->buffers = sock->buffers->next;
                    kfree(buf->buf);
                    kfree(buf);
                }

                spinlock_release(&sock->buffers_lock);
                dynarray_unref(sockets, fd);
                return ret;

            case SOCK_STREAM:
                // this will need some kind of a loop
                // TODO:
                panic_if("TODO");
                break;
        }
    }
}

static int socket_close(int fd) {
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

static int socket_read(int fd, void* buf, size_t len) {
    struct socket_t* sock = dynarray_getelem(struct socket_t, sockets, fd);
    if(sock == NULL) {
        errno = EBADF;
        return -1;
    }

    int ret = socket_recv(sock, buf, len, 0);

    dynarray_unref(sockets, fd);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel socket API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int socket(int domain, int type, int protocol) {
    // check the domain
    if (domain != AF_INET && domain != AF_PACKET) {
        errno = EAFNOSUPPORT;
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
        errno = EPROTONOSUPPORT;
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
    descriptor.fd_handler.read = socket_read;
    descriptor.fd_handler.recv = socket_recv;
    return fd_create(&descriptor);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Socket packet stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void socket_process_packet(struct packet_t* pkt) {
    int domain = -1;
    int protocol = pkt->network.type; // these are actually the same

    switch(pkt->datalink.type) {
        case ETHER_IPV4: domain = AF_INET; break;
    }

    size_t i = 0;
    struct socket_t* sock = NULL;
    while (1) {
        sock = dynarray_search(struct socket_t, sockets, &i,
                elem->domain == domain && elem->proto == protocol, i);
        if (sock == NULL) {
            break;
        }

        // additional domain related checks
        switch (sock->domain) {
            case AF_INET:
                if(sock->domain_ctx.inet.remote_address.raw != 0 &&
                !IPV4_EQUAL(sock->domain_ctx.inet.remote_address, pkt->network.src)) {
                    continue;
                }
                break;
        }

        // additional protocol related checks
        switch (sock->proto) {
            // udp checks
            case PROTO_UDP:
                if(sock->proto_ctx.udp.local_port != pkt->transport.dst) {
                    continue;
                }

                if(sock->proto_ctx.udp.remote_port != 0 &&
                    sock->proto_ctx.udp.remote_port != pkt->transport.src) {
                    continue;
                }

                break;

            // TODO: TCP
        }

        spinlock_acquire(&sock->buffers_lock);

        // copy the packet
        struct socket_buffer_t* buffer = kalloc(sizeof(struct socket_buffer_t));
        buffer->buf = kalloc(pkt->data_len);
        buffer->len = pkt->data_len;
        memcpy(buffer->buf, pkt->data, pkt->data_len);

        // link it
        sock->last_buffer->next = buffer;
        sock->last_buffer = buffer;

        dynarray_unref(sockets, i);
        spinlock_release(&sock->buffers_lock);

        // trigger that we have a buffer
        event_trigger(&sock->has_buffers);
    }
}
