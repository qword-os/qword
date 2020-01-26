#ifndef __NET__PROTO__IPV4_H__
#define __NET__PROTO__IPV4_H__

#include <stdint.h>
#include <net/net.h>

struct ipv4_hdr_t {
    uint8_t head_len : 4;
    uint8_t ver : 4;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t fragment;
#define IPV4_HEAD_DF_MASK      0x4000u
#define IPV4_HEAD_MF_MASK      0x2000u
#define IPV4_HEAD_OFFSET_MASK  0x1fffu
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    ipv4_addr_t src;
    ipv4_addr_t dst;
} __attribute__((packed));

#endif //__NET__PROTO__IPV4_H__
