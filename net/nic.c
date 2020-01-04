#include <net/nic.h>
#include <lib/klib.h>
#include <lib/cio.h>

extern int probe_rtl8139(void);
extern void init_rtl8139(void);

static int rtl8139_present;

void init_nic(void) {
    kprint(KPRN_INFO, "nic: Initializing devices");

    rtl8139_present = probe_rtl8139();

    if (!rtl8139_present) {
        init_rtl8139();
    }
}
