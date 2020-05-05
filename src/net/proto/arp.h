#ifndef __ARP_H__
#define __ARP_H__

#include <stdint.h>
#include <net/net.h>
#include <net/proto/ether.h>

struct arp_hdr_t {
    uint16_t hw_type;
#define ARP_HW_ETHER            HTONS(1u)
    uint16_t proto_type;
#define ARP_PROTO_IPV4          ETHER_IPV4
    uint8_t hw_addr_len;
    uint8_t proto_addr_len;
    uint16_t opcode;
#define ARP_OPCODE_REQUEST      HTONS(1u)
#define ARP_OPCODE_REPLY        HTONS(2u)
} __attribute__((packed));

struct arp_ipv4_t {
    mac_addr_t sender_mac;
    ipv4_addr_t sender_ip;
    mac_addr_t target_mac;
    ipv4_addr_t target_ip;
} __attribute__((packed));

/**
 * This is called directly from the network stack
 */
void arp_process_packet(struct packet_t* pkt);

/**
 * Request the MAC address from the ipv4
 */
int arp_query_ipv4(struct nic_t* nic, ipv4_addr_t addr, mac_addr_t* mac);

#endif //__ARP_H__
