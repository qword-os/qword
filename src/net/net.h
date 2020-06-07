#ifndef __NET__NET_H__
#define __NET__NET_H__

#include <stdint.h>
#include <lib/dynarray.h>
#include <lib/endian.h>
#include <lib/types.h>

#define HTONS(n) ((uint16_t)(((((uint16_t)(n) & 0xFFu)) << 8u) | (((uint16_t)(n) & 0xFF00u) >> 8u)))
#define NTOHS(n) ((uint16_t)(((((uint16_t)(n) & 0xFFu)) << 8u) | (((uint16_t)(n) & 0xFF00u) >> 8u)))

#define HTONL(n) (((((unsigned long)(n) & 0xFFu)) << 24u) | \
                  ((((unsigned long)(n) & 0xFF00u)) << 8u) | \
                  ((((unsigned long)(n) & 0xFF0000u)) >> 8u) | \
                  ((((unsigned long)(n) & 0xFF000000u)) >> 24u))

#define NTOHL(n) (((((unsigned long)(n) & 0xFFu)) << 24u) | \
                  ((((unsigned long)(n) & 0xFF00u)) << 8u) | \
                  ((((unsigned long)(n) & 0xFF0000u)) >> 8u) | \
                  ((((unsigned long)(n) & 0xFF000000u)) >> 24u))

#define MAC_EQUAL(x, y) (memcmp(x, y, sizeof(mac_addr_t)) == 0)
#define IPV4_EQUAL(x, y) ((x).raw == (y).raw)

// mac address
typedef struct {
    uint8_t raw[6];
} __attribute__((packed)) mac_addr_t;

// ipv4 address
typedef union {
    uint32_t raw;
    uint8_t addr[4];
} __attribute__((packed)) ipv4_addr_t;

#define HTON_IPV4(ip) ((ipv4_addr_t){ .raw = HTONL((ip).raw) })
#define NTOH_IPV4(ip) ((ipv4_addr_t){ .raw = NTOHL((ip).raw) })

struct nic_calls_t {
    int (*send_packet)(int fd, void* packet, size_t length, uint64_t flags);
};

struct nic_t {
    struct nic_calls_t calls;
    int internal_fd;

    mac_addr_t mac_addr;
    ipv4_addr_t ipv4_addr;
    int subnet;

    ipv4_addr_t ipv4_gateway;

    uint64_t flags;
#define NIC_RX_IP_CS   (1u << 0u)
#define NIC_RX_UDP_CS  (1u << 1u)
#define NIC_RX_TCP_CS  (1u << 2u)
#define NIC_TX_IP_CS   (1u << 3u)
#define NIC_TX_UDP_CS  (1u << 4u)
#define NIC_TX_TCP_CS  (1u << 5u)
};

#define PKT_FLAG_IP_CS   (1u << 0u)
#define PKT_FLAG_UDP_CS  (1u << 1u)
#define PKT_FLAG_TCP_CS  (1u << 2u)

/* packet descriptor */
struct packet_t {
    struct nic_t *nic;
    size_t pkt_len;

    char *buf;
};

public_dynarray_prototype(struct nic_t *, nics);

void net_add_nic(struct nic_t *);
int net_best_nic(ipv4_addr_t, struct nic_t **);
int net_query_mac(ipv4_addr_t, mac_addr_t *);
int net_dispatch_pkt(struct packet_t *);
int addr_to_raw(char *);
char *raw_to_addr(int);
struct packet_t *pkt_new(void);

#endif //__NET__NET_H__
