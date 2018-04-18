#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <pit.h>

uint64_t uptime_raw = 0;
uint64_t uptime_sec = 0;

void ksleep(uint64_t time) {
    /* implements sleep in milliseconds */

    uint64_t final_time = uptime_raw + (time * (PIT_FREQUENCY / 1000));

    while (uptime_raw < final_time) {
        asm volatile ("hlt");
    }

    return;
}
