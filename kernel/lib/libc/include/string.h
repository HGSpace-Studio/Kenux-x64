#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include <arch/types.h>

void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t num);
size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t num);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t num);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
void* memchr(const void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t num);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* str, const char* delimiters);

#endif
