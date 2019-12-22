#ifndef __E1000_H__
#define __E1000_H__

#include <stdint.h>

/* Structs for transmit and receipt buffers. */
#define E1000_TRANSMIT_COUNT 8
#define E1000_RECEIVE_COUNT 32

struct e1000_transmit {
        volatile uint64_t address;
        volatile uint16_t length;
        volatile uint8_t cso;
        volatile uint8_t cmd;
        volatile uint8_t status;
        volatile uint8_t css;
        volatile uint16_t special;
} __attribute__((packed));

struct e1000_receive {
        volatile uint64_t address;
        volatile uint16_t length;
        volatile uint16_t checksum;
        volatile uint8_t status;
        volatile uint8_t errors;
        volatile uint16_t special;
} __attribute__((packed));

/* The final E1000 structure. */
struct e1000 {
    uint64_t mem_base; /* MMIO Base Address. */
    int has_eeprom; /* Has EEPROM built-in. */
    uint8_t mac[6]; /* MAC address, once we get it. */
    struct e1000_transmit **transmits; /* Transmit buffers. */
    struct e1000_receive **receives; /* Receive buffers. */
    int current_transmit; /* Index of the current transmission. */
    int current_receive; /* Index of the current receive. */
};

/* Functions and global variables. */
extern int e1000_enabled;
void init_e1000(void);

#endif
