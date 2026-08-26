

#include "kapi_string.h"
#include "kapi.h"

#include <string.h>
#include <stdarg.h>

size_t kapi_strlcpy(char* dst, const char* src, size_t size)
{
    if (!dst || !src) {
        return 0;
    }

    size_t src_len = strlen(src);

    if (size) {
        size_t copy_len = src_len < size - 1 ? src_len : size - 1;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }

    return src_len;
}

size_t kapi_strscpy(char* dst, const char* src, size_t size)
{
    if (!dst || !src || !size) {
        return 0;
    }

    size_t i;
    for (i = 0; i < size - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';

    if (src[i]) {
        return 0;
    }

    return i;
}

int kapi_snprintf(char* str, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = kapi_vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int kapi_vsnprintf(char* str, size_t size, const char* format, va_list args)
{
    if (!str || !format || size == 0) {
        return 0;
    }

    int count = 0;
    while (*format && count < (int)size - 1) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && count < (int)size - 1) {
                    str[count++] = *s++;
                }
            } else if (*format == 'd' || *format == 'i') {
                int n = va_arg(args, int);
                if (n < 0 && count < (int)size - 1) {
                    str[count++] = '-';
                    n = -n;
                }
                char buffer[12];
                int i = 0;
                do {
                    buffer[i++] = '0' + (n % 10);
                    n /= 10;
                } while (n > 0 && i < 11);
                while (i > 0 && count < (int)size - 1) {
                    str[count++] = buffer[--i];
                }
            } else if (*format == 'x' || *format == 'X') {
                unsigned int n = va_arg(args, unsigned int);
                char buffer[16];
                int i = 0;
                do {
                    char digit = n % 16;
                    buffer[i++] = (digit < 10) ? ('0' + digit) : ((*format == 'X') ? ('A' + digit - 10) : ('a' + digit - 10));
                    n /= 16;
                } while (n > 0 && i < 15);
                while (i > 0 && count < (int)size - 1) {
                    str[count++] = buffer[--i];
                }
            } else if (*format == 'c') {
                if (count < (int)size - 1) {
                    str[count++] = (char)va_arg(args, int);
                }
            } else if (*format == '%') {
                if (count < (int)size - 1) {
                    str[count++] = '%';
                }
            }
            format++;
        } else {
            str[count++] = *format++;
        }
    }
    str[count] = '\0';
    return count;
}

static uint32_t kapi_utf8_next(const char* text, uint32_t len, uint32_t* offset,
                               uint32_t* byte_len)
{
    unsigned char ch = (unsigned char)text[*offset];
    if (ch < 0x80U) {
        *byte_len = 1;
        return ch;
    }
    if ((ch & 0xE0U) == 0xC0U && *offset + 1U < len) {
        *byte_len = 2;
        return (((uint32_t)ch & 0x1FU) << 6) |
               ((uint32_t)text[*offset + 1U] & 0x3FU);
    }
    if ((ch & 0xF0U) == 0xE0U && *offset + 2U < len) {
        *byte_len = 3;
        return (((uint32_t)ch & 0x0FU) << 12) |
               (((uint32_t)text[*offset + 1U] & 0x3FU) << 6) |
               ((uint32_t)text[*offset + 2U] & 0x3FU);
    }
    if ((ch & 0xF8U) == 0xF0U && *offset + 3U < len) {
        *byte_len = 4;
        return (((uint32_t)ch & 0x07U) << 18) |
               (((uint32_t)text[*offset + 1U] & 0x3FU) << 12) |
               (((uint32_t)text[*offset + 2U] & 0x3FU) << 6) |
               ((uint32_t)text[*offset + 3U] & 0x3FU);
    }
    *byte_len = 1;
    return KAPI_TEXT_REPLACEMENT_CHAR;
}

int kapi_text_layout_utf8(const char* text, uint32_t byte_len,
                          kapi_text_glyph_t* glyphs, uint32_t capacity,
                          kapi_text_layout_t* out_layout)
{
    if (!text || !out_layout) return KAPI_EINVAL;
    memset(out_layout, 0, sizeof(*out_layout));
    out_layout->text = text;
    out_layout->byte_len = byte_len;
    out_layout->capacity = capacity;
    out_layout->glyphs = glyphs;

    uint32_t off = 0;
    while (off < byte_len) {
        uint32_t blen = 1;
        uint32_t cp = kapi_utf8_next(text, byte_len, &off, &blen);
        uint32_t cells = cp >= 0x1100U ? 2U : 1U;
        if (glyphs && out_layout->count < capacity) {
            kapi_text_glyph_t* g = &glyphs[out_layout->count];
            g->codepoint = cp;
            g->byte_offset = off;
            g->byte_len = blen;
            g->cell_width = cells;
            g->pixel_width = cells * 8U;
        }
        out_layout->count++;
        out_layout->total_cells += cells;
        out_layout->total_px += cells * 8U;
        off += blen;
    }
    return KAPI_OK;
}
