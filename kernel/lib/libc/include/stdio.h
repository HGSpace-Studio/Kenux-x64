#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <arch/types.h>
#include <stdarg.h>

int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vsprintf(char* str, const char* format, va_list args);
int vsnprintf(char* str, size_t size, const char* format, va_list args);
int puts(const char* str);
int putchar(int c);

#endif
