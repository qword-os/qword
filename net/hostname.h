#ifndef __HOSTNAME_H__
#define __HOSTNAME_H__

#define MAX_HOSTNAME_LEN 256

extern char hostname[MAX_HOSTNAME_LEN];

void init_hostname(void);

#endif
