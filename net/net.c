#include <net/proto/arp.h>
#include <lib/klib.h>
#include <acpi/lai/core/libc.h>
#include <sys/panic.h>
#include <lib/errno.h>
#include "net.h"

public_dynarray_new(struct nic_t*, nics);

// this is the nic that is used by default
static struct nic_t* default_nic;

void net_add_nic(struct nic_t* nic) {
    char buffer[2 * 6 + 10] = {0};
    lai_snprintf(buffer, sizeof(buffer), "%02x.%02x.%02x.%02x.%02x",
                 nic->mac_addr.raw[0],
                 nic->mac_addr.raw[1],
                 nic->mac_addr.raw[2],
                 nic->mac_addr.raw[3],
                 nic->mac_addr.raw[4],
                 nic->mac_addr.raw[5]);

    // generate link local address (rfc5735/rfc3927)
    nic->subnet = 16;
    nic->ipv4_addr.addr[0] = 169u;
    nic->ipv4_addr.addr[1] = 254u;
    nic->ipv4_addr.addr[2] = (rand32() % 254) + 1;
    nic->ipv4_addr.addr[3] = rand32() % 256;

    // log it
    kprint(KPRN_INFO, "net: added device %s [assigned %d.%d.%d.%d/%d]",
           buffer,
           nic->ipv4_addr.addr[0],
           nic->ipv4_addr.addr[1],
           nic->ipv4_addr.addr[2],
           nic->ipv4_addr.addr[3],
           nic->subnet);

    // add it to the nic list
    dynarray_add(struct nic_t*, nics, &nic);
}

int net_route_ipv4(ipv4_addr_t addr, struct nic_t** nic) {
    panic_unless(nic);
    *nic = NULL;

    spinlock_acquire(&nics_lock);

    // search for nic with the same network address
    for (size_t i = 0; i < nics_i; i++) {
        struct nic_t* cur_nic = nics[i]->data;
        uint32_t mask = ((1 << cur_nic->subnet) - 1);
        if ((cur_nic->ipv4_addr.raw & mask) == (addr.raw & mask)) {
            *nic = cur_nic;
            return 0;
        }
    }

    spinlock_release(&nics_lock);
    errno = EHOSTUNREACH;
    return -1;
}

int net_query_mac(ipv4_addr_t addr, mac_addr_t* mac) {
    struct nic_t* nic = NULL;
    if (net_route_ipv4(addr, &nic)) {
        // found the route, request ip
        return arp_query_ipv4(nic, addr, mac);
    } else {
        // search for the first gateway we have
        if (default_nic == NULL) {
            errno = EHOSTUNREACH;
            return -1;
        }

        // request the mac of the gateway
        return net_query_mac(default_nic->ipv4_gateway, mac);
    }
}
