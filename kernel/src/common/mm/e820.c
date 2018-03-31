#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <e820.h>
#include <klib.h>

void get_e820(void *);
void calculate_free_memory_size(void);

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
    
    calculate_free_memory_size();

    return;
}

void calculate_free_memory_size(void) {
    uint64_t total_size;

    for (size_t i = 0; e820_map[i].type; i++) {
        if (e820_map[i].type == 1) {
            // Usable memory.
            uint64_t size = e820_map[i].length;
            total_size += size;
        }
        else
            continue;
    }

    memory_size = total_size;

    kprint(KPRN_INFO, "mm: Total usable memory: %X bytes", total_size);
}
