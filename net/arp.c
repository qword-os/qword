#include <net/arp.h>
#include <lib/klib.h>
#include <lib/cio.h>

void arp_handle_packet(void *packet, size_t length) {
    kprint(KPRN_INFO, "arp: Handling packet of %d bytes", length);
}
