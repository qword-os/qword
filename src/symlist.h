#ifndef __SYMLIST_H__
#define __SYMLIST_H__

#include <stddef.h>

struct symlist_t {
    size_t addr;
    char *name;
};

extern struct symlist_t symlist[];

#endif
