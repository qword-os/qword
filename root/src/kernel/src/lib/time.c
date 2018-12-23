#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <pit.h>

volatile uint64_t uptime_raw = 0;
volatile uint64_t uptime_sec = 0;

void ksleep(uint64_t time) {
    /* implements sleep in milliseconds */

    uint64_t final_time = uptime_raw + (time * (PIT_FREQUENCY / 1000));

    final_time++;

    while (uptime_raw < final_time) {
        asm volatile ("hlt");
    }

    return;
}

uint64_t mktime64(int year0, int mon0, int day, int hour, int min,
        int sec) {
        unsigned int mon = mon0, year = year0;

    /* taken from linux/kernel/time/time.c */
    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int) (mon -= 2)) {
        mon += 12;	/* Puts Feb last since it has leap day */
        year -= 1;
    }

    return ((((uint64_t)
            (year/4 - year/100 + year/400 + 367*mon/12 + day) +
            year*365 - 719499
            )*24 + hour
            )*60 + min
            )*60 + sec;
}
