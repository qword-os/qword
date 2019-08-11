#include <stdint.h>
#include <stddef.h>
#include <acpi/acpi.h>
#include <sys/hpet.h>
#include <lib/klib.h>
#include <mm/mm.h>
#include <sys/apic.h>
#include <sys/panic.h>

struct hpet_table_t {
    struct sdt_t sdt;
    uint8_t hardware_rev_id;        /* Hardware revision.                  */
    uint8_t comparator_count:5;     /* Comparator count.                   */
    uint8_t counter_size:1;         /* 1 -> 64 bits, 0 -> 32 bits.         */
    uint8_t reserved:1;             /* Reserved.                           */
    uint8_t legacy_replacement:1;   /* Support legacy ISA interrupt, bool. */
    uint16_t pci_vendor_id;
    uint8_t address_space_id;       /* 0 - system memory, 1 - system I/O.  */
    uint8_t register_bit_width;     /* Width of control registers          */
    uint8_t register_bit_offset;    /* Offset of those registers           */
    uint8_t reserved1;              /* Reserved (duh)                      */
    uint64_t address;               /* Address                             */
    uint8_t hpet_number;            /* Number of HPET.                     */
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

struct hpet_timer_t {
    volatile uint64_t config_and_capabilities;
    volatile uint64_t comparator_value;
    volatile uint64_t fsb_interrupt_route;
    volatile uint64_t unused;
};

struct hpet_t {
    volatile uint64_t general_capabilities;
    volatile uint64_t unused0;
    volatile uint64_t general_configuration;
    volatile uint64_t unused1;
    volatile uint64_t general_int_status;
    volatile uint64_t unused2;
    volatile uint64_t unused3[2][12];
    volatile uint64_t main_counter_value;
    volatile uint64_t unused4;
    struct hpet_timer_t timers[];
};

static struct hpet_t *hpet;

void init_hpet(void) {
    uint64_t tmp;

    /* Find the HPET description table. */
    struct hpet_table_t *hpet_table = acpi_find_sdt("HPET", 0);

    if (!hpet_table)
        panic("HPET ACPI table not found", 0, 0, NULL);

    hpet = (struct hpet_t *)(hpet_table->address + MEM_PHYS_OFFSET);
    tmp = hpet->general_capabilities;

    /* Check that the HPET is valid for our uses */
    if (!(tmp & (1 << 15)))
        panic("HPET is not legacy replacement capable", 0, 0, NULL);

    uint64_t counter_clk_period = tmp >> 32;
    uint64_t frequency = 1000000000000000 / counter_clk_period;

    kprint(KPRN_INFO, "hpet: Detected frequency of %UHz", frequency);

    kprint(KPRN_INFO, "hpet: Enabling legacy replacement mode");
    tmp = hpet->general_configuration;
    tmp |= 0b10;
    hpet->general_configuration = tmp;

    hpet->main_counter_value = 0;

    kprint(KPRN_INFO, "hpet: Enabling interrupts on timer #0");
    tmp = hpet->timers[0].config_and_capabilities;
    if (!(tmp & (1 << 4)))
        panic("HPET timer #0 does not support periodic mode", 0, 0, NULL);

    tmp |= (1 << 2) | (1 << 3) | (1 << 6);
    hpet->timers[0].config_and_capabilities = tmp;
    hpet->timers[0].comparator_value = frequency / HPET_FREQUENCY_HZ;

    kprint(KPRN_INFO, "hpet: Enabling overall");
    tmp = hpet->general_configuration;
    tmp |= 0b01;
    hpet->general_configuration = tmp;

    kprint(KPRN_INFO, "hpet: Unmasking IRQ #0");
    io_apic_set_mask(0, 0, 1);
}
