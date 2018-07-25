#ifndef __TRACE_H__
#define __TRACE_H__

#include <stddef.h>

extern char debug_symbols_names[];
extern size_t debug_symbols_addresses[];

char *get_symbol_from_address(size_t *, size_t);
void print_stacktrace(int);

#endif
