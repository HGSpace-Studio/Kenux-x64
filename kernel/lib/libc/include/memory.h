#ifndef LIBC_MEMORY_H
#define LIBC_MEMORY_H

#include <arch/types.h>

void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
void* memchr(const void* ptr, int value, size_t num);
void* memmove(void* dest, const void* src, size_t num);

#endif
