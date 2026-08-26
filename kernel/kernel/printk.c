

#include <arch/types.h>
#include <arch/io.h>
#include <arch/vga.h>
#include <string.h>
#include <stdarg.h>
#include <spinlock.h>

#define LOG_BUF_SHIFT       16
#define LOG_BUF_SIZE        (1 << LOG_BUF_SHIFT)

#define KERN_EMERG          "<0>"
#define KERN_ALERT          "<1>"
#define KERN_CRIT           "<2>"
#define KERN_ERR            "<3>"
#define KERN_WARNING        "<4>"
#define KERN_NOTICE         "<5>"
#define KERN_INFO           "<6>"
#define KERN_DEBUG          "<7>"

static int console_loglevel = 7;
static int default_loglevel = 4;

static char log_buf[LOG_BUF_SIZE];
static uint64_t log_head = 0;
static uint64_t log_tail = 0;
static spinlock_t logbuf_lock = SPINLOCK_INIT;

static void console_write(const char* str, uint64_t len)
{
    for (uint64_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            vga_putchar('\r');
        }
        vga_putchar(str[i]);
    }
}

static int __extract_level(const char** fmt)
{
    const char* p = *fmt;
    if (p[0] == '<' && p[2] == '>') {
        int level = p[1] - '0';
        if (level >= 0 && level <= 7) {
            *fmt += 3;
            return level;
        }
    }
    return default_loglevel;
}

static void __log_store(const char* text, uint64_t len)
{
    spin_lock(&logbuf_lock);
    for (uint64_t i = 0; i < len; i++) {
        log_buf[log_head % LOG_BUF_SIZE] = text[i];
        log_head++;
        if (log_head - log_tail > LOG_BUF_SIZE) {
            log_tail = log_head - LOG_BUF_SIZE;
        }
    }
    spin_unlock(&logbuf_lock);
}

static int __vsnprintf(char* buf, uint64_t size, const char* fmt, va_list args)
{
    uint64_t i = 0;

    while (*fmt && i < size - 1) {
        if (*fmt != '%') {
            buf[i++] = *fmt++;
            continue;
        }
        fmt++;
        switch (*fmt) {
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    buf[i++] = '-';
                    val = -val;
                }
                char num[16];
                int n = 0;
                do {
                    num[n++] = '0' + (val % 10);
                    val /= 10;
                } while (val > 0);
                while (n-- > 0 && i < size - 1) buf[i++] = num[n];
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                char num[16];
                int n = 0;
                do {
                    num[n++] = '0' + (val % 10);
                    val /= 10;
                } while (val > 0);
                while (n-- > 0 && i < size - 1) buf[i++] = num[n];
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                buf[i++] = '0';
                if (i < size - 1) buf[i++] = 'x';
                for (int shift = 28; shift >= 0 && i < size - 1; shift -= 4) {
                    int nibble = (val >> shift) & 0xF;
                    buf[i++] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
                }
                break;
            }
            case 'l': {
                fmt++;
                if (*fmt == 'u') {
                    uint64_t val = va_arg(args, uint64_t);
                    char num[24];
                    int n = 0;
                    do {
                        num[n++] = '0' + (val % 10);
                        val /= 10;
                    } while (val > 0);
                    while (n-- > 0 && i < size - 1) buf[i++] = num[n];
                } else if (*fmt == 'x') {
                    uint64_t val = va_arg(args, uint64_t);
                    buf[i++] = '0';
                    if (i < size - 1) buf[i++] = 'x';
                    for (int shift = 60; shift >= 0 && i < size - 1; shift -= 4) {
                        int nibble = (int)((val >> shift) & 0xF);
                        buf[i++] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
                    }
                } else if (*fmt == 'd') {
                    int64_t val = va_arg(args, int64_t);
                    if (val < 0) {
                        buf[i++] = '-';
                        val = -val;
                    }
                    char num[24];
                    int n = 0;
                    do {
                        num[n++] = '0' + (val % 10);
                        val /= 10;
                    } while (val > 0);
                    while (n-- > 0 && i < size - 1) buf[i++] = num[n];
                } else {
                    buf[i++] = '%';
                    if (i < size - 1) buf[i++] = 'l';
                    if (i < size - 1) buf[i++] = *fmt;
                }
                break;
            }
            case 'p': {
                void* ptr = va_arg(args, void*);
                uint64_t val = (uint64_t)ptr;
                buf[i++] = '0';
                if (i < size - 1) buf[i++] = 'x';
                for (int shift = 60; shift >= 0 && i < size - 1; shift -= 4) {
                    int nibble = (int)((val >> shift) & 0xF);
                    buf[i++] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
                }
                break;
            }
            case 's': {
                const char* str = va_arg(args, const char*);
                if (!str) str = "(null)";
                while (*str && i < size - 1) buf[i++] = *str++;
                break;
            }
            case 'c': {
                int c = va_arg(args, int);
                buf[i++] = (char)c;
                break;
            }
            case '%':
                buf[i++] = '%';
                break;
            default:
                buf[i++] = '%';
                if (i < size - 1) buf[i++] = *fmt;
                break;
        }
        fmt++;
    }
    buf[i] = '\0';
    return (int)i;
}

