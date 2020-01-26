#ifndef __NET__SOCKET_H__
#define __NET__SOCKET_H__

#include <stddef.h>
#include "net.h"

enum socket_domain {
    AF_INET = 2,
    AF_PACKET = 17
};

enum socket_type {
    SOCK_STREAM = 1,
    SOCK_DGRAM = 2
};

enum socket_proto {
    PROTO_TCP = HTONS(6),
    PROTO_UDP = HTONS(17)
};

struct socket_buffer_t {
    struct socket_buffer_t* next;
    char* buf;
    size_t len;
};

struct socket_t {
    int domain;
    int type;
    int proto;

    // context for the domain
    union {
        struct {
            ipv4_addr_t src;
            ipv4_addr_t dst;
        } inet;
    } domain_ctx;

    // protocol related context
    union {
        struct {
            uint16_t sport;
            uint16_t dport;
        } udp;
        struct {
            uint16_t sport;
            uint16_t dport;
        } tcp;
    } proto_ctx;

    // these hold the messages
    struct socket_buffer_t* buffers;
    struct socket_buffer_t* last_buffer;
    lock_t buffers_lock;
    size_t buffer_offset;
};

// TODO: we might wanna have different types of socket arrays
//       for better performance
public_dynarray_prototype(struct socket_t, sockets);

int socket(int domain, int type, int protocol);

#endif //__NET__SOCKET_H__
