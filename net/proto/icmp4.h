#ifndef __NET_PROTO__ICMP4_H__
#define __NET_PROTO__ICMP4_H__

#include <stdint.h>

struct icmp4_hdr_t {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
} __attribute__((packed));

#define ICMP4_ECHO_REQUEST_TYPE 8
#define ICMP4_ECHO_REPLY_TYPE   0
struct icmp4_echo_t {
    uint16_t identifier;
    uint16_t sequence_number;
} __attribute__((packed));

#endif //__NET_PROTO__ICMP4_H__
