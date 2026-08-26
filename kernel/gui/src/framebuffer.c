#include "framebuffer.h"
#include "color.h"
#include "font.h"
#include "cjk_font.h"
#include "emoji_font.h"
#include <arch/framebuffer.h>
#include <arch/memory.h>
#include <string.h>

gui_fb_t fb;
static bool fb_rgb_is_bgra = true;
static uint8_t* fb_hw_base = NULL;
static uint32_t fb_hw_pitch = 0;
static uint32_t gui_scale_percent_x = 100;
static uint32_t gui_scale_percent_y = 100;
static uint32_t gui_scale_percent_min = 100;

static uint32_t clamp_scale_percent(uint32_t pct) {
    if (pct < 155) return 155;
    if (pct > 200) return 200;
    return pct;
}

static uint32_t scale_value(uint32_t v, uint32_t pct) {
    uint64_t n = (uint64_t)v * pct;
    uint32_t out = (uint32_t)((n + 50) / 100);
    if (v > 0 && out == 0) out = 1;
    return out;
}

/* 从 KenuxK 内核的 fb_info_t 初始化 GUI framebuffer */
void gui_fb_init(void) {
    fb_info_t* info = fb_get_info();
    if (info == NULL || info->addr == NULL) {
        fb.base = (uint8_t*)0;
        fb.width = 0;
        fb.height = 0;
        fb.size = 0;
        return;
    }

    fb.width = info->width;
    fb.height = info->height;
    fb.pitch = info->bytes_per_line;
    fb_hw_base = info->addr;
    fb_hw_pitch = info->bytes_per_line;
    fb.base = (uint8_t*)memory_alloc(info->total_size);
    if (fb.base == NULL) {
        fb.base = info->addr;
    }
    fb.size = (uint32_t)info->total_size;
    fb.bpp = info->bpp;

    gui_scale_percent_x = fb.width * 100 / 1280;
    gui_scale_percent_y = fb.height * 100 / 720;
    gui_scale_percent_x = clamp_scale_percent(gui_scale_percent_x);
    gui_scale_percent_y = clamp_scale_percent(gui_scale_percent_y);
    gui_scale_percent_min = gui_scale_percent_x < gui_scale_percent_y ? gui_scale_percent_x : gui_scale_percent_y;

    if (info->pixel_format == FB_PIXEL_FORMAT_BGRA) {
        fb_rgb_is_bgra = true;
    } else {
        fb_rgb_is_bgra = false;
    }
}

uint32_t gui_scale_x(uint32_t v) {
    return scale_value(v, gui_scale_percent_x);
}

uint32_t gui_scale_y(uint32_t v) {
    return scale_value(v, gui_scale_percent_y);
}

uint32_t gui_scale_size(uint32_t v) {
    return scale_value(v, gui_scale_percent_min);
}

uint32_t gui_unscale_x(uint32_t v) {
    if (gui_scale_percent_x == 0) return v;
    return (uint32_t)(((uint64_t)v * 100 + gui_scale_percent_x / 2) / gui_scale_percent_x);
}

uint32_t gui_unscale_y(uint32_t v) {
    if (gui_scale_percent_y == 0) return v;
    return (uint32_t)(((uint64_t)v * 100 + gui_scale_percent_y / 2) / gui_scale_percent_y);
}

uint32_t fb_convert_color(uint32_t rgb_color) {
    /* Ensure alpha is opaque (0xFF) if not set */
    uint8_t a = (rgb_color >> 24) & 0xFF;
    if (a == 0) a = 0xFF;

    if (fb_rgb_is_bgra) {
        /* BGRA framebuffer: B,G,R,Reserved in memory
         * uint32_t in little-endian: Reserved<<24 | R<<16 | G<<8 | B
         * RGB macro already stores as R<<16 | G<<8 | B, so just set alpha */
        return (rgb_color & 0x00FFFFFF) | ((uint32_t)a << 24);
    }
    /* RGB framebuffer: R,G,B,Reserved in memory
     * uint32_t in little-endian: Reserved<<24 | B<<16 | G<<8 | R
     * Need to swap R and B channels */
    uint8_t r = (rgb_color >> 16) & 0xFF;
    uint8_t g = (rgb_color >> 8) & 0xFF;
    uint8_t b = rgb_color & 0xFF;
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

static inline void set_pixel_direct(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb.width || y >= fb.height) return;
    volatile uint32_t* ptr = (volatile uint32_t*)(fb.base + y * fb.pitch + x * 4);
    *ptr = fb_convert_color(color);
}

