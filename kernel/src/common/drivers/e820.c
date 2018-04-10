#include <stdint.h>
#include <stddef.h>
#include <e820.h>
#include <klib.h>

void get_e820(void *);

uint64_t memory_size = 0;

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
    #ifdef __I386__
        int memory_limit_warning = 0;
    #endif

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
            #ifdef __I386__
                if (e820_map[i].base >= 0x100000000)
                    memory_limit_warning = 1;
            #endif
        }
    }

    kprint(KPRN_INFO, "e820: Total usable memory: %U MiB", memory_size / 1024 / 1024);

    #ifdef __I386__
        if (memory_limit_warning) {
            kprint(KPRN_WARN, "e820: Usable memory located above 4 GiB and using a i386 build.");
            kprint(KPRN_WARN, "e820: Any usable memory above 4 GiB will NOT be accessible.");
        }
    #endif

    return;
}
