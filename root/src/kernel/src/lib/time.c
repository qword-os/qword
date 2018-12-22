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

uint64_t mktime64(int year, int month, int day, int hour, int minute,
        int second) {
    uint64_t nanoPrefix = 1e9;
    year -= (month <= 2);
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    unsigned int yoe = (unsigned int)(year - era * 400);  // [0, 399]
    unsigned int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2)/5 + day - 1;  // [0, 365]
    unsigned int doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
    int64_t days = era * 146097 + ((uint64_t)doe) - 719468;

    return second * nanoPrefix + minute * 60 * nanoPrefix + hour * 3600 * nanoPrefix
            + days * 86400 * nanoPrefix;
}
