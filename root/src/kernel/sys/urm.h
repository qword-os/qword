#ifndef __URM_H__
#define __URM_H__

#include <lib/lock.h>

void execve_send_request(pid_t, const char *, const char **, const char **, lock_t **, int **);
void exit_send_request(pid_t, int, int);
void userspace_request_monitor(void *);

#define WNOHANG 2

#endif
