#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>
#include <stddef.h>

typedef int64_t time_t;
typedef int64_t clockid_t;

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct timeval {
    time_t tv_sec;
    long tv_usec;
};

struct s_time_t {
    uint32_t seconds;
    uint32_t minutes;
    uint32_t hours;
    uint32_t days;
    uint32_t months;
    uint32_t years;
};

#define RUSAGE_SELF 1
#define RUSAGE_CHILDREN 2

struct rusage_t {
    struct timeval ru_utime; /* user CPU time used */
    struct timeval ru_stime; /* system CPU time used */
};

void bios_get_time(struct s_time_t *);

extern volatile uint64_t uptime_raw;
extern volatile uint64_t uptime_sec;
extern volatile uint64_t unix_epoch;

void ksleep(uint64_t);
uint64_t get_jdn(int, int, int);
uint64_t get_unix_epoch(int, int, int, int, int, int);
void add_timeval(struct timeval *, struct timeval *);
void add_usage(struct rusage_t *, struct rusage_t *);

#endif
