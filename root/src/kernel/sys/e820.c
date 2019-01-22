#include <stdint.h>
#include <stddef.h>
#include <sys/e820.h>
#include <lib/klib.h>

void get_e820(void *);

uint64_t memory_size = 0;

struct e820_entry_t e820_map[256];

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
    /* Get e820 memory map. */
    get_e820(e820_map);

    /* Print out memory map and find total usable memory. */
    for (size_t i = 0; e820_map[i].type; i++) {
        kprint(KPRN_INFO, "e820: [%X -> %X] : %X  <%s>", e820_map[i].base,
                                              e820_map[i].base + e820_map[i].length,
                                              e820_map[i].length,
                                              e820_type(e820_map[i].type));
        if (e820_map[i].type == 1) {
            memory_size += e820_map[i].length;
        }
    }

    kprint(KPRN_INFO, "e820: Total usable memory: %U MiB", memory_size / 1024 / 1024);

    return;
}
