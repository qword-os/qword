#include <net/nic.h>
#include <lib/klib.h>
#include <lib/cio.h>

extern uint8_t *rtl8139_mac;
extern int probe_rtl8139(void);
extern void init_rtl8139(void);

uint8_t *nic_mac = NULL;

void init_nic(void) {
    kprint(KPRN_INFO, "nic: Initializing devices");

    if (!probe_rtl8139()) {
        init_rtl8139();
        nic_mac = rtl8139_mac;
    }
}