void gui_fb_set_pixel(uint32_t x, uint32_t y, uint32_t color) {
    set_pixel_direct(x, y, color);
}

uint32_t gui_fb_get_pixel(uint32_t x, uint32_t y) {
    if (x >= fb.width || y >= fb.height) return 0;
    volatile uint32_t* ptr = (volatile uint32_t*)(fb.base + y * fb.pitch + x * 4);
    return *ptr;
}

void gui_fb_clear(uint32_t color) {
    gui_fb_fill_rect(0, 0, fb.width, fb.height, color);
}

void gui_fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (fb.base == NULL) return;
    if (x >= fb.width || y >= fb.height) return;
    if ((uint64_t)x + (uint64_t)w > fb.width) w = fb.width - x;
    if ((uint64_t)y + (uint64_t)h > fb.height) h = fb.height - y;

    uint32_t conv_color = fb_convert_color(color);
    for (uint32_t j = 0; j < h; j++) {
        volatile uint32_t* ptr = (volatile uint32_t*)(fb.base + (y + j) * fb.pitch + x * 4);
        for (uint32_t i = 0; i < w; i++) {
            ptr[i] = conv_color;
        }
    }
}

void gui_fb_flush(void) {
    if (fb.base == NULL || fb_hw_base == NULL || fb.base == fb_hw_base) return;
    uint32_t rows = fb.height;
    uint32_t bytes = fb.width * 4;
    if (bytes > fb.pitch) {
        bytes = fb.pitch;
    }
    if (bytes > fb_hw_pitch) {
        bytes = fb_hw_pitch;
    }
    /* The common case is a tightly packed framebuffer. Copy it in one block
     * to reduce loop overhead during full-screen repaint; keep the row path
     * for firmware framebuffers with padding at the end of each scanline. */
    if (fb.pitch == bytes && fb_hw_pitch == bytes) {
        memcpy(fb_hw_base, fb.base, (size_t)bytes * rows);
        return;
    }
    for (uint32_t y = 0; y < rows; y++) {
        memcpy(fb_hw_base + y * fb_hw_pitch, fb.base + y * fb.pitch, bytes);
    }
}

void gui_fb_flush_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (fb.base == NULL || fb_hw_base == NULL || fb.base == fb_hw_base) return;
    if (x >= fb.width || y >= fb.height || w == 0 || h == 0) return;
    if ((uint64_t)x + (uint64_t)w > fb.width) w = fb.width - x;
    if ((uint64_t)y + (uint64_t)h > fb.height) h = fb.height - y;

    uint32_t bytes = w * 4;
    uint32_t src_limit = fb.pitch > x * 4 ? fb.pitch - x * 4 : 0;
    uint32_t dst_limit = fb_hw_pitch > x * 4 ? fb_hw_pitch - x * 4 : 0;
    if (bytes > src_limit) bytes = src_limit;
    if (bytes > dst_limit) bytes = dst_limit;
    if (bytes == 0) return;

    for (uint32_t row = 0; row < h; row++) {
        uint32_t yy = y + row;
        memcpy(fb_hw_base + yy * fb_hw_pitch + x * 4,
               fb.base + yy * fb.pitch + x * 4,
               bytes);
    }
}

