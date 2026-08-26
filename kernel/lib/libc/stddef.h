#ifndef STDDEF_H
#define STDDEF_H

#include <stdint.h>

#define NULL ((void*)0)
#define offsetof(type, member) __builtin_offsetof(type, member)

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#endif