void printk(const char* fmt, ...)
{
    const char* p = fmt;
    int level = __extract_level(&p);

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), p, args);
    va_end(args);

    __log_store(buf, len);

    if (level <= console_loglevel) {
        console_write(buf, len);
    }
}

void printk_emerg(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __log_store(buf, len);
    console_write(buf, len);
}

void printk_alert(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_ALERT; __log_store(prefix, 3);
    __log_store(buf, len);
    if (1 <= console_loglevel) console_write(buf, len);
}

void printk_crit(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_CRIT; __log_store(prefix, 3);
    __log_store(buf, len);
    if (2 <= console_loglevel) console_write(buf, len);
}

void printk_err(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_ERR; __log_store(prefix, 3);
    __log_store(buf, len);
    if (3 <= console_loglevel) console_write(buf, len);
}

void printk_warn(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_WARNING; __log_store(prefix, 3);
    __log_store(buf, len);
    if (4 <= console_loglevel) console_write(buf, len);
}

void printk_notice(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_NOTICE; __log_store(prefix, 3);
    __log_store(buf, len);
    if (5 <= console_loglevel) console_write(buf, len);
}

void printk_info(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_INFO; __log_store(prefix, 3);
    __log_store(buf, len);
    if (6 <= console_loglevel) console_write(buf, len);
}

void printk_debug(const char* fmt, ...)
{
    char buf[1024]; va_list args;
    va_start(args, fmt);
    int len = __vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char prefix[] = KERN_DEBUG; __log_store(prefix, 3);
    __log_store(buf, len);
    if (7 <= console_loglevel) console_write(buf, len);
}

uint64_t dmesg_read(char* buf, uint64_t size)
{
    spin_lock(&logbuf_lock);
    uint64_t len = log_head - log_tail;
    if (len > LOG_BUF_SIZE) len = LOG_BUF_SIZE;
    if (len > size) len = size;

    for (uint64_t i = 0; i < len; i++) {
        buf[i] = log_buf[(log_tail + i) % LOG_BUF_SIZE];
    }
    spin_unlock(&logbuf_lock);
    return len;
}

void dmesg_clear(void)
{
    spin_lock(&logbuf_lock);
    log_head = log_tail = 0;
    memset(log_buf, 0, LOG_BUF_SIZE);
    spin_unlock(&logbuf_lock);
}

void printk_set_loglevel(int level)
{
    if (level >= 0 && level <= 7) {
        console_loglevel = level;
    }
}

int printk_get_loglevel(void)
{
    return console_loglevel;
}
