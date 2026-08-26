#include <arch/win32.h>
#include <stdarg.h>
#include <string.h>
#include <arch/vga.h>
#include <arch/io.h>

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);
extern void* memory_alloc_aligned(uint64_t size, uint64_t alignment);
extern void* memory_zalloc(uint64_t size);

HANDLE CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess, uint32_t dwShareMode, void* lpSecurityAttributes, uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL ReadFile(HANDLE hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead, uint32_t* lpNumberOfBytesRead, void* lpOverlapped);
BOOL WriteFile(HANDLE hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite, uint32_t* lpNumberOfBytesWritten, void* lpOverlapped);
BOOL SetFilePointer(HANDLE hFile, int32_t lDistanceToMove, int32_t* lpDistanceToMoveHigh, uint32_t dwMoveMethod);
BOOL GetFileSizeEx(HANDLE hFile, int64_t* lpFileSize);
BOOL CloseHandle(HANDLE hObject);
BOOL DeleteFileA(const char* lpFileName);
BOOL MoveFileA(const char* lpExistingFileName, const char* lpNewFileName);
BOOL CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes);
BOOL RemoveDirectoryA(const char* lpPathName);
DWORD GetEnvironmentVariableA(const char* lpName, char* lpBuffer, DWORD nSize);

#define SERIAL_PORT 0x3F8

static void serial_putc(char c)
{
    int i;
    for (i = 0; i < 1000; i++) {
        if ((inb(SERIAL_PORT + 5) & 0x20) != 0)
            break;
    }
    outb(SERIAL_PORT, (uint8_t)c);
}

