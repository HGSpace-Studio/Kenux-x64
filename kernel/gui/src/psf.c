#include "psf.h"
#include "framebuffer.h"
#include <arch/memory.h>
#include <string.h>

void psf_init(void)
{
}

static int psf1_load(const void* data, uint32_t size, psf_font_t* font)
{
    if (size < sizeof(psf1_header_t)) return -1;

    const psf1_header_t* hdr = (const psf1_header_t*)data;
    if (hdr->magic != PSF1_MAGIC) return -2;
    if (hdr->charsize == 0) return -3;

    font->type = PSF_TYPE_PSF1;
    font->glyph_height = hdr->charsize;
    font->glyph_width = 8;
    font->glyph_bytes = hdr->charsize;
    font->glyph_count = (hdr->mode & PSF1_MODE512) ? 512 : 256;
    font->has_unicode = 0;
    font->unicode_table = NULL;
    font->unicode_count = 0;

    uint32_t glyph_data_size = font->glyph_count * font->glyph_bytes;
    uint32_t needed = sizeof(psf1_header_t) + glyph_data_size;
    if (size < needed) return -4;

    font->glyphs = (uint8_t*)memory_alloc((uint64_t)glyph_data_size);
    if (!font->glyphs) return -5;
    memcpy(font->glyphs, (const uint8_t*)data + sizeof(psf1_header_t), glyph_data_size);

    return 0;
}

static int psf2_load(const void* data, uint32_t size, psf_font_t* font)
{
    if (size < sizeof(psf2_header_t)) return -1;

    const psf2_header_t* hdr = (const psf2_header_t*)data;
    if (hdr->magic != PSF2_MAGIC) return -2;
    if (hdr->charsize == 0 || hdr->length == 0) return -3;

    font->type = PSF_TYPE_PSF2;
    font->glyph_count = hdr->length;
    font->glyph_height = hdr->height;
    font->glyph_width = hdr->width;
    font->glyph_bytes = hdr->charsize;
    font->has_unicode = (hdr->flags & 0x01) ? 1 : 0;
    font->unicode_table = NULL;
    font->unicode_count = 0;

    uint32_t glyph_data_size = font->glyph_count * font->glyph_bytes;
    uint32_t glyph_offset = hdr->headersize;
    if (size < glyph_offset + glyph_data_size) return -4;

    font->glyphs = (uint8_t*)memory_alloc((uint64_t)glyph_data_size);
    if (!font->glyphs) return -5;
    memcpy(font->glyphs, (const uint8_t*)data + glyph_offset, glyph_data_size);

    if (font->has_unicode) {
        uint32_t unicode_offset = glyph_offset + glyph_data_size;
        font->unicode_count = font->glyph_count;
        font->unicode_table = (uint32_t*)memory_alloc(
            (uint64_t)font->unicode_count * 2 * sizeof(uint32_t));
        if (font->unicode_table) {
            const uint8_t* uptr = (const uint8_t*)data + unicode_offset;
            const uint8_t* uend = (const uint8_t*)data + size;
            uint32_t glyph_idx = 0;
            uint32_t table_idx = 0;
            uint32_t max_table = font->unicode_count * 2;

            while (uptr < uend && glyph_idx < font->glyph_count && table_idx < max_table) {
                uint32_t cp = 0;
                if (*uptr == 0xFF) {
                    glyph_idx++;
                    uptr++;
                    continue;
                }
                if ((*uptr & 0x80) == 0) {
                    cp = *uptr++;
                } else if ((*uptr & 0xE0) == 0xC0) {
                    if (uptr + 1 >= uend) break;
                    cp = ((uptr[0] & 0x1F) << 6) | (uptr[1] & 0x3F);
                    uptr += 2;
                } else if ((*uptr & 0xF0) == 0xE0) {
                    if (uptr + 2 >= uend) break;
                    cp = ((uptr[0] & 0x0F) << 12) | ((uptr[1] & 0x3F) << 6) | (uptr[2] & 0x3F);
                    uptr += 3;
                } else if ((*uptr & 0xF8) == 0xF0) {
                    if (uptr + 3 >= uend) break;
                    cp = ((uptr[0] & 0x07) << 18) | ((uptr[1] & 0x3F) << 12) |
                         ((uptr[2] & 0x3F) << 6) | (uptr[3] & 0x3F);
                    uptr += 4;
                } else {
                    uptr++;
                    continue;
                }
                font->unicode_table[table_idx++] = glyph_idx;
                font->unicode_table[table_idx++] = cp;
            }
            font->unicode_count = table_idx / 2;
        }
    }

    return 0;
}

