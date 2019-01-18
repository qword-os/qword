#include <stdint.h>
#include <stddef.h>
#include <cio.h>
#include <klib.h>
#include <pci.h>
#include <mm.h>

static const uint16_t i8254x_devices[] = {
    // list of valid i8254x devices, vendor is 0x8086 (Intel)
    0x1000,    // 82542 (Fiber)
    0x1001,    // 82543GC (Fiber)
    0x1004,    // 82543GC (Copper)
    0x1008,    // 82544EI (Copper)
    0x1009,    // 82544EI (Fiber)
    0x100A,    // 82540EM
    0x100C,    // 82544GC (Copper)
    0x100D,    // 82544GC (LOM)
    0x100E,    // 82540EM
    0x100F,    // 82545EM (Copper)
    0x1010,    // 82546EB (Copper)
    0x1011,    // 82545EM (Fiber)
    0x1012,    // 82546EB (Fiber)
    0x1013,    // 82541EI
    0x1014,    // 82541ER
    0x1015,    // 82540EM (LOM)
    0x1016,    // 82540EP (Mobile)
    0x1017,    // 82540EP
    0x1018,    // 82541EI
    0x1019,    // 82547EI
    0x101a,    // 82547EI (Mobile)
    0x101d,    // 82546EB
    0x101e,    // 82540EP (Mobile)
    0x1026,    // 82545GM
    0x1027,    // 82545GM
    0x1028,    // 82545GM
    0x105b,    // 82546GB (Copper)
    0x1075,    // 82547GI
    0x1076,    // 82541GI
    0x1077,    // 82541GI
    0x1078,    // 82541ER
    0x1079,    // 82546GB
    0x107a,    // 82546GB
    0x107b,    // 82546GB
    0x107c,    // 82541PI
    0x10b5,    // 82546GB (Copper)
    0x1107,    // 82544EI
    0x1112,    // 82544GC
    0xffff
};

int init_net_i8254x(void) {
    // First of all see if there is such a NIC present
    for (int i = 0; i8254x_devices[i] != 0xffff; i++) {
        struct pci_device_t device = {0};
        if (!pci_get_device_by_vendor(&device, 0x8086, i8254x_devices[i])) {
            // Found!
            kprint(KPRN_INFO, "net: i8254x: Found NIC %x:%x", 0x8086, i8254x_devices[i]);
            // Bail out until PCI is fixed
            return;
            uint32_t bar0 = pci_read_device(&device, 0x4);
            if (!(bar0 & (1 << 2))) {
                kprint(KPRN_DBG, "enabling busmastering");
                bar0 |= (1 << 2);
                pci_write_device(&device, 0x4, bar0);
            }
            void *net_io_base = (void *)((size_t)(bar0 & 0xfffffff0));
            net_io_base += MEM_PHYS_OFFSET;
            kprint(KPRN_INFO, "net: i8254x: I/O base at %X", net_io_base);
            // Get MAC address
            uint8_t mac[6];
            for (int i = 0; i < 6; i++)
                mac[i] = ((uint8_t *)(net_io_base + 0x5400))[i];
            if (!mac[0] && !mac[1] && !mac[2] && !mac[3]) {
                // Get MAC via EPROM
                uint32_t xxx;
                *((volatile uint32_t *)(net_io_base + 0x14)) = 0x001;
                xxx = *((volatile uint32_t *)(net_io_base + 0x14));
                mac[0] = (uint8_t)(xxx >> 16);
                mac[1] = (uint8_t)(xxx >> 8);
                *((volatile uint32_t *)(net_io_base + 0x14)) = 0x101;
                xxx = *((volatile uint32_t *)(net_io_base + 0x14));
                mac[2] = (uint8_t)(xxx >> 16);
                mac[3] = (uint8_t)(xxx >> 8);
                *((volatile uint32_t *)(net_io_base + 0x14)) = 0x201;
                xxx = *((volatile uint32_t *)(net_io_base + 0x14));
                mac[4] = (uint8_t)(xxx >> 16);
                mac[5] = (uint8_t)(xxx >> 8);
            }
            kprint(KPRN_INFO, "net: i8254x: MAC address %x:%x:%x:%x:%x:%x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }
}
