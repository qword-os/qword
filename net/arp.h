#ifndef __ARP_H__
#define __ARP_H__

#include <stddef.h>
#include <stdint.h>
#include <net/mac.h>

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
void arp_handle_packet(void *packet, size_t length);

#endif
