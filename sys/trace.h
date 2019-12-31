#ifndef __SYS__TRACE_H__
#define __SYS__TRACE_H__

#include <stddef.h>

char *trace_address(size_t *off, size_t addr);
void print_stacktrace(int type);

#endif
