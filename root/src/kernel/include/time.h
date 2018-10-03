#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>
#include <stddef.h>

struct s_time_t {
    uint32_t seconds;
    uint32_t minutes;
    uint32_t hours;
    uint32_t days;
    uint32_t months;
    uint32_t years;
};

void bios_get_time(struct s_time_t *);

extern volatile uint64_t uptime_raw;
extern volatile uint64_t uptime_sec;

void ksleep(uint64_t);

#endif
