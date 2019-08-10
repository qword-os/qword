#include <stdint.h>
#include <stddef.h>
#include <lib/time.h>
#include <sys/pit.h>

volatile uint64_t uptime_raw = 0;
volatile uint64_t uptime_sec = 0;
volatile uint64_t unix_epoch = 0;

void ksleep(uint64_t time) {
    /* implements sleep in milliseconds */

    uint64_t final_time = uptime_raw + (time * (PIT_FREQUENCY / 1000));

    final_time++;

    while (uptime_raw < final_time);

    return;
}

uint64_t get_jdn(int days, int months, int years) {
    return (1461 * (years + 4800 + (months - 14)/12))/4 + (367 *
            (months - 2 - 12 * ((months - 14)/12)))/12 - (3 * (
                    (years + 4900 + (months - 14)/12)/100))/4
            + days - 32075;
}

uint64_t get_unix_epoch(int seconds, int minutes, int hours,
                        int days, int months, int years) {

    uint64_t jdn_current = get_jdn(days, months, years);
    uint64_t jdn_1970 = get_jdn(1, 1, 1970);

    uint64_t jdn_diff = jdn_current - jdn_1970;

    return (jdn_diff * (60 * 60 * 24)) + hours*3600 + minutes*60 + seconds;
}

void add_timeval(struct timeval *val, struct timeval *to_add) {
    val->tv_sec += to_add->tv_sec;
    val->tv_usec += to_add->tv_usec;
    if (val->tv_usec >= 1000000) {
        val->tv_usec -= 1000000;
        val->tv_sec += 1;
    }
}

void add_usage(struct rusage_t *usage, struct rusage_t *to_add) {
    add_timeval(&usage->ru_stime, &to_add->ru_stime);
    add_timeval(&usage->ru_utime, &to_add->ru_utime);
}
