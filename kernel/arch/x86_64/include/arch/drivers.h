#ifndef ARCH_X86_64_DRIVERS_H
#define ARCH_X86_64_DRIVERS_H

#include <arch/types.h>

void drivers_init(void);
void drivers_register(const char* name, void* init_func);
void drivers_start(void);

#endif
