#include <net/nic.h>
#include <lib/klib.h>
#include <lib/cio.h>

extern int probe_rtl8139(void);
extern void init_rtl8139(void);
extern int probe_e1000(void);
extern void init_e1000(void);

void init_nic(void) {
    kprint(KPRN_INFO, "nic: Initializing devices");

    if (!probe_rtl8139()) {
        init_rtl8139();
    }

    if (!probe_e1000()) {
        init_e1000();
    }
}
