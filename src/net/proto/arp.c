#include <lib/time.h>
#include <lib/event.h>
#include <net/net.h>
#include <net/netstack.h>
#include <lib/klib.h>
#include <lib/errno.h>
#include "arp.h"

struct ipv4_arp_packet_t {
    struct arp_hdr_t hdr;
    struct arp_ipv4_t ipv4;
} __attribute__((packed));

// 4 hour timeout by default
#define ARP_TIMEOUT (60 * 4)

struct arp_entry_t {
    uint64_t timestamp;
    mac_addr_t phys;
    ipv4_addr_t ip;
};

dynarray_new(struct arp_entry_t, arp_cache);

struct arp_request_t {
    ipv4_addr_t ip;
    event_t* event;
};

dynarray_new(struct arp_request_t, arp_requests);

/* check if ipv4 addr already in cache */
static int is_in_cache(ipv4_addr_t addr) {
    size_t i = 0;
    void *res = dynarray_search(struct arp_entry_t, arp_cache, &i,
            IPV4_EQUAL(elem->ip, addr), 0);

    if (res) {
        dynarray_unref(arp_cache, i);
        return 1;
    } else {
        return 0;
    }
}

static int send_arp_request(struct nic_t *nic, ipv4_addr_t addr, mac_addr_t *mac) {
/*    event_t event = 0;
    struct arp_request_t request = {
        .event = &event,
        .ip = addr
    };

    dynarray_add(struct arp_request_t, arp_requests, &request);

    // create the packet
    // TODO: ugly
    struct ipv4_arp_packet_t arp_request = {
        .hdr = {
            .opcode = ARP_OPCODE_REQUEST,
            .proto_addr_len = sizeof(ipv4_addr_t),
            .hw_addr_len = sizeof(mac_addr_t),
            .hw_type = ARP_HW_ETHER,
            .proto_type = ARP_PROTO_IPV4
        },
        .ipv4 = {
            .sender_ip = nic->ipv4_addr,
            .sender_mac = nic->mac_addr,
            .target_ip = addr,
            .target_mac = { {0} }
        }
    };

    // form a proper packet request
    struct packet_t pkt_req = {
        .nic = nic,
        .data = (char *)&arp_request,
        .data_len = sizeof(arp_request),
        .datalink.type = ETHER_ARP,
        .datalink.src = nic->mac_addr,
        .datalink.dst = (mac_addr_t){ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
    };

    // actually send it
    if (netstack_send_frame(&pkt_req) < 0) {
        kprint(KPRN_WARN, "arp: error sending arp request %d", errno);
        return -1;
    }

    // wait for the result
    event_await(&event);
    arp_query_ipv4(nic, addr, mac);

    return 0; */
}

void arp_process_packet(struct packet_t *pkt) {
/*    switch (pkt->network.type) {
        case ARP_OPCODE_REPLY: {

            // add to cache if not in cache
            if (!is_in_cache(pkt->network.src)) {
                // add the entry
                struct arp_entry_t entry = {
                    .timestamp = unix_epoch,
                    .ip = pkt->network.src,
                    .phys = pkt->datalink.src
                };
                dynarray_add(struct arp_entry_t, arp_cache, &entry);
            } else {
                kprint(KPRN_WARN, "arp: got reply for already existing address, ignoring");
                return;
            }

            // got over all related requests and trigger them
            struct arp_request_t *req = NULL;
            size_t i = 0;
            do {
                req = dynarray_search(struct arp_request_t, arp_requests, &i,
                                      IPV4_EQUAL(elem->ip, pkt->network.src), i);
                if (req == NULL) {
                    break;
                }

                event_trigger(req->event);
                dynarray_unref(arp_requests, i);
                dynarray_remove(arp_requests, i);

            } while (1);
        } break;

        case ARP_OPCODE_REQUEST: {
            // check if this is our ip
            if (IPV4_EQUAL(pkt->nic->ipv4_addr, pkt->network.dst)) {
                // add the requester as an entry
                struct arp_entry_t entry = {
                    .timestamp = unix_epoch,
                    .ip = pkt->network.src,
                    .phys = pkt->datalink.src
                };
                dynarray_add(struct arp_entry_t, arp_cache, &entry);

                // create the packet
                // TODO: ugly
                struct ipv4_arp_packet_t arp_request = {
                    .hdr = {
                        .opcode = ARP_OPCODE_REPLY,
                        .proto_addr_len = sizeof(ipv4_addr_t),
                        .hw_addr_len = sizeof(mac_addr_t),
                        .hw_type = ARP_HW_ETHER,
                        .proto_type = ARP_PROTO_IPV4
                    },
                    .ipv4 = {
                        .sender_ip = pkt->nic->ipv4_addr,
                        .sender_mac = pkt->nic->mac_addr,
                        .target_ip = pkt->network.src,
                        .target_mac = pkt->datalink.src
                    }
                };

                // form a proper packet request
                struct packet_t pkt_req = {
                    .nic = pkt->nic,
                    .data = (char *)&arp_request,
                    .data_len = sizeof(arp_request),
                    .datalink.type = ETHER_ARP,
                    .datalink.src = pkt->nic->mac_addr,
                    .datalink.dst = pkt->datalink.src
                };

                // actually send it
                if (netstack_send_frame(&pkt_req) < 0) {
                    kprint(KPRN_WARN, "arp: error sending arp reply %d", errno);
                }
            }
        } break;
    } */
}

int arp_query_ipv4(struct nic_t *nic, ipv4_addr_t addr, mac_addr_t *mac) {
    /* // first check in the cache if we have the ip
    int i = 0;
    struct arp_entry_t *entry = dynarray_search(struct arp_entry_t, arp_cache, &i, IPV4_EQUAL(elem->ip, addr), 0);

    if (entry) {
        // check the timeout on the entry
        if (unix_epoch - entry->timestamp > ARP_TIMEOUT) {
            dynarray_unref(arp_cache, i);
            dynarray_remove(arp_cache, i);
        }

        // we have it
        *mac = entry->phys;
        dynarray_unref(arp_cache, i);
        return 0;
    }

    // TODO: handle a case where we get two requests at the same time, instead of a
    //       double request we wanna have a single request

    // we don't have it, lets request it
    return send_arp_request(nic, addr, mac); */
}
