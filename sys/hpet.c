#include <acpi/acpi.h>
#include <sys/hpet.h>

/* O if failed, 1 if succeeded */
int init_hpet(void) {
    #if 0
    /* Find the HPET description table. */
    hpett_t *hpet_table =  acpi_find_sdt("HPET", 0);

    if (!hpet_table) {
        return 0;
    } else {
        /* Check that the HPET is valid for our uses */
        if (!hpet_table->legacy_replacement || !hpet_table->counter_size) {
            return 0;
        }

        /* Enable HPET */
        hpet->address->register_bit_offset
    }
    #endif
    return 1;
}
