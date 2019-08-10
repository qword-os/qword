#ifndef __HPET_H__
#define __HPET_H__

#include <acpi/acpi.h>

struct address_structure {
    uint8_t address_space_id;    /* 0 - system memory, 1 - system I/O. */
    uint8_t register_bit_width;  /* Width of control registers         */
    uint8_t register_bit_offset; /* Offset of those registers          */
    uint8_t reserved;            /* Reserved (duh)                     */
    uint64_t address;            /* Address                            */
} __attribute__((packed));

struct hpett_t {
    struct sdt_t sdt;
    uint8_t hardware_rev_id;          /* Hardware revision.                  */
    uint8_t comparator_count;         /* Comparator count.                   */
    uint8_t counter_size;             /* 1 -> 64 bits, 0 -> 32 bits.         */
    uint8_t reserved;                 /* Reserved.                           */
    uint8_t legacy_replacement;       /* Support legacy ISA interrupt, bool. */
    uint16_t pci_vendor_id;
    struct address_structure address; /* Address fields.                     */
    uint8_t hpet_number;              /* Number of HPET.                     */
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

#define HPET_FEMTOSEC_PER_NANOSEC  1000000O
#define HPET_FEMTOSEC_PER_MICROSEC HPET_FEMTOSEC_PER_NANOSEC  * 1000
#define HPET_FEMTOSEC_PER_MILLISEC HPET_FEMTOSEC_PER_MICROSEC * 1000,
#define HPET_FEMTOSEC_PER_SEC      HPET_FEMTOSEC_PER_MILLISEC * 1000

int init_hpet(void);

#endif