int psf_load(const void* data, uint32_t size, psf_font_t* font)
{
    if (!data || size < 4 || !font) return -1;

    uint16_t magic16 = *(const uint16_t*)data;
    if (magic16 == PSF1_MAGIC) {
        return psf1_load(data, size, font);
    }

    uint32_t magic32 = *(const uint32_t*)data;
    if (magic32 == PSF2_MAGIC) {
        return psf2_load(data, size, font);
    }

    return -2;
}

void psf_free(psf_font_t* font)
{
    if (!font) return;
    if (font->glyphs) {
        memory_free(font->glyphs);
        font->glyphs = NULL;
    }
    if (font->unicode_table) {
        memory_free(font->unicode_table);
        font->unicode_table = NULL;
    }
    font->glyph_count = 0;
}

static int32_t psf_find_glyph_index(psf_font_t* font, uint32_t codepoint)
{
    if (!font) return -1;

    if (codepoint < 0x80 && codepoint < font->glyph_count) {
        return (int32_t)codepoint;
    }

    if (font->has_unicode && font->unicode_table && font->unicode_count > 0) {
        for (uint32_t i = 0; i < font->unicode_count; i++) {
            if (font->unicode_table[i * 2 + 1] == codepoint) {
                return (int32_t)font->unicode_table[i * 2];
            }
        }
    }

    return -2;
}

int psf_get_glyph(psf_font_t* font, uint32_t codepoint, uint8_t* bitmap)
{
    if (!font || !bitmap) return -1;

    int32_t idx = psf_find_glyph_index(font, codepoint);
    if (idx < 0) return -2;

    memcpy(bitmap, font->glyphs + (uint32_t)idx * font->glyph_bytes, font->glyph_bytes);
    return 0;
}

int psf_draw_char(psf_font_t* font, uint32_t codepoint, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg)
{
    if (!font) return -1;

    int32_t idx = psf_find_glyph_index(font, codepoint);
    if (idx < 0) idx = psf_find_glyph_index(font, '?');
    if (idx < 0) idx = 0;

    const uint8_t* glyph = font->glyphs + (uint32_t)idx * font->glyph_bytes;
    uint32_t width = font->glyph_width;
    uint32_t height = font->glyph_height;
    uint32_t bytes_per_row = (width + 7) / 8;

    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            uint32_t byte_idx = row * bytes_per_row + col / 8;
            uint32_t bit_idx = 7 - (col % 8);
            int set = (glyph[byte_idx] >> bit_idx) & 1;
            uint32_t color = set ? fg : bg;
            fb_set_pixel(x + col, y + row, color);
        }
    }

    return 0;
}

int psf_draw_string(psf_font_t* font, const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg)
{
    if (!font || !str) return -1;

    uint32_t cx = x;
    while (*str) {
        uint32_t codepoint;
        uint8_t b = (uint8_t)*str;

        if (b < 0x80) {
            codepoint = b;
            str++;
        } else if ((b & 0xE0) == 0xC0) {
            codepoint = (b & 0x1F) << 6;
            if (str[1]) codepoint |= ((uint8_t)str[1] & 0x3F);
            str += 2;
        } else if ((b & 0xF0) == 0xE0) {
            codepoint = (b & 0x0F) << 12;
            if (str[1]) codepoint |= ((uint8_t)str[1] & 0x3F) << 6;
            if (str[2]) codepoint |= ((uint8_t)str[2] & 0x3F);
            str += 3;
        } else if ((b & 0xF8) == 0xF0) {
            codepoint = (b & 0x07) << 18;
            if (str[1]) codepoint |= ((uint8_t)str[1] & 0x3F) << 12;
            if (str[2]) codepoint |= ((uint8_t)str[2] & 0x3F) << 6;
            if (str[3]) codepoint |= ((uint8_t)str[3] & 0x3F);
            str += 4;
        } else {
            str++;
            continue;
        }

        psf_draw_char(font, codepoint, cx, y, fg, bg);
        cx += font->glyph_width;
    }

    return 0;
}

int psf_measure_string(psf_font_t* font, const char* str, uint32_t* width, uint32_t* height)
{
    if (!font || !str || !width || !height) return -1;

    uint32_t len = 0;
    while (*str) {
        uint8_t b = (uint8_t)*str;
        if (b < 0x80) str++;
        else if ((b & 0xE0) == 0xC0) str += 2;
        else if ((b & 0xF0) == 0xE0) str += 3;
        else if ((b & 0xF8) == 0xF0) str += 4;
        else str++;
        len++;
    }

    *width = len * font->glyph_width;
    *height = font->glyph_height;
    return 0;
}