#ifndef ARCH_X86_64_PSF_H
#define ARCH_X86_64_PSF_H

#include <arch/types.h>

#define PSF1_MAGIC     0x0436
#define PSF1_MODE512  0x01
#define PSF1_MODEHAK  0x02

#define PSF2_MAGIC     0x864AB572

typedef struct {
    uint16_t magic;
    uint8_t  mode;
    uint8_t  charsize;
} __attribute__((packed)) psf1_header_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t length;
    uint32_t charsize;
    uint32_t height;
    uint32_t width;
} __attribute__((packed)) psf2_header_t;

typedef struct {
    int       type;
    uint32_t  glyph_count;
    uint32_t  glyph_width;
    uint32_t  glyph_height;
    uint32_t  glyph_bytes;
    uint8_t*  glyphs;
    uint32_t* unicode_table;
    uint32_t  unicode_count;
    int       has_unicode;
} psf_font_t;

#define PSF_TYPE_PSF1   1
#define PSF_TYPE_PSF2   2

void psf_init(void);
int  psf_load(const void* data, uint32_t size, psf_font_t* font);
void psf_free(psf_font_t* font);
int  psf_get_glyph(psf_font_t* font, uint32_t codepoint, uint8_t* bitmap);
int  psf_draw_char(psf_font_t* font, uint32_t codepoint, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
int  psf_draw_string(psf_font_t* font, const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
int  psf_measure_string(psf_font_t* font, const char* str, uint32_t* width, uint32_t* height);

#endif