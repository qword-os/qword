#ifndef __NETSTACK_H__
#define __NETSTACK_H__

#include <stddef.h>
#include "net.h"

/**
 * Process a packet through the network stack
 *
 * will not assume ownership over the packet buffer and copy it if needed
 */
void netstack_process_frame(struct packet_t* pkt);

/**
 * Will send a single frame to the network
 */
int netstack_send_frame(struct packet_t* pkt);

#endif //__NETSTACK_H__
