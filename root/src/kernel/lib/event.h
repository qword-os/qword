#ifndef __EVENT_H__
#define __EVENT_H__

#include <lib/lock.h>

typedef lock_t event_t;

void event_await(event_t *);
void event_trigger(event_t *);

#endif
