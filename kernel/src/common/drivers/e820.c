#include <stdint.h>
#include <stddef.h>
#include <e820.h>
#include <klib.h>

void get_e820(void *);

e820_entry_t e820_map[256];

static const char *e820_type(uint32_t type) {
    switch (type) {
        case 1:
            return "Usable RAM";
        case 2:
            return "Reserved";
        case 3:
            return "ACPI reclaimable";
        case 4:
            return "ACPI NVS";
        case 5:
            return "Bad memory";
        default:
            return "???";
    }
}

void init_e820(void) {
    /* get e820 memory map */
    get_e820(e820_map);

    /* print out memory map */
    for (size_t i = 0; e820_map[i].type; i++) {
        kprint(KPRN_INFO, "e820: [%X -> %X] : %X  <%s>", e820_map[i].base,
                                              e820_map[i].base + e820_map[i].length,
                                              e820_map[i].length,
                                              e820_type(e820_map[i].type));
    }

    return;
}
