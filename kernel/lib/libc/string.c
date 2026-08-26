#include <string.h>
#include <stdint.h>

void* memset(void* ptr, int value, size_t num)
{
    uint8_t* p = (uint8_t*)ptr;
    for (size_t i = 0; i < num; i++) {
        p[i] = (uint8_t)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < num; i++) {
        d[i] = s[i];
    }
    return dest;
}

size_t strlen(const char* str)
{
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strncpy(char* dest, const char* src, size_t num)
{
    size_t i;
    for (i = 0; i < num && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < num; i++) {
        dest[i] = '\0';
    }
    return dest;
}

void* memchr(const void* ptr, int value, size_t num)
{
    const uint8_t* p = (const uint8_t*)ptr;
    for (size_t i = 0; i < num; i++) {
        if (p[i] == (uint8_t)value) {
            return (void*)(p + i);
        }
    }
    return NULL;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num)
{
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;
    for (size_t i = 0; i < num; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

void* memmove(void* dest, const void* src, size_t num)
{
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d < s) {
        for (size_t i = 0; i < num; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = num; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    return dest;
}

char* strcat(char* dest, const char* src)
{
    size_t dest_len = strlen(dest);
    size_t i;
    for (i = 0; src[i]; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';
    return dest;
}

char* strcpy(char* dest, const char* src)
{
    return strncpy(dest, src, strlen(src) + 1);
}

char* strchr(const char* s, int c)
{
    while (*s) {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    if ((char)c == '\0') {
        return (char*)s;
    }
    return NULL;
}

char* strrchr(const char* s, int c)
{
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    if ((char)c == '\0') {
        return (char*)s;
    }
    return (char*)last;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strncat(char* dest, const char* src, size_t num)
{
    size_t dest_len = strlen(dest);
    size_t i;
    for (i = 0; i < num && src[i]; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';
    return dest;
}

char* strstr(const char* haystack, const char* needle)
{
    if (!needle[0]) {
        return (char*)haystack;
    }
    size_t needle_len = strlen(needle);
    for (const char* p = haystack; *p; p++) {
        if (strncmp(p, needle, needle_len) == 0) {
            return (char*)p;
        }
    }
    return NULL;
}

char* strtok(char* str, const char* delimiters)
{
    static char* last = NULL;
    char* token;

    if (str) {
        last = str;
    }

    if (!last || *last == '\0') {
        return NULL;
    }

    while (*last && strchr(delimiters, *last)) {
        last++;
    }

    if (*last == '\0') {
        return NULL;
    }

    token = last;

    while (*last && !strchr(delimiters, *last)) {
        last++;
    }

    if (*last) {
        *last = '\0';
        last++;
    }

    return token;
}
