#ifndef __NET__PROTO__UDP_H__
#define __NET__PROTO__UDP_H__

#include <stdint.h>

struct udp_hdr_t {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

#endif //__NET_PROTO__UDP_H__