static void serial_print(const char* s)
{
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

static void output_char(char c, void* ctx)
{
    (void)ctx;
    if (vga_putc) vga_putc(c);
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
}

static void output_string(const char* s, void* ctx)
{
    (void)ctx;
    if (vga_print) vga_print(s);
    serial_print(s);
}

static void reverse_str(char* s, int len)
{
    int i = 0, j = len - 1;
    while (i < j) {
        char tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
        i++;
        j--;
    }
}

static int int_to_str(long long value, char* buf, int base, int uppercase)
{
    int len = 0;
    int neg = 0;
    unsigned long long uval;

    if (base < 2 || base > 36) base = 10;

    if (value < 0 && base == 10) {
        neg = 1;
        uval = (unsigned long long)(-(value + 1)) + 1;
    } else {
        uval = (unsigned long long)value;
    }

    if (uval == 0) {
        buf[len++] = '0';
    } else {
        while (uval > 0) {
            unsigned long long digit = uval % (unsigned long long)base;
            if (digit < 10) {
                buf[len++] = '0' + (char)digit;
            } else {
                if (uppercase) {
                    buf[len++] = 'A' + (char)(digit - 10);
                } else {
                    buf[len++] = 'a' + (char)(digit - 10);
                }
            }
            uval /= (unsigned long long)base;
        }
    }

    if (neg) buf[len++] = '-';
    reverse_str(buf, len);
    buf[len] = '\0';
    return len;
}

static int uint_to_str(unsigned long long value, char* buf, int base, int uppercase)
{
    int len = 0;

    if (base < 2 || base > 36) base = 10;

    if (value == 0) {
        buf[len++] = '0';
    } else {
        while (value > 0) {
            unsigned long long digit = value % (unsigned long long)base;
            if (digit < 10) {
                buf[len++] = '0' + (char)digit;
            } else {
                if (uppercase) {
                    buf[len++] = 'A' + (char)(digit - 10);
                } else {
                    buf[len++] = 'a' + (char)(digit - 10);
                }
            }
            value /= (unsigned long long)base;
        }
    }

    reverse_str(buf, len);
    buf[len] = '\0';
    return len;
}

static int format_num(char* out, size_t out_size, size_t* out_pos,
                      const char* num_str, int num_len,
                      int width, int left_align, int zero_pad)
{
    int pad = width - num_len;
    size_t pos = *out_pos;

    if (pad > 0 && !left_align) {
        while (pad-- > 0 && pos < out_size - 1) {
            out[pos++] = zero_pad ? '0' : ' ';
        }
    }
    for (int i = 0; i < num_len && pos < out_size - 1; i++) {
        out[pos++] = num_str[i];
    }
    if (pad > 0 && left_align) {
        while (pad-- > 0 && pos < out_size - 1) {
            out[pos++] = ' ';
        }
    }
    *out_pos = pos;
    return 0;
}

static int vsnprintf_internal(char* buf, size_t n, const char* fmt, va_list args)
{
    size_t pos = 0;
    char tmp[64];

    if (n == 0) return 0;
    if (!buf) return -1;

    while (*fmt && pos < n - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        fmt++;

        int left_align = 0;
        int zero_pad = 0;
        int width = 0;

        while (*fmt == '-') { left_align = 1; fmt++; }
        while (*fmt == '0') { zero_pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int slen = 0;
                while (s[slen]) slen++;
                int pad = width - slen;
                if (pad > 0 && !left_align) {
                    while (pad-- > 0 && pos < n - 1) buf[pos++] = ' ';
                }
                while (*s && pos < n - 1) buf[pos++] = *s++;
                if (pad > 0 && left_align) {
                    while (pad-- > 0 && pos < n - 1) buf[pos++] = ' ';
                }
                break;
            }
            case 'c': {
                int c = va_arg(args, int);
                int pad = width - 1;
                if (pad > 0 && !left_align) {
                    while (pad-- > 0 && pos < n - 1) buf[pos++] = ' ';
                }
                if (pos < n - 1) buf[pos++] = (char)c;
                if (pad > 0 && left_align) {
                    while (pad-- > 0 && pos < n - 1) buf[pos++] = ' ';
                }
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(args, int);
                int len = int_to_str((long long)v, tmp, 10, 0);
                format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                break;
            }
            case 'l': {
                fmt++;
                if (*fmt == 'l') {
                    fmt++;
                    if (*fmt == 'd' || *fmt == 'i') {
                        long long v = va_arg(args, long long);
                        int len = int_to_str(v, tmp, 10, 0);
                        format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                    } else if (*fmt == 'u') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        int len = uint_to_str(v, tmp, 10, 0);
                        format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                    } else if (*fmt == 'x') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        int len = uint_to_str(v, tmp, 16, 0);
                        format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                    } else if (*fmt == 'X') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        int len = uint_to_str(v, tmp, 16, 1);
                        format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                    }
                } else if (*fmt == 'd' || *fmt == 'i') {
                    long v = va_arg(args, long);
                    int len = int_to_str((long long)v, tmp, 10, 0);
                    format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                } else if (*fmt == 'u') {
                    unsigned long v = va_arg(args, unsigned long);
                    int len = uint_to_str((unsigned long long)v, tmp, 10, 0);
                    format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                } else if (*fmt == 'x') {
                    unsigned long v = va_arg(args, unsigned long);
                    int len = uint_to_str((unsigned long long)v, tmp, 16, 0);
                    format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                } else if (*fmt == 'X') {
                    unsigned long v = va_arg(args, unsigned long);
                    int len = uint_to_str((unsigned long long)v, tmp, 16, 1);
                    format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                }
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                int len = uint_to_str((unsigned long long)v, tmp, 10, 0);
                format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(args, unsigned int);
                int len = uint_to_str((unsigned long long)v, tmp, 16, 0);
                format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                break;
            }
            case 'X': {
                unsigned int v = va_arg(args, unsigned int);
                int len = uint_to_str((unsigned long long)v, tmp, 16, 1);
                format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                break;
            }
            case 'p': {
                void* v = va_arg(args, void*);
                unsigned long long addr = (unsigned long long)(uintptr_t)v;
                tmp[0] = '0'; tmp[1] = 'x';
                int len = uint_to_str(addr, tmp + 2, 16, 0) + 2;
                format_num(buf, n, &pos, tmp, len, width, left_align, zero_pad);
                break;
            }
            case '%': {
                if (pos < n - 1) buf[pos++] = '%';
                break;
            }
            default: {
                if (pos < n - 1) buf[pos++] = '%';
                if (pos < n - 1 && *fmt) buf[pos++] = *fmt;
                break;
            }
        }
        if (*fmt) fmt++;
    }
    buf[pos] = '\0';
    return (int)pos;
}

void* malloc(size_t size)
{
    if (size == 0) size = 1;
    return memory_alloc((uint64_t)size);
}

void* calloc(size_t num, size_t size)
{
    size_t total = num * size;
    if (total == 0) total = 1;
    return memory_zalloc((uint64_t)total);
}

void* realloc(void* ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void* new_ptr = memory_alloc((uint64_t)size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, size);
    memory_free(ptr);
    return new_ptr;
}

void free(void* ptr)
{
    if (ptr) memory_free(ptr);
}

