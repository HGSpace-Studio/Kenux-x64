#include "font.h"
#include "font_data.h"
#include "cjk_font.h"
#include "emoji_font.h"
#include "font_ttf.h"
#include "framebuffer.h"

void font_draw_char(uint32_t x, uint32_t y, char c, uint32_t color) {
    const uint8_t* glyph = font_8x16[(uint8_t)c];

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if (bits & (1 << (7 - col))) {
                fb_set_pixel(x + col, y + row, color);
            }
        }
    }
}

static bool font_color_is_light(uint32_t color) {
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    return (r * 30 + g * 59 + b * 11) > 15000;
}

static uint32_t font_shadow_color(uint32_t color) {
    if (font_color_is_light(color)) {
        return RGB(0x06, 0x0A, 0x14);
    }
    return RGB(0xF2, 0xF5, 0xFF);
}

static void font_draw_codepoint(uint32_t x, uint32_t y, uint32_t code, uint32_t color) {
    if (code < 0x80) {
        font_draw_char(x, y, (char)code, color);
    } else if (emoji_font_lookup(code) >= 0) {
        emoji_font_draw_char(x, y, code, color);
    } else if (cjk_font_lookup(code) >= 0) {
        cjk_font_draw_char(x, y, code, color);
    } else {
        font_draw_char(x, y, '?', color);
    }
}

/* Decode one UTF-8 codepoint from str.
 * Returns number of bytes consumed, puts codepoint in *out_code.
 * Returns 0 if end of string. */
static uint32_t utf8_decode(const char* str, uint32_t* out_code) {
    uint8_t b0 = (uint8_t)str[0];
    if (b0 == 0) return 0;

    if (b0 < 0x80) {
        *out_code = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0) {
        uint32_t code = (b0 & 0x1F) << 6;
        if (str[1]) code |= ((uint8_t)str[1] & 0x3F);
        *out_code = code;
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0) {
        uint32_t code = (b0 & 0x0F) << 12;
        if (str[1]) code |= ((uint8_t)str[1] & 0x3F) << 6;
        if (str[2]) code |= ((uint8_t)str[2] & 0x3F);
        *out_code = code;
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0) {
        uint32_t code = (b0 & 0x07) << 18;
        if (str[1]) code |= ((uint8_t)str[1] & 0x3F) << 12;
        if (str[2]) code |= ((uint8_t)str[2] & 0x3F) << 6;
        if (str[3]) code |= ((uint8_t)str[3] & 0x3F);
        *out_code = code;
        return 4;
    }
    /* Invalid UTF-8, treat as single byte */
    *out_code = b0;
    return 1;
}

/* Determine glyph width for a codepoint.
 * Returns width in pixels. */
static uint32_t glyph_width_bitmap(uint32_t code) {
    if (code < 0x80) return FONT_WIDTH;
    /* Check emoji/symbol font first (includes CJK punctuation, arrows, etc.) */
    if (emoji_font_lookup(code) >= 0) return EMOJI_FONT_W;
    /* Then check CJK font */
    if (cjk_font_lookup(code) >= 0) return CJK_FONT_W;
    return FONT_WIDTH;
}

static uint32_t glyph_width(uint32_t code) {
    uint32_t ttf_width = font_ttf_codepoint_width(code);
    if (ttf_width > 0) return ttf_width;
    return glyph_width_bitmap(code);
}

void font_draw_text(uint32_t x, uint32_t y, const char* str, uint32_t color) {
    uint32_t cx = x;
    while (*str) {
        uint32_t code;
        uint32_t adv = utf8_decode(str, &code);
        if (adv == 0) break;

        if (font_ttf_draw_codepoint(&cx, y, code, color)) {
            str += adv;
            continue;
        }
        font_draw_codepoint(cx, y, code, color);
        cx += glyph_width_bitmap(code);
        str += adv;
    }
}

void font_draw_text_bg(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg) {
    uint32_t cx = x;
    while (*str) {
        uint32_t code;
        uint32_t adv = utf8_decode(str, &code);
        if (adv == 0) break;

        if (font_ttf_draw_codepoint_bg(&cx, y, code, fg, bg)) {
            str += adv;
            continue;
        }

        if (code < 0x80) {
            const uint8_t* glyph = font_8x16[(uint8_t)code];
            for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
                uint8_t bits = glyph[row];
                for (uint32_t col = 0; col < FONT_WIDTH; col++) {
                    if (bits & (1 << (7 - col))) {
                        fb_set_pixel(cx + col, y + row, fg);
                    } else {
                        fb_set_pixel(cx + col, y + row, bg);
                    }
                }
            }
            cx += FONT_WIDTH;
        } else {
            /* For non-ASCII, fill background then draw glyph on top */
            int32_t eidx = emoji_font_lookup(code);
            int32_t cidx = cjk_font_lookup(code);
            uint32_t gw, gh;
            const uint8_t* glyph;

            if (eidx >= 0) {
                gw = EMOJI_FONT_W;
                gh = EMOJI_FONT_H;
                glyph = emoji_font[eidx];
            } else if (cidx >= 0) {
                gw = CJK_FONT_W;
                gh = CJK_FONT_H;
                glyph = cjk_font[cidx];
            } else {
                /* Unknown: draw '?' with bg */
                const uint8_t* dg = font_8x16['?'];
                for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
                    uint8_t bits = dg[row];
                    for (uint32_t col = 0; col < FONT_WIDTH; col++) {
                        if (bits & (1 << (7 - col))) {
                            fb_set_pixel(cx + col, y + row, fg);
                        } else {
                            fb_set_pixel(cx + col, y + row, bg);
                        }
                    }
                }
                cx += FONT_WIDTH;
                str += adv;
                continue;
            }

            /* Fill background for glyph area */
            for (uint32_t row = 0; row < gh; row++) {
                for (uint32_t col = 0; col < gw; col++) {
                    fb_set_pixel(cx + col, y + row, bg);
                }
            }
            /* Draw glyph foreground */
            for (uint32_t row = 0; row < gh; row++) {
                uint8_t hi = glyph[row * 2];
                uint8_t lo = glyph[row * 2 + 1];
                for (uint32_t col = 0; col < 8; col++) {
                    if (hi & (1 << (7 - col)))
                        fb_set_pixel(cx + col, y + row, fg);
                }
                for (uint32_t col = 0; col < 8; col++) {
                    if (lo & (1 << (7 - col)))
                        fb_set_pixel(cx + 8 + col, y + row, fg);
                }
            }
            cx += gw;
        }
        str += adv;
    }
}

uint32_t font_text_width(const char* str) {
    uint32_t width = 0;
    while (*str) {
        uint32_t code;
        uint32_t adv = utf8_decode(str, &code);
        if (adv == 0) break;

        width += glyph_width(code);
        str += adv;
    }
    return width;
}

uint32_t font_text_fit_bytes(const char* str, uint32_t max_width) {
    uint32_t width = 0;
    uint32_t bytes = 0;

    if (!str) return 0;
    while (str[bytes]) {
        uint32_t code;
        uint32_t adv = utf8_decode(str + bytes, &code);
        if (adv == 0) break;

        uint32_t gw = glyph_width(code);
        if (width + gw > max_width) break;

        width += gw;
        bytes += adv;
    }

    return bytes;
}
