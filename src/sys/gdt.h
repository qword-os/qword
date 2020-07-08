#pragma once

#include <stddef.h>

void init_gdt(void);
void load_tss(size_t addr);
