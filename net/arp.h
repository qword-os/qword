#ifndef __ARP_H__
#define __ARP_H__

#include <stddef.h>
#include <stdint.h>
#include <net/net.h>

struct arp_header {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t sender_hardware_address_len;
    uint8_t sender_protocol_address_len;
    uint16_t operation;
    mac_t sender_hardware_address;
    mac_t sender_protocol_address;
    mac_t target_hardware_address;
    mac_t target_protocol_address;
} __attribute__((packed));

struct arp_table_entry {
    mac_t mac_address;
    ipv4_t ip_address;
};

void arp_handle_packet(struct arp_header *packet, size_t length);

#endif