void fb_test_pattern(void) {
    if (fb.base == NULL) return;

    /* Clear screen with dark background */
    gui_fb_fill_rect(0, 0, fb.width, fb.height, RGB(0x10, 0x10, 0x10));

    uint32_t y = 0;
    uint32_t w = fb.width;

    /* === Title section === */
    gui_fb_fill_rect(0, 0, w, 40, RGB(0x00, 0x78, 0xD4));
    font_draw_text(10, 12, "Kenux Color & Font Preview", RGB(0xFF, 0xFF, 0xFF));

    /* Direct CJK draw test - bypass UTF-8 decoding to verify font data */
    cjk_font_draw_char(300, 12, 0x4E00, RGB(0xFF, 0xFF, 0xFF));  /* 一 */
    cjk_font_draw_char(320, 12, 0x4E2D, RGB(0xFF, 0xFF, 0xFF));  /* 中 */
    cjk_font_draw_char(340, 12, 0x6587, RGB(0xFF, 0xFF, 0xFF));  /* 文 */
    cjk_font_draw_char(360, 12, 0x5B57, RGB(0xFF, 0xFF, 0xFF));  /* 字 */
    cjk_font_draw_char(380, 12, 0x4F53, RGB(0xFF, 0xFF, 0xFF));  /* 体 */

    y = 48;

    /* === Section 1: English Font Preview === */
    font_draw_text(10, y, "English Font: BRLNSR TTF anti-aliased", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    font_draw_text(10, y, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", RGB(0xFF, 0xFF, 0xFF));
    y += 18;
    font_draw_text(10, y, "abcdefghijklmnopqrstuvwxyz", RGB(0xE0, 0xE0, 0xE0));
    y += 18;
    font_draw_text(10, y, "0123456789 !@#$%^&*()_+-=", RGB(0xA0, 0xFF, 0xA0));
    y += 28;

    /* === Section 2: Chinese Font Preview === */
    font_draw_text(10, y, "\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97\xe4\xbd\x93\xef\xbc\x9a\xe9\xbb\x91\xe4\xbd\x93 16x16", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    /* "Kenux 操作系统 颜色预览" */
    font_draw_text(10, y, "Kenux \xe6\x93\x8d\xe4\xbd\x9c\xe7\xb3\xbb\xe7\xbb\x9f \xe9\xa2\x9c\xe8\x89\xb2\xe9\xa2\x84\xe8\xa7\x88", RGB(0xFF, 0xFF, 0xFF));
    y += 22;
    /* "中文字体显示测试 欢迎使用" */
    font_draw_text(10, y, "\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97\xe4\xbd\x93\xe6\x98\xbe\xe7\xa4\xba\xe6\xb5\x8b\xe8\xaf\x95 \xe6\xac\xa2\xe8\xbf\x8e\xe4\xbd\xbf\xe7\x94\xa8", RGB(0xFF, 0xD6, 0x0A));
    y += 22;
    /* "颜色名称：红橙黄绿青蓝紫" */
    font_draw_text(10, y, "\xe9\xa2\x9c\xe8\x89\xb2\xef\xbc\x9a\xe7\xba\xa2\xe6\xa9\x99\xe9\xbb\x84\xe7\xbb\xbf\xe9\x9d\x92\xe8\x93\x9d\xe7\xb4\xab", RGB(0xFF, 0xA5, 0x00));
    y += 28;

    /* === Section 3: Emoji & Symbols === */
    font_draw_text(10, y, "Symbols & Emoji:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    {
        /* Display a grid of emoji symbols with different colors */
        /* Row 1: Weather & Nature */
        font_draw_text(10, y,
            "\xe2\x98\x80 "  /* sun */
            "\xe2\x98\x81 "  /* cloud */
            "\xe2\x98\x82 "  /* umbrella */
            "\xe2\x98\x83 "  /* snowman */
            "\xe2\x9d\x84 "  /* snowflake */
            "\xe2\x98\x85 "  /* black star */
            "\xe2\x98\x86 "  /* white star */
            "\xe2\x9c\xa8 "  /* sparkles */
            "\xe2\x9d\xa4 "  /* heart */
            "\xe2\x99\xa0 "  /* spade */
            "\xe2\x99\xa5 "  /* heart suit */
            "\xe2\x99\xa6 "  /* diamond */
            "\xe2\x99\xa3 "  /* club */
            "\xe2\x99\xaa "  /* eighth note */
            "\xe2\x99\xab "  /* beamed notes */
            "\xe2\x9a\xa0 ", /* warning */
            RGB(0xFF, 0xD7, 0x00));
        y += 20;
        /* Row 2: Faces & Gestures */
        font_draw_text(10, y,
            "\xe2\x98\xba "  /* smile */
            "\xe2\x98\xbb "  /* black smile */
            "\xe2\x98\xbc "  /* sun face */
            "\xe2\x9c\x8c "  /* victory */
            "\xe2\x9c\x88 "  /* airplane */
            "\xe2\x9c\x89 "  /* envelope */
            "\xe2\x9c\x8f "  /* pencil */
            "\xe2\x98\x8e "  /* telephone */
            "\xe2\x98\x95 "  /* coffee */
            "\xe2\x9c\x85 "  /* white check */
            "\xe2\x9c\x94 "  /* heavy check */
            "\xe2\x9d\x8c "  /* cross mark */
            "\xe2\x9d\x93 "  /* question */
            "\xe2\x9d\x95 "  /* exclamation */
            "\xe2\x98\x91 "  /* ballot check */
            "\xe2\x98\x92 ", /* ballot x */
            RGB(0x00, 0xFF, 0x88));
        y += 20;
        /* Row 3: Arrows */
        font_draw_text(10, y,
            "\xe2\x86\x90 "  /* left */
            "\xe2\x86\x91 "  /* up */
            "\xe2\x86\x92 "  /* right */
            "\xe2\x86\x93 "  /* down */
            "\xe2\x86\x94 "  /* left-right */
            "\xe2\x86\x95 "  /* up-down */
            "\xe2\x86\x96 "  /* nw */
            "\xe2\x86\x97 "  /* ne */
            "\xe2\x86\x98 "  /* se */
            "\xe2\x86\x99 "  /* sw */
            "\xe2\x9e\xa1 "  /* black right */
            "\xe2\x86\xa9 "  /* undo */
            "\xe2\x86\xaa "  /* redo */
            "\xe2\x9e\x95 "  /* heavy plus */
            "\xe2\x9e\x96 "  /* heavy minus */
            "\xe2\x9e\x97 ", /* heavy division */
            RGB(0x00, 0xD4, 0xFF));
        y += 20;
        /* Row 4: Geometric shapes */
        font_draw_text(10, y,
            "\xe2\x97\x8f "  /* black circle */
            "\xe2\x97\x8b "  /* white circle */
            "\xe2\x97\x8e "  /* fisheye */
            "\xe2\x96\xa0 "  /* black square */
            "\xe2\x96\xa1 "  /* white square */
            "\xe2\x96\xb2 "  /* black up tri */
            "\xe2\x96\xb3 "  /* white up tri */
            "\xe2\x96\xbc "  /* black down tri */
            "\xe2\x96\xbd "  /* white down tri */
            "\xe2\x97\x86 "  /* black diamond */
            "\xe2\x97\x87 "  /* white diamond */
            "\xe2\x97\x88 "  /* diamond dot */
            "\xe2\x97\xa1 "  /* circled dot */
            "\xe2\x97\xa2 "  /* circled ring */
            "\xe2\x97\xa3 "  /* circled star */
            "\xe2\x88\x9e ", /* infinity */
            RGB(0xFF, 0x6B, 0x6B));
        y += 20;
        /* Row 5: Math & special */
        font_draw_text(10, y,
            "\xc2\xb0 "   /* degree */
            "\xc2\xb1 "   /* plus-minus */
            "\xc3\x97 "   /* not in font, skip - actually U+00D7 is multiplication */
            "\xe2\x88\x9a "  /* sqrt */
            "\xe2\x88\x91 "  /* sum */
            "\xe2\x88\x8f "  /* product */
            "\xe2\x88\xab "  /* integral */
            "\xe2\x89\x88 "  /* almost equal */
            "\xe2\x89\xa0 "  /* not equal */
            "\xe2\x89\xa4 "  /* less-equal */
            "\xe2\x89\xa5 "  /* greater-equal */
            "\xe2\x89\xa1 "  /* identical */
            "\xe2\x88\x88 "  /* element of */
            "\xe2\x88\xa9 "  /* intersection */
            "\xe2\x88\xaa "  /* union */
            "\xe2\x82\xac ",  /* euro */
            RGB(0xBB, 0x88, 0xFF));
        y += 28;
    }

    /* === Section 4: Rainbow Colors === */
    font_draw_text(10, y, "Rainbow Colors:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    {
        uint32_t colors[] = {COL_RED, COL_ORANGE, COL_YELLOW, COL_GREEN, COL_CYAN, COL_BLUE, COL_PURPLE, COL_MAGENTA};
        const char* names[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Magenta"};
        uint32_t count = 8;
        uint32_t sw = w / count;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t bx = i * sw;
            gui_fb_fill_rect(bx, y, sw, 30, colors[i]);
            font_draw_text_bg(bx + 4, y + 7, names[i], RGB(0xFF,0xFF,0xFF), colors[i]);
        }
        y += 38;
    }

    /* === Section 4: Grayscale === */
    font_draw_text(10, y, "Grayscale:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    {
        uint32_t grays[] = {COL_GRAY10, COL_GRAY20, COL_GRAY30, COL_GRAY40, COL_GRAY50,
                           COL_GRAY60, COL_GRAY70, COL_GRAY80, COL_GRAY90, COL_WHITE};
        uint32_t count = 10;
        uint32_t sw = w / count;
        for (uint32_t i = 0; i < count; i++) {
            gui_fb_fill_rect(i * sw, y, sw, 25, grays[i]);
        }
        y += 33;
    }

    /* === Section 5: Material Design Colors === */
    font_draw_text(10, y, "Material Design:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    {
        uint32_t mats[] = {COL_MAT_RED, COL_MAT_PINK, COL_MAT_PURPLE, COL_MAT_INDIGO,
                           COL_MAT_BLUE, COL_MAT_CYAN, COL_MAT_TEAL, COL_MAT_GREEN,
                           COL_MAT_LIME, COL_MAT_AMBER, COL_MAT_ORANGE};
        uint32_t count = 11;
        uint32_t sw = w / count;
        for (uint32_t i = 0; i < count; i++) {
            gui_fb_fill_rect(i * sw, y, sw, 25, mats[i]);
        }
        y += 33;
    }

    /* === Section 6: Apple System Colors === */
    font_draw_text(10, y, "Apple System Colors:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    {
        uint32_t apples[] = {COL_APPLE_RED, COL_APPLE_ORANGE, COL_APPLE_YELLOW, COL_APPLE_GREEN,
                             COL_APPLE_MINT, COL_APPLE_TEAL, COL_APPLE_CYAN, COL_APPLE_BLUE,
                             COL_APPLE_INDIGO, COL_APPLE_PURPLE, COL_APPLE_PINK};
        uint32_t count = 11;
        uint32_t sw = w / count;
        for (uint32_t i = 0; i < count; i++) {
            gui_fb_fill_rect(i * sw, y, sw, 25, apples[i]);
        }
        y += 33;
    }

    /* === Section 7: Accent Colors === */
    font_draw_text(10, y, "Accent Colors:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    {
        uint32_t accents[] = {ACCENT_BLUE, ACCENT_PURPLE, ACCENT_PINK, ACCENT_RED,
                              ACCENT_ORANGE, ACCENT_YELLOW, ACCENT_GREEN, ACCENT_MINT,
                              ACCENT_TEAL, ACCENT_CYAN, ACCENT_INDIGO, ACCENT_ROSE};
        uint32_t count = 12;
        uint32_t sw = w / count;
        for (uint32_t i = 0; i < count; i++) {
            gui_fb_fill_rect(i * sw, y, sw, 25, accents[i]);
        }
        y += 33;
    }

    /* === Section 8: Gradient Bar === */
    font_draw_text(10, y, "Gradients:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    gfx_draw_gradient(0, y, w / 2, 20, COL_RED, COL_BLUE, GRADIENT_HORIZONTAL);
    gfx_draw_gradient(w / 2, y, w / 2, 20, COL_BLUE, COL_GREEN, GRADIENT_HORIZONTAL);
    y += 28;

    /* === Section 9: Chinese color names === */
    font_draw_text(10, y, "\xe9\xa2\x9c\xe8\x89\xb2\xe5\x90\x8d\xe7\xa7\xb0\xe6\xb5\x8b\xe8\xaf\x95:", RGB(0x00, 0xD4, 0xFF));
    y += 22;
    /* "红色 橙色 黄色 绿色 青色 蓝色 紫色" */
    {
        uint32_t cn_colors[] = {COL_RED, COL_ORANGE, COL_YELLOW, COL_GREEN, COL_CYAN, COL_BLUE, COL_PURPLE};
        const char* cn_names[] = {
            "\xe7\xba\xa2\xe8\x89\xb2",  /* 红色 */
            "\xe6\xa9\x99\xe8\x89\xb2",  /* 橙色 */
            "\xe9\xbb\x84\xe8\x89\xb2",  /* 黄色 */
            "\xe7\xbb\xbf\xe8\x89\xb2",  /* 绿色 */
            "\xe9\x9d\x92\xe8\x89\xb2",  /* 青色 */
            "\xe8\x93\x9d\xe8\x89\xb2",  /* 蓝色 */
            "\xe7\xb4\xab\xe8\x89\xb2"   /* 紫色 */
        };
        uint32_t count = 7;
        uint32_t sw = w / count;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t bx = i * sw;
            gui_fb_fill_rect(bx, y, sw, 30, cn_colors[i]);
            font_draw_text_bg(bx + 4, y + 7, cn_names[i], RGB(0xFF,0xFF,0xFF), cn_colors[i]);
        }
        y += 38;
    }

    /* === Footer === */
    font_draw_text(10, fb.height - 20, "400+ Colors | CJK+EN+Emoji Fonts | UTF-8 | HSL Gradients", RGB(0x80, 0x80, 0x80));
}

/* Alpha-blend src onto dst: result = src*a/255 + dst*(255-a)/255 */
static inline uint32_t alpha_blend(uint32_t src_rgb, uint32_t dst_rgb, uint8_t a) {
    if (a == 0) return dst_rgb;
    if (a == 255) return src_rgb;
    uint32_t sr = (src_rgb >> 16) & 0xFF;
    uint32_t sg = (src_rgb >> 8) & 0xFF;
    uint32_t sb = src_rgb & 0xFF;
    uint32_t dr = (dst_rgb >> 16) & 0xFF;
    uint32_t dg = (dst_rgb >> 8) & 0xFF;
    uint32_t db = dst_rgb & 0xFF;
    uint32_t r = (sr * a + dr * (255 - a)) / 255;
    uint32_t g = (sg * a + dg * (255 - a)) / 255;
    uint32_t b = (sb * a + db * (255 - a)) / 255;
    return RGB(r, g, b);
}

void fb_blit_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t* data) {
    if (fb.base == NULL || data == NULL) return;
    for (uint32_t j = 0; j < h; j++) {
        uint32_t dy = y + j;
        if (dy >= fb.height) break;
        for (uint32_t i = 0; i < w; i++) {
            uint32_t dx = x + i;
            if (dx >= fb.width) break;
            uint32_t pixel = data[j * w + i];
            uint8_t a = (pixel >> 24) & 0xFF;
            if (a == 0) continue;
            uint32_t src_rgb = pixel & 0x00FFFFFF;
            if (a == 255) {
                set_pixel_direct(dx, dy, src_rgb);
            } else {
                uint32_t dst_raw = gui_fb_get_pixel(dx, dy);
                /* get_pixel returns raw fb format; convert back to RGB */
                uint32_t dst_rgb;
                if (fb_rgb_is_bgra) {
                    dst_rgb = dst_raw;
                } else {
                    uint8_t r = dst_raw & 0xFF;
                    uint8_t g = (dst_raw >> 8) & 0xFF;
                    uint8_t b = (dst_raw >> 16) & 0xFF;
                    dst_rgb = RGB(r, g, b);
                }
                uint32_t blended = alpha_blend(src_rgb, dst_rgb, a);
                set_pixel_direct(dx, dy, blended);
            }
        }
    }
}

/* ---- Rounded rectangle and shadow helpers ---- */

void fb_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t r, uint32_t color) {
    if (fb.base == NULL || w == 0 || h == 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    /* Fill the central cross */
    fb_fill_rect(x + r, y, w - 2 * r, h, color);
    fb_fill_rect(x, y + r, r, h - 2 * r, color);
    fb_fill_rect(x + w - r, y + r, r, h - 2 * r, color);

    /* Four rounded corners */
    int32_t rr = (int32_t)r;
    int32_t r2 = rr * rr;
    for (int32_t dy = 0; dy < rr; dy++) {
        for (int32_t dx = 0; dx < rr; dx++) {
            int32_t cx = rr - 1 - dx;
            int32_t cy = rr - 1 - dy;
            if (cx * cx + cy * cy <= r2) {
                set_pixel_direct(x + dx, y + dy, color);               /* TL */
                set_pixel_direct(x + w - 1 - dx, y + dy, color);       /* TR */
                set_pixel_direct(x + dx, y + h - 1 - dy, color);       /* BL */
                set_pixel_direct(x + w - 1 - dx, y + h - 1 - dy, color); /* BR */
            }
        }
    }
}

void fb_fill_rounded_top_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                              uint32_t r, uint32_t color) {
    if (fb.base == NULL || w == 0 || h == 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h) r = h;

    /* Fill everything except the top-left and top-right corners */
    fb_fill_rect(x + r, y, w - 2 * r, h, color);
    fb_fill_rect(x, y + r, r, h - r, color);
    fb_fill_rect(x + w - r, y + r, r, h - r, color);

    /* Rounded top corners */
    int32_t rr = (int32_t)r;
    int32_t r2 = rr * rr;
    for (int32_t dy = 0; dy < rr; dy++) {
        for (int32_t dx = 0; dx < rr; dx++) {
            int32_t cx = rr - 1 - dx;
            int32_t cy = rr - 1 - dy;
            if (cx * cx + cy * cy <= r2) {
                set_pixel_direct(x + dx, y + dy, color);
                set_pixel_direct(x + w - 1 - dx, y + dy, color);
            }
        }
    }
}

void fb_draw_rounded_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                  uint32_t r, uint32_t color) {
    if (fb.base == NULL || w == 0 || h == 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    for (uint32_t i = r; i < w - r; i++) {
        set_pixel_direct(x + i, y, color);
        set_pixel_direct(x + i, y + h - 1, color);
    }
    for (uint32_t j = r; j < h - r; j++) {
        set_pixel_direct(x, y + j, color);
        set_pixel_direct(x + w - 1, y + j, color);
    }

    int32_t rr = (int32_t)r;
    int32_t r2 = rr * rr;
    for (int32_t dy = 0; dy < rr; dy++) {
        for (int32_t dx = 0; dx < rr; dx++) {
            int32_t cx = rr - 1 - dx;
            int32_t cy = rr - 1 - dy;
            int32_t dist2 = cx * cx + cy * cy;
            if (dist2 <= r2 && dist2 > (r2 - 2 * rr + 1)) {
                set_pixel_direct(x + dx, y + dy, color);
                set_pixel_direct(x + w - 1 - dx, y + dy, color);
                set_pixel_direct(x + dx, y + h - 1 - dy, color);
                set_pixel_direct(x + w - 1 - dx, y + h - 1 - dy, color);
            }
        }
    }
}

void fb_blend_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   uint32_t color, uint8_t a) {
    if (fb.base == NULL) return;
    if (x >= fb.width || y >= fb.height) return;
    if (x + w > fb.width) w = fb.width - x;
    if (y + h > fb.height) h = fb.height - y;

    for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
            uint32_t dst_raw = gui_fb_get_pixel(x + i, y + j);
            uint32_t dst_rgb;
            if (fb_rgb_is_bgra) {
                dst_rgb = dst_raw;
            } else {
                uint8_t r = dst_raw & 0xFF;
                uint8_t g = (dst_raw >> 8) & 0xFF;
                uint8_t b = (dst_raw >> 16) & 0xFF;
                dst_rgb = RGB(r, g, b);
            }
            uint32_t blended = alpha_blend(color, dst_rgb, a);
            set_pixel_direct(x + i, y + j, blended);
        }
    }
}

