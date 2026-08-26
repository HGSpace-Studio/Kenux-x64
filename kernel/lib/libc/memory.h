#ifndef MEMORY_H
#define MEMORY_H

#include <arch/types.h>

void memory_init(void);
void* memory_alloc(uint64_t size);
void memory_free(void* ptr);
uint64_t memory_get_free(void);
uint64_t memory_get_total(void);

#endif
