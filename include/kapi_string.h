

#ifndef KAPI_STRING_H
#define KAPI_STRING_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t kapi_strlcpy(char* dst, const char* src, size_t size);

size_t kapi_strscpy(char* dst, const char* src, size_t size);

int kapi_snprintf(char* str, size_t size, const char* format, ...);

int kapi_vsnprintf(char* str, size_t size, const char* format, va_list args);

/* ===== Fused LeonOS text.h (UTF-8 layout / unicode conversion) ===== */
#define KAPI_TEXT_REPLACEMENT_CHAR 0xfffdu

#define KAPI_UNICODE_OP_LAYOUT_UTF8      1u
#define KAPI_UNICODE_OP_UTF8_TO_UTF16LE  2u
#define KAPI_UNICODE_OP_UTF16LE_TO_UTF8  3u
#define KAPI_UNICODE_OP_VALIDATE_UTF8    4u

typedef struct {
    uint32_t codepoint;
    uint32_t byte_offset;
    uint32_t byte_len;
    uint32_t cell_width;
    uint32_t pixel_width;
} kapi_text_glyph_t;

typedef struct {
    const char*       text;
    uint32_t          byte_len;
    uint32_t          capacity;
    uint32_t          count;
    uint32_t          total_cells;
    uint32_t          total_px;
    kapi_text_glyph_t* glyphs;
} kapi_text_layout_t;

int kapi_text_layout_utf8(const char* text, uint32_t byte_len,
                          kapi_text_glyph_t* glyphs, uint32_t capacity,
                          kapi_text_layout_t* out_layout);

/* LeonOS compat aliases */
#define KAPI_Text_LayoutUTF8(t,l,g,c,o) kapi_text_layout_utf8((t),(l),(g),(c),(o))
typedef kapi_text_glyph_t  KAPI_TEXT_GLYPH;
typedef kapi_text_layout_t KAPI_TEXT_LAYOUT;

#ifdef __cplusplus
}
#endif

#endif