void fb_blend_pixel(uint32_t x, uint32_t y, uint32_t color, uint8_t a) {
    if (fb.base == NULL || x >= fb.width || y >= fb.height || a == 0) return;
    if (a == 255) {
        set_pixel_direct(x, y, color);
        return;
    }

    uint32_t dst_raw = gui_fb_get_pixel(x, y);
    uint32_t dst_rgb;
    if (fb_rgb_is_bgra) {
        dst_rgb = dst_raw;
    } else {
        uint8_t r = dst_raw & 0xFF;
        uint8_t g = (dst_raw >> 8) & 0xFF;
        uint8_t b = (dst_raw >> 16) & 0xFF;
        dst_rgb = RGB(r, g, b);
    }

    set_pixel_direct(x, y, alpha_blend(color, dst_rgb, a));
}

void fb_draw_window_shadow(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           uint32_t radius, uint32_t blur, uint8_t alpha) {
    if (fb.base == NULL || blur == 0) return;
    (void)radius; /* rounded shadow would need per-pixel work; keep simple */

    uint32_t shadow_color = RGB(0, 0, 0);

    /* Right side shadow */
    for (uint32_t i = 0; i < blur; i++) {
        uint8_t a = (uint8_t)((uint32_t)alpha * (blur - i) / blur);
        fb_blend_rect(x + w + i, y, 1, h + blur, shadow_color, a);
    }
    /* Bottom side shadow */
    for (uint32_t i = 0; i < blur; i++) {
        uint8_t a = (uint8_t)((uint32_t)alpha * (blur - i) / blur);
        fb_blend_rect(x, y + h + i, w + blur, 1, shadow_color, a);
    }
    /* Bottom-right corner shadow */
    for (uint32_t i = 0; i < blur; i++) {
        uint8_t a = (uint8_t)((uint32_t)alpha * (blur - i) / blur * (blur - i) / blur);
        fb_blend_rect(x + w + i, y + h + i, 1, 1, shadow_color, a);
    }
}

void fb_blit_scaled(uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh,
                     const uint32_t* src, uint32_t sw, uint32_t sh) {
    if (fb.base == NULL || src == NULL || dw == 0 || dh == 0 || sw == 0 || sh == 0) return;
    for (uint32_t j = 0; j < dh; j++) {
        uint32_t fy = (dh > 1 && sh > 1) ? (uint32_t)(((uint64_t)j * (sh - 1) << 16) / (dh - 1)) : 0;
        uint32_t sy0 = fy >> 16;
        uint32_t sy1 = sy0 + 1 < sh ? sy0 + 1 : sy0;
        uint32_t wy = fy & 0xFFFF;
        uint32_t py = dy + j;
        if (py >= fb.height) break;
        for (uint32_t i = 0; i < dw; i++) {
            uint32_t fx = (dw > 1 && sw > 1) ? (uint32_t)(((uint64_t)i * (sw - 1) << 16) / (dw - 1)) : 0;
            uint32_t sx0 = fx >> 16;
            uint32_t sx1 = sx0 + 1 < sw ? sx0 + 1 : sx0;
            uint32_t wx = fx & 0xFFFF;
            uint32_t px = dx + i;
            if (px >= fb.width) break;

            uint32_t p00 = src[sy0 * sw + sx0];
            uint32_t p10 = src[sy0 * sw + sx1];
            uint32_t p01 = src[sy1 * sw + sx0];
            uint32_t p11 = src[sy1 * sw + sx1];
            uint32_t inv_x = 65536u - wx;
            uint32_t inv_y = 65536u - wy;
            uint32_t channels[4];
            for (uint32_t c = 0; c < 4; c++) {
                uint32_t shift = c * 8;
                uint64_t top = (((p00 >> shift) & 0xFFu) * inv_x + ((p10 >> shift) & 0xFFu) * wx) >> 16;
                uint64_t bottom = (((p01 >> shift) & 0xFFu) * inv_x + ((p11 >> shift) & 0xFFu) * wx) >> 16;
                channels[c] = (uint32_t)((top * inv_y + bottom * wy) >> 16);
            }
            uint8_t a = (uint8_t)channels[3];
            if (a == 0) continue;
            uint32_t src_rgb = RGB(channels[2], channels[1], channels[0]);
            if (a == 255) {
                set_pixel_direct(px, py, src_rgb);
            } else {
                uint32_t dst_raw = gui_fb_get_pixel(px, py);
                uint32_t dst_rgb;
                if (fb_rgb_is_bgra) {
                    dst_rgb = dst_raw;
                } else {
                    uint8_t r = dst_raw & 0xFF;
                    uint8_t g = (dst_raw >> 8) & 0xFF;
                    uint8_t b = (dst_raw >> 16) & 0xFF;
                    dst_rgb = RGB(r, g, b);
                }
                set_pixel_direct(px, py, alpha_blend(src_rgb, dst_rgb, a));
            }
        }
    }
}