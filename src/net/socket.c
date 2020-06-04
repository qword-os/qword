#include <stdint.h>
#include <stddef.h>
#include <net/socket.h>
#include <lib/dynarray.h>

/* a simple implementation of sockets for handling tcp and udp connections */
/* provides the follow functions:
 * int socket_connect(): Allows establishing a connection to a specific remote host
 * int socket_accept(): Accept incoming TCP or UDP connection
 * int socket_listen(): Setup socket to begin listening for incoming connections
 * int socket_bind(): Bind a socket to a certain address
 * int socket_new(): Create a new socket and return an associated file descriptor */

dynarray_new(struct socket_descriptor_t *, sockets);

/* translate fd to the socket descriptor it refers to */
struct socket_descriptor_t *socket_from_fd(int fd) {
    struct socket_descriptor_t **desc = dynarray_getelem(struct socket_descriptor_t *, sockets, fd);
    struct socket_descriptor_t *s = *desc;
    if (s->valid)
        return s;
    else
        return NULL;
}
