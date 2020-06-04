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

#define MAC_EQUAL(x, y) (memcmp(x, y, sizeof(struct mac_addr_t)) == 0)
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

// packet descriptor
/* struct packet_t {
    char *data;
    size_t data_len;

    struct nic_t *nic;

    // flags passed from the nic
    uint64_t nic_flags;
#define PKT_FLAG_IP_CS   (1u << 0u)
#define PKT_FLAG_UDP_CS  (1u << 1u)
#define PKT_FLAG_TCP_CS  (1u << 2u)

    // Datalink layer abstraction
    //  - ether
    struct {
        size_t size;
        int type;
        mac_addr_t dst;
        mac_addr_t src;
    } datalink;

    // Network layer abstraction
    //  - arp
    //  - ipv4
    struct {
        size_t size;
        int type;
        ipv4_addr_t dst;
        ipv4_addr_t src;
    } network;

    // Transport layer abstraction
    //  - TCP
    //  - UDP
    struct {
        size_t size;
        int src;
        int dst;
        uint32_t snd_sq;
        uint32_t snd_ack;
        uint32_t rcv_sq;
        uint16_t win_sz;
    } transport;
}; */

struct packet_t {
    char *buf;
    size_t pkt_len;
};

public_dynarray_prototype(struct nic_t*, nics);

/**
 * Add a network interface
 */
void net_add_nic(struct nic_t* nic);

/**
 * Route an ip address to the correct interface to send on
 */
int net_route_ipv4(ipv4_addr_t addr, struct nic_t** nic);
/**
 * Query the mac to send to for an ip address
 *
 * This also handles stuff like gateway
 */
int net_query_mac(ipv4_addr_t addr, mac_addr_t* mac);

#endif //__NET__NET_H__
