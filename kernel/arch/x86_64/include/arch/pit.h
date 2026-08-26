#ifndef ARCH_X86_64_PIT_H
#define ARCH_X86_64_PIT_H

#include <arch/types.h>

void pit_init(void);
void pit_sleep(uint64_t ms);
uint64_t pit_get_ticks(void);
void pit_set_rate(uint32_t hz);

#endif
