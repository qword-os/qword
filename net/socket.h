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
            ipv4_addr_t remote_address;
        } inet;
    } domain_ctx;

    // protocol related context
    union {
        struct {
            uint16_t local_port;
            uint16_t remote_port;
        } udp;
        struct {
            uint16_t local_port;
            uint16_t remote_port;
        } tcp;
    } proto_ctx;

    // these hold the messages
    event_t has_buffers;
    struct socket_buffer_t* buffers;
    struct socket_buffer_t* last_buffer;
    lock_t buffers_lock;
    size_t buffer_offset;
};

// TODO: we might wanna have different types of socket arrays
//       for better performance
public_dynarray_prototype(struct socket_t, sockets);

#define MSG_PEEK        (0x2)
#define MSG_DONTWAIT    (0x40)

/**
 * Process a packet at the socket level
 */
void socket_process_packet(struct packet_t* pkt);

int socket(int domain, int type, int protocol);
int recv(int sockfd, void* buf, size_t len, int flags);

#endif //__NET__SOCKET_H__
