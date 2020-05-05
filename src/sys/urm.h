#ifndef __URM_H__
#define __URM_H__

#include <lib/lock.h>
#include <lib/event.h>

void execve_send_request(pid_t, tid_t, const char *, const char **, const char **, event_t **, int **);
void exit_send_request(pid_t, int, int);
void userspace_request_monitor(void *);

#define WNOHANG 2

#endif
