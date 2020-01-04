#ifndef __MAC_H__
#define __MAC_H__

#include <stdint.h>

typedef uint8_t  mac_t[6];
typedef uint8_t ipv4_t[4];

void print_mac(const char *head, mac_t mac);
int are_macs_equal(mac_t mac1, mac_t mac2);
void print_ipv4(const char *head, ipv4_t ip);

#endif
