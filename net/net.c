#include <net/net.h>
#include <string.h>
#include <lib/klib.h>
#include <lib/cio.h>

void print_mac(const char *head, mac_t mac) {
    kprint(KPRN_INFO, "%s %x:%x:%x:%x:%x:%x", head, mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]);
}

int are_macs_equal(mac_t mac1, mac_t mac2) {
    return !memcmp(mac1, mac2, 6);
}

void print_ipv4(const char *head, ipv4_t ip) {
    kprint(KPRN_INFO, "%s %d.%d.%d.%d", head, ip[0], ip[1], ip[2], ip[3]);
}
