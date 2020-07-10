#ifndef __CMDLINE_H__
#define __CMDLINE_H__

void  init_cmdline(const char *cmd);
char *cmdline_get_value(char *buf, size_t limit, const char *key);

#endif
