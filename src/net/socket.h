#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <stdint.h>
#include <stddef.h>
#include <lib/dynarray.h>

/* structure that describes all the key info about a socket */
struct socket_descriptor_t {
    /* whether socket has been associated with a remote server */
    /* FIXME: change this to `used` ? */
    int valid;

    int domain; /* TODO: make this a generic socket api  @aurelian */
    int type;
    int proto; /* tcp or udp */

    uint16_t ipid;
    uint32_t source_ip;
    uint16_t source_port;
    uint32_t dest_ip;
    uint16_t dest_port;
};

struct in_addr;

/* maybe typedefs ? */
/* https://linux.die.net/man/7/ip */
struct sockaddr_in {
    uint8_t sin_len;
    uint8_t sin_family;
    uint16_t sin_port;
    uint16_t sin_addr;
    char sin_zero[8];
};

struct sockaddr {
    uint8_t sa_len;
    uint8_t sa_family;
    char sa_data[14];
};

struct socket_descriptor_t *socket_from_fd(int fd);

#endif