void* _aligned_malloc(size_t size, size_t alignment)
{
    if (alignment == 0) alignment = sizeof(void*);
    return memory_alloc_aligned((uint64_t)size, (uint64_t)alignment);
}

void _aligned_free(void* ptr)
{
    if (ptr) memory_free(ptr);
}

size_t strlen(const char* s)
{
    size_t len = 0;
    if (!s) return 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dst, const char* src)
{
    char* d = dst;
    if (!dst || !src) return dst;
    while ((*dst++ = *src++)) {}
    return d;
}

char* strncpy(char* dst, const char* src, size_t n)
{
    char* d = dst;
    size_t i = 0;
    if (!dst || !src) return dst;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return d;
}

int strcmp(const char* a, const char* b)
{
    if (!a || !b) {
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int strncmp(const char* a, const char* b, size_t n)
{
    if (!a || !b) {
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)a - *(unsigned char*)b;
}

char* strcat(char* dst, const char* src)
{
    char* d = dst;
    if (!dst || !src) return dst;
    while (*dst) dst++;
    while ((*dst++ = *src++)) {}
    return d;
}

char* strncat(char* dst, const char* src, size_t n)
{
    char* d = dst;
    size_t i = 0;
    if (!dst || !src) return dst;
    while (*dst) dst++;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
    return d;
}

char* strchr(const char* s, int c)
{
    if (!s) return NULL;
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    if ((char)c == '\0') return (char*)s;
    return NULL;
}

char* strrchr(const char* s, int c)
{
    const char* last = NULL;
    if (!s) return NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == '\0') return (char*)s;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    if (!needle[0]) return (char*)haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncmp(haystack, needle, nlen) == 0) {
            return (char*)haystack;
        }
    }
    return NULL;
}

size_t strspn(const char* s, const char* accept)
{
    size_t count = 0;
    if (!s || !accept) return 0;
    while (*s) {
        if (strchr(accept, *s)) {
            count++;
            s++;
        } else {
            break;
        }
    }
    return count;
}

size_t strcspn(const char* s, const char* reject)
{
    size_t count = 0;
    if (!s || !reject) return s ? strlen(s) : 0;
    while (*s) {
        if (strchr(reject, *s)) {
            break;
        }
        count++;
        s++;
    }
    return count;
}

char* strtok(char* str, const char* delim)
{
    static char* last = NULL;
    char* token;
    if (str) last = str;
    if (!last || !*last) return NULL;
    while (*last && strchr(delim, *last)) last++;
    if (!*last) return NULL;
    token = last;
    while (*last && !strchr(delim, *last)) last++;
    if (*last) {
        *last = '\0';
        last++;
    }
    return token;
}

int atoi(const char* s)
{
    return (int)atol(s);
}

long atol(const char* s)
{
    if (!s) return 0;
    long sign = 1, result = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return sign * result;
}

long long atoll(const char* s)
{
    if (!s) return 0;
    long long sign = 1, result = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return sign * result;
}

double atof(const char* s)
{
    if (!s) return 0.0;
    int sign = 1;
    double result = 0.0, fraction = 0.0;
    int in_frac = 0;
    double frac_div = 10.0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            if (in_frac) {
                fraction += (double)(*s - '0') / frac_div;
                frac_div *= 10.0;
            } else {
                result = result * 10.0 + (double)(*s - '0');
            }
        } else if (*s == '.' && !in_frac) {
            in_frac = 1;
        } else {
            break;
        }
        s++;
    }
    return (double)sign * (result + fraction);
}

char* itoa(int value, char* str, int base)
{
    if (!str) return NULL;
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }
    int_to_str((long long)value, str, base, 0);
    return str;
}

char* ltoa(long value, char* str, int base)
{
    if (!str) return NULL;
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }
    int_to_str((long long)value, str, base, 0);
    return str;
}

char* lltoa(long long value, char* str, int base)
{
    if (!str) return NULL;
    if (base < 2 || base > 36) {
        str[0] = '\0';
        return str;
    }
    int_to_str(value, str, base, 0);
    return str;
}

void* memset(void* s, int c, size_t n)
{
    uint8_t* p = (uint8_t*)s;
    if (!s) return NULL;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (!dst || !src) return dst;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memmove(void* dst, const void* src, size_t n)
{
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (!dst || !src) return dst;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n)
{
    const uint8_t* p1 = (const uint8_t*)a;
    const uint8_t* p2 = (const uint8_t*)b;
    if (!a || !b) {
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return (int)p1[i] - (int)p2[i];
    }
    return 0;
}

void* memchr(const void* s, int c, size_t n)
{
    const uint8_t* p = (const uint8_t*)s;
    if (!s) return NULL;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (uint8_t)c) return (void*)(p + i);
    }
    return NULL;
}

