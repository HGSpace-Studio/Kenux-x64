#include "font_ttf.h"
#include "framebuffer.h"
#include "color.h"
#include <arch/memory.h>
#include <string.h>

extern double sqrt(double x);
extern double pow(double x, double y);
extern double fmod(double x, double y);
extern double cos(double x);
extern double acos(double x);
extern double fabs(double x);

static int ttf_ifloor(float x) {
    int i = (int)x;
    return (float)i > x ? i - 1 : i;
}

static int ttf_iceil(float x) {
    int i = (int)x;
    return (float)i < x ? i + 1 : i;
}

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) ((void)(u), memory_alloc((uint64_t)(x)))
#define STBTT_free(x, u)   ((void)(u), memory_free((void*)(x)))
#define STBTT_assert(x)    ((void)0)
#define STBTT_ifloor(x)    ttf_ifloor((float)(x))
#define STBTT_iceil(x)     ttf_iceil((float)(x))
#define STBTT_sqrt(x)      sqrt((double)(x))
#define STBTT_pow(x, y)    pow((double)(x), (double)(y))
#define STBTT_fmod(x, y)   fmod((double)(x), (double)(y))
#define STBTT_cos(x)       cos((double)(x))
#define STBTT_acos(x)      acos((double)(x))
#define STBTT_fabs(x)      fabs((double)(x))
#define STBTT_strlen(x)    strlen((const char*)(x))
#define STBTT_memcpy       memcpy
#define STBTT_memset       memset
#include "stb_truetype.h"

extern const uint8_t kenux_font_latin_ttf[];
extern const uint8_t kenux_font_latin_ttf_end[];
extern const uint8_t kenux_font_cjk_ttf[];
extern const uint8_t kenux_font_cjk_ttf_end[];

#define TTF_GLYPH_BITMAP_W 96
#define TTF_GLYPH_BITMAP_H 96

typedef struct {
    stbtt_fontinfo info;
    const uint8_t* data;
    uint32_t size;
    bool ready;
} kenux_ttf_face_t;

static kenux_ttf_face_t s_latin;
static kenux_ttf_face_t s_cjk;
static bool s_init_done = false;
static bool s_available = false;
static uint8_t s_glyph_bitmap[TTF_GLYPH_BITMAP_W * TTF_GLYPH_BITMAP_H];

static bool ttf_init_face(kenux_ttf_face_t* face, const uint8_t* begin, const uint8_t* end) {
    if (!face || !begin || !end || end <= begin) return false;

    int offset = stbtt_GetFontOffsetForIndex(begin, 0);
    if (offset < 0) return false;

    if (!stbtt_InitFont(&face->info, begin, offset)) return false;

    face->data = begin;
    face->size = (uint32_t)(end - begin);
    face->ready = true;
    return true;
}

bool font_ttf_init(void) {
    if (s_init_done) return s_available;
    s_init_done = true;

    bool latin_ok = ttf_init_face(&s_latin, kenux_font_latin_ttf, kenux_font_latin_ttf_end);
    bool cjk_ok = ttf_init_face(&s_cjk, kenux_font_cjk_ttf, kenux_font_cjk_ttf_end);
    s_available = latin_ok || cjk_ok;
    return s_available;
}

bool font_ttf_available(void) {
    return font_ttf_init();
}

static kenux_ttf_face_t* ttf_face_for_codepoint(uint32_t code) {
    if (!font_ttf_init()) return NULL;

    kenux_ttf_face_t* first = code < 0x80 ? &s_latin : &s_cjk;
    kenux_ttf_face_t* second = code < 0x80 ? &s_cjk : &s_latin;

    if (first->ready && stbtt_FindGlyphIndex(&first->info, (int)code) != 0) return first;
    if (second->ready && stbtt_FindGlyphIndex(&second->info, (int)code) != 0) return second;
    return NULL;
}

static uint32_t ttf_advance_for_codepoint(kenux_ttf_face_t* face, uint32_t code) {
    if (!face || !face->ready) return 0;

    int advance = 0;
    int lsb = 0;
    float scale = stbtt_ScaleForPixelHeight(&face->info, (float)KENUX_TTF_PIXEL_HEIGHT);
    stbtt_GetCodepointHMetrics(&face->info, (int)code, &advance, &lsb);

    int px = ttf_ifloor((float)advance * scale + 0.5f);
    if (px <= 0) px = KENUX_TTF_PIXEL_HEIGHT / 2;
    return (uint32_t)px;
}

uint32_t font_ttf_codepoint_width(uint32_t code) {
    kenux_ttf_face_t* face = ttf_face_for_codepoint(code);
    if (!face) return 0;
    return ttf_advance_for_codepoint(face, code);
}

static bool ttf_draw_codepoint_internal(uint32_t* x, uint32_t y, uint32_t code,
                                        uint32_t color, bool opaque_bg,
                                        uint32_t bg) {
    if (!x) return false;

    kenux_ttf_face_t* face = ttf_face_for_codepoint(code);
    if (!face) return false;

    float scale = stbtt_ScaleForPixelHeight(&face->info, (float)KENUX_TTF_PIXEL_HEIGHT);
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &line_gap);

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&face->info, (int)code, &advance, &lsb);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBoxSubpixel(&face->info, (int)code, scale, scale,
                                        0.0f, 0.0f, &x0, &y0, &x1, &y1);

    int bw = x1 - x0;
    int bh = y1 - y0;
    uint32_t adv_px = ttf_advance_for_codepoint(face, code);

    if (opaque_bg) {
        fb_fill_rect(*x, y, adv_px ? adv_px : (uint32_t)(KENUX_TTF_PIXEL_HEIGHT / 2),
                     KENUX_TTF_PIXEL_HEIGHT, bg);
    }

    if (bw <= 0 || bh <= 0) {
        *x += adv_px;
        return true;
    }

    if (bw > TTF_GLYPH_BITMAP_W || bh > TTF_GLYPH_BITMAP_H) {
        return false;
    }

    memset(s_glyph_bitmap, 0, sizeof(s_glyph_bitmap));
    stbtt_MakeCodepointBitmapSubpixel(&face->info, s_glyph_bitmap,
                                      bw, bh, TTF_GLYPH_BITMAP_W,
                                      scale, scale, 0.0f, 0.0f, (int)code);

    int baseline = (int)y + ttf_ifloor((float)ascent * scale + 0.5f);
    int draw_x = (int)(*x) + x0;
    int draw_y = baseline + y0;

    for (int row = 0; row < bh; row++) {
        int py = draw_y + row;
        if (py < 0 || (uint32_t)py >= fb.height) continue;

        for (int col = 0; col < bw; col++) {
            uint8_t a = s_glyph_bitmap[row * TTF_GLYPH_BITMAP_W + col];
            if (a == 0) continue;

            int px = draw_x + col;
            if (px < 0 || (uint32_t)px >= fb.width) continue;
            fb_blend_pixel((uint32_t)px, (uint32_t)py, color, a);
        }
    }

    *x += adv_px;
    return true;
}

bool font_ttf_draw_codepoint(uint32_t* x, uint32_t y, uint32_t code, uint32_t color) {
    return ttf_draw_codepoint_internal(x, y, code, color, false, 0);
}

bool font_ttf_draw_codepoint_bg(uint32_t* x, uint32_t y, uint32_t code,
                                uint32_t fg, uint32_t bg) {
    return ttf_draw_codepoint_internal(x, y, code, fg, true, bg);
}
