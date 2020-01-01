#include <net/mac.h>
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