int vprintf(const char* fmt, va_list argptr)
{
    char buf[1024];
    int len = vsnprintf_internal(buf, sizeof(buf), fmt, argptr);
    output_string(buf, NULL);
    return len;
}

int vsprintf(char* buf, const char* fmt, va_list argptr)
{
    return vsnprintf_internal(buf, (size_t)-1, fmt, argptr);
}

int vsnprintf(char* buf, size_t n, const char* fmt, va_list argptr)
{
    return vsnprintf_internal(buf, n, fmt, argptr);
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    int len = vsnprintf_internal(buf, sizeof(buf), fmt, args);
    va_end(args);
    output_string(buf, NULL);
    return len;
}

int sprintf(char* buf, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf_internal(buf, (size_t)-1, fmt, args);
    va_end(args);
    return len;
}

int snprintf(char* buf, size_t n, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf_internal(buf, n, fmt, args);
    va_end(args);
    return len;
}

int fprintf(FILE* f, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    int len = vsnprintf_internal(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (f == stdout || f == stderr || f == NULL) {
        output_string(buf, NULL);
    } else if (f && f->fd >= 3) {
        uint32_t written = 0;
        WriteFile((HANDLE)(uintptr_t)f->fd, buf, (uint32_t)len, &written, NULL);
    }
    return len;
}

int fflush(FILE* f)
{
    (void)f;
    return 0;
}

int sscanf(const char* s, const char* fmt, ...)
{
    (void)s;
    (void)fmt;
    return 0;
}

int puts(const char* s)
{
    if (s) output_string(s, NULL);
    output_string("\n", NULL);
    return 0;
}

int fputs(const char* s, FILE* f)
{
    if (!s) return EOF;
    if (f == stdout || f == stderr || f == NULL) {
        output_string(s, NULL);
    } else if (f && f->fd >= 3) {
        int len = (int)strlen(s);
        uint32_t written = 0;
        WriteFile((HANDLE)(uintptr_t)f->fd, s, (uint32_t)len, &written, NULL);
    }
    return strlen(s);
}

int fputc(int c, FILE* f)
{
    char ch = (char)c;
    if (f == stdout || f == stderr || f == NULL) {
        output_char(ch, NULL);
    } else if (f && f->fd >= 3) {
        uint32_t written = 0;
        WriteFile((HANDLE)(uintptr_t)f->fd, &ch, 1, &written, NULL);
    }
    return (int)(unsigned char)ch;
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int fgetc(FILE* f)
{
    if (!f) return EOF;
    if (f->ungot > 0) {
        f->ungot--;
        return (int)(unsigned char)f->ungetbuf[f->ungot];
    }
    if (f == stdin) {
        return EOF;
    }
    if (f->fd >= 3) {
        char ch;
        uint32_t read = 0;
        if (ReadFile((HANDLE)(uintptr_t)f->fd, &ch, 1, &read, NULL) && read == 1) {
            return (int)(unsigned char)ch;
        }
        f->eof = 1;
    }
    return EOF;
}

int getchar(void)
{
    return fgetc(stdin);
}

char* fgets(char* s, int n, FILE* f)
{
    if (!s || n <= 0 || !f) return NULL;
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

char* gets_s(char* s, size_t n)
{
    if (!s || n <= 0) return NULL;
    return fgets(s, (int)n, stdin);
}

FILE* fopen(const char* path, const char* mode)
{
    if (!path || !mode) return NULL;
    FILE* f = (FILE*)malloc(sizeof(FILE));
    if (!f) return NULL;
    memset(f, 0, sizeof(FILE));

    uint32_t access = 0;
    uint32_t creation = OPEN_EXISTING;
    int writable = 0;

    if (mode[0] == 'r') {
        access = GENERIC_READ;
        creation = OPEN_EXISTING;
    } else if (mode[0] == 'w') {
        access = GENERIC_WRITE;
        creation = CREATE_ALWAYS;
        writable = 1;
    } else if (mode[0] == 'a') {
        access = GENERIC_WRITE;
        creation = OPEN_ALWAYS;
        writable = 1;
    }
    for (int i = 1; mode[i]; i++) {
        if (mode[i] == '+') {
            access = GENERIC_READ | GENERIC_WRITE;
        }
    }

    HANDLE h = CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE || h == NULL) {
        free(f);
        return NULL;
    }
    f->fd = (int)(uintptr_t)h;
    f->mode = writable ? 1 : 0;
    f->eof = 0;
    f->err = 0;
    f->ungot = 0;
    return f;
}

int fclose(FILE* f)
{
    if (!f) return EOF;
    if (f->fd >= 3) {
        CloseHandle((HANDLE)(uintptr_t)f->fd);
    }
    free(f);
    return 0;
}

size_t fread(void* buf, size_t sz, size_t n, FILE* f)
{
    if (!buf || !f || sz == 0 || n == 0) return 0;
    size_t total = sz * n;
    if (f == stdin) return 0;
    if (f->fd >= 3) {
        uint32_t read = 0;
        if (ReadFile((HANDLE)(uintptr_t)f->fd, buf, (uint32_t)total, &read, NULL)) {
            if (read < total) f->eof = 1;
            return (size_t)read / sz;
        }
        f->err = 1;
    }
    return 0;
}

size_t fwrite(const void* buf, size_t sz, size_t n, FILE* f)
{
    if (!buf || !f || sz == 0 || n == 0) return 0;
    size_t total = sz * n;
    if (f == stdout || f == stderr) {
        const char* p = (const char*)buf;
        for (size_t i = 0; i < total; i++) output_char(p[i], NULL);
        return n;
    }
    if (f->fd >= 3) {
        uint32_t written = 0;
        if (WriteFile((HANDLE)(uintptr_t)f->fd, buf, (uint32_t)total, &written, NULL)) {
            return (size_t)written / sz;
        }
        f->err = 1;
    }
    return 0;
}

int fseek(FILE* f, long offset, int origin)
{
    if (!f || f->fd < 3) return -1;
    uint32_t method = FILE_BEGIN;
    if (origin == SEEK_CUR) method = FILE_CURRENT;
    else if (origin == SEEK_END) method = FILE_END;
    int32_t high = 0;
    if (SetFilePointer((HANDLE)(uintptr_t)f->fd, (int32_t)offset, &high, method)) {
        f->eof = 0;
        return 0;
    }
    return -1;
}

long ftell(FILE* f)
{
    if (!f || f->fd < 3) return -1;
    int32_t high = 0;
    int32_t low = 0;
    if (SetFilePointer((HANDLE)(uintptr_t)f->fd, 0, &high, FILE_CURRENT)) {
        return (long)low;
    }
    return -1;
}

void rewind(FILE* f)
{
    if (!f) return;
    fseek(f, 0, SEEK_SET);
    f->eof = 0;
    f->err = 0;
}

int feof(FILE* f)
{
    if (!f) return 0;
    return f->eof;
}

int ferror(FILE* f)
{
    if (!f) return 0;
    return f->err;
}

int remove(const char* path)
{
    if (!path) return -1;
    if (DeleteFileA(path)) return 0;
    return -1;
}

int rename(const char* oldp, const char* newp)
{
    if (!oldp || !newp) return -1;
    if (MoveFileA(oldp, newp)) return 0;
    return -1;
}

int _mkdir(const char* path)
{
    if (!path) return -1;
    if (CreateDirectoryA(path, NULL)) return 0;
    return -1;
}

int _rmdir(const char* path)
{
    if (!path) return -1;
    if (RemoveDirectoryA(path)) return 0;
    return -1;
}

int _chdir(const char* path)
{
    (void)path;
    return -1;
}

char* _getcwd(char* buf, size_t n)
{
    if (!buf || n == 0) return NULL;
    if (n > 1) {
        buf[0] = '/';
        buf[1] = '\0';
    } else if (n == 1) {
        buf[0] = '\0';
    }
    return buf;
}

int _access(const char* path, int mode)
{
    (void)path;
    (void)mode;
    return -1;
}

long _filelength(int fd)
{
    if (fd < 3) return -1;
    int64_t size = 0;
    if (GetFileSizeEx((HANDLE)(uintptr_t)fd, &size)) {
        return (long)size;
    }
    return -1;
}

static unsigned int msvcrt_rand_seed = 1;

int rand(void)
{
    msvcrt_rand_seed = msvcrt_rand_seed * 1103515245u + 12345u;
    return (int)(msvcrt_rand_seed / 65536u) % (RAND_MAX + 1);
}

void srand(unsigned int seed)
{
    msvcrt_rand_seed = seed;
}

static void qsort_swap(char* a, char* b, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

void qsort(void* base, size_t num, size_t size,
           int (*cmp)(const void*, const void*))
{
    if (!base || num < 2 || size == 0 || !cmp) return;
    char* arr = (char*)base;
    for (size_t i = 1; i < num; i++) {
        size_t j = i;
        while (j > 0 && cmp(arr + (j - 1) * size, arr + j * size) > 0) {
            qsort_swap(arr + (j - 1) * size, arr + j * size, size);
            j--;
        }
    }
}

void* bsearch(const void* key, const void* base, size_t num,
              size_t size, int (*cmp)(const void*, const void*))
{
    if (!key || !base || size == 0 || !cmp) return NULL;
    size_t low = 0, high = num;
    const char* arr = (const char*)base;
    while (low < high) {
        size_t mid = (low + high) / 2;
        int r = cmp(key, arr + mid * size);
        if (r == 0) return (void*)(arr + mid * size);
        else if (r < 0) high = mid;
        else low = mid + 1;
    }
    return NULL;
}

void abort(void)
{
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

#define ATEXIT_MAX_FUNCS 32
static void (*atexit_funcs[ATEXIT_MAX_FUNCS])(void);
static int atexit_count = 0;

void exit(int code)
{
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_funcs[i]) atexit_funcs[i]();
    }
    (void)code;
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void _exit(int code)
{
    (void)code;
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

int atexit(void (*fn)(void))
{
    if (!fn || atexit_count >= ATEXIT_MAX_FUNCS) return -1;
    atexit_funcs[atexit_count++] = fn;
    return 0;
}

char* getenv(const char* name)
{
    static char env_buf[256];
    if (!name) return NULL;
    DWORD r = GetEnvironmentVariableA(name, env_buf, sizeof(env_buf));
    if (r == 0 || r >= sizeof(env_buf)) return NULL;
    return env_buf;
}

int system(const char* cmd)
{
    (void)cmd;
    return -1;
}

int _setmode(int fd, int mode)
{
    (void)fd;
    return mode;
}

size_t wcslen(const unsigned short* s)
{
    size_t len = 0;
    if (!s) return 0;
    while (s[len]) len++;
    return len;
}

unsigned short* wcscpy(unsigned short* d, const unsigned short* s)
{
    unsigned short* r = d;
    if (!d || !s) return d;
    while ((*d++ = *s++)) {}
    return r;
}

int wcscmp(const unsigned short* a, const unsigned short* b)
{
    if (!a || !b) {
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    while (*a && *a == *b) { a++; b++; }
    return (int)*a - (int)*b;
}

static int wc_tolower(unsigned short c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return (int)c;
}

int _wcsicmp(const unsigned short* a, const unsigned short* b)
{
    if (!a || !b) {
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    while (*a && *b && wc_tolower(*a) == wc_tolower(*b)) {
        a++; b++;
    }
    return wc_tolower(*a) - wc_tolower(*b);
}

int MultiByteToWideChar(uint32_t cp, uint32_t flags,
                        const char* mb, int mb_len,
                        unsigned short* wc, int wc_len)
{
    (void)cp;
    (void)flags;
    if (!mb || mb_len == 0 || (!wc && wc_len != 0)) return 0;
    if (mb_len < 0) mb_len = (int)strlen(mb);
    if (!wc) return mb_len + 1;
    int count = 0;
    for (int i = 0; i < mb_len && count < wc_len - 1; i++, count++) {
        wc[count] = (unsigned short)(unsigned char)mb[i];
    }
    if (count < wc_len) wc[count] = 0;
    return count;
}

int WideCharToMultiByte(uint32_t cp, uint32_t flags,
                        const unsigned short* wc, int wc_len,
                        char* mb, int mb_len,
                        const char* def, int* used)
{
    (void)cp;
    (void)flags;
    (void)def;
    if (used) *used = 0;
    if (!wc || wc_len == 0 || (!mb && mb_len != 0)) return 0;
    if (wc_len < 0) wc_len = (int)wcslen(wc);
    if (!mb) return wc_len + 1;
    int count = 0;
    for (int i = 0; i < wc_len && count < mb_len - 1; i++, count++) {
        mb[count] = wc[i] < 0x80 ? (char)wc[i] : '?';
    }
    if (count < mb_len) mb[count] = 0;
    return count;
}

int msvcrt_init(void)
{
    return 0;
}
