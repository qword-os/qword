#ifndef __CMDLINE_H__
#define __CMDLINE_H__

extern char *cmdline;

char *cmdline_get_value(char *buf, size_t limit, const char *key);

#endif
