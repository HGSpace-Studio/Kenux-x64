#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <arch/types.h>

int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);
double atof(const char* str);
int rand(void);
void srand(unsigned int seed);
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t size);
void abort(void);
void exit(int status);
int atexit(void (*func)(void));

#endif
