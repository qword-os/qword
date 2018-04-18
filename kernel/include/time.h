#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>
#include <stddef.h>

extern uint64_t uptime_raw;
extern uint64_t uptime_sec;

void ksleep(size_t);

#endif
