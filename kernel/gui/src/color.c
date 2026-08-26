/*
 * color.c - Kenux GUI Color System Implementation
 * All operations use integer math (no FPU required)
 * Factor scale: 0-256 where 256 = 100%
 */

#include "color.h"
#include "framebuffer.h"

/* ============================================================
 * Basic color operations
 * ============================================================ */

uint32_t color_lighten(uint32_t color, uint32_t factor) {
    /* factor: 0-256, where 256 = no change, 128 = lighten 50% */
    if (factor >= 256) return color;
    uint32_t inv = 256 - factor;
    uint32_t r = (COLOR_R(color) * factor + 255 * inv) / 256;
    uint32_t g = (COLOR_G(color) * factor + 255 * inv) / 256;
    uint32_t b = (COLOR_B(color) * factor + 255 * inv) / 256;
    return RGB(r, g, b);
}

uint32_t color_darken(uint32_t color, uint32_t factor) {
    /* factor: 0-256, where 256 = no change, 128 = darken 50% */
    if (factor >= 256) return color;
    uint32_t r = (COLOR_R(color) * factor) / 256;
    uint32_t g = (COLOR_G(color) * factor) / 256;
    uint32_t b = (COLOR_B(color) * factor) / 256;
    return RGB(r, g, b);
}

uint32_t color_mix(uint32_t color1, uint32_t color2, uint32_t ratio) {
    /* ratio: 0-256, 0 = color1, 256 = color2 */
    if (ratio >= 256) return color2;
    uint32_t inv = 256 - ratio;
    uint32_t r = (COLOR_R(color1) * inv + COLOR_R(color2) * ratio) / 256;
    uint32_t g = (COLOR_G(color1) * inv + COLOR_G(color2) * ratio) / 256;
    uint32_t b = (COLOR_B(color1) * inv + COLOR_B(color2) * ratio) / 256;
    return RGB(r, g, b);
}

uint32_t color_alpha_blend(uint32_t src, uint32_t dst, uint8_t alpha) {
    if (alpha == 0) return dst;
    if (alpha == 255) return src;
    uint32_t inv = 255 - alpha;
    uint32_t r = (COLOR_R(src) * alpha + COLOR_R(dst) * inv) / 255;
    uint32_t g = (COLOR_G(src) * alpha + COLOR_G(dst) * inv) / 255;
    uint32_t b = (COLOR_B(src) * alpha + COLOR_B(dst) * inv) / 255;
    return RGB(r, g, b);
}

uint32_t color_with_alpha(uint32_t color, uint8_t alpha) {
    return (color & 0x00FFFFFF) | ((uint32_t)alpha << 24);
}

uint32_t color_invert(uint32_t color) {
    return RGB(255 - COLOR_R(color),
               255 - COLOR_G(color),
               255 - COLOR_B(color));
}

uint32_t color_to_grayscale(uint32_t color) {
    /* Use luminance formula: 0.299R + 0.587G + 0.114B */
    uint32_t gray = (COLOR_R(color) * 77 + COLOR_G(color) * 150 + COLOR_B(color) * 29) / 256;
    return RGB(gray, gray, gray);
}

uint8_t color_luminance(uint32_t color) {
    return (uint8_t)((COLOR_R(color) * 77 + COLOR_G(color) * 150 + COLOR_B(color) * 29) / 256);
}

uint32_t color_text_for_bg(uint32_t bg_color) {
    /* Use luminance threshold to pick black or white text */
    if (color_luminance(bg_color) > 128) {
        return RGB(0, 0, 0);  /* Dark text on light background */
    }
    return RGB(255, 255, 255);  /* Light text on dark background */
}

uint32_t color_tint(uint32_t color, uint32_t amount) {
    /* Tint toward white: amount 0-256 */
    if (amount == 0) return color;
    if (amount >= 256) return COL_WHITE;
    uint32_t inv = 256 - amount;
    uint32_t r = (COLOR_R(color) * inv + 255 * amount) / 256;
    uint32_t g = (COLOR_G(color) * inv + 255 * amount) / 256;
    uint32_t b = (COLOR_B(color) * inv + 255 * amount) / 256;
    return RGB(r, g, b);
}

uint32_t color_shade(uint32_t color, uint32_t amount) {
    /* Shade toward black: amount 0-256 */
    if (amount == 0) return color;
    if (amount >= 256) return COL_BLACK;
    uint32_t inv = 256 - amount;
    uint32_t r = (COLOR_R(color) * inv) / 256;
    uint32_t g = (COLOR_G(color) * inv) / 256;
    uint32_t b = (COLOR_B(color) * inv) / 256;
    return RGB(r, g, b);
}

uint32_t color_lerp(uint32_t color1, uint32_t color2, uint32_t t) {
    /* t: 0-256, 0 = color1, 256 = color2 */
    return color_mix(color1, color2, t);
}

/* ============================================================
 * HSL color space conversion (integer math)
 * ============================================================ */

hsl_color_t rgb_to_hsl(uint32_t rgb) {
    hsl_color_t hsl;
    uint32_t r = COLOR_R(rgb);
    uint32_t g = COLOR_G(rgb);
    uint32_t b = COLOR_B(rgb);

    uint32_t max_val = r > g ? (r > b ? r : b) : (g > b ? g : b);
    uint32_t min_val = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint32_t delta = max_val - min_val;

    /* Lightness: 0-100 */
    hsl.l = (uint8_t)((max_val + min_val) * 100 / 510);

    /* Saturation */
    if (delta == 0) {
        hsl.h = 0;
        hsl.s = 0;
        return hsl;
    }

    if (max_val + min_val > 255) {
        /* Light side */
        hsl.s = (uint8_t)(delta * 100 / (510 - max_val - min_val));
    } else {
        /* Dark side */
        if (max_val + min_val == 0) {
            hsl.s = 0;
        } else {
            hsl.s = (uint8_t)(delta * 100 / (max_val + min_val));
        }
    }

    /* Hue: 0-359 */
    if (max_val == r) {
        /* H = 60 * ((G-B)/delta mod 6) */
        int32_t hue;
        if (g >= b) {
            hue = (int32_t)((g - b) * 60 / (int32_t)delta);
        } else {
            hue = (int32_t)((g - b) * 60 / (int32_t)delta) + 360;
        }
        hsl.h = (uint16_t)hue;
    } else if (max_val == g) {
        /* H = 60 * ((B-R)/delta + 2) */
        hsl.h = (uint16_t)(((b - r) * 60 / (int32_t)delta) + 120);
    } else {
        /* H = 60 * ((R-G)/delta + 4) */
        hsl.h = (uint16_t)(((r - g) * 60 / (int32_t)delta) + 240);
    }

    return hsl;
}

/* Helper for HSL to RGB conversion */
static uint32_t hue_to_rgb(uint32_t p, uint32_t q, uint32_t t) {
    /* t: 0-256 (0-1 scaled) */
    if (t < 256) {
        /* t / 256 is in [0,1) */
        uint32_t val = p + (q - p) * 6 * t / 1536;  /* 6*256 = 1536 */
        return val;
    }
    if (t < 2 * 256) {
        uint32_t nt = t - 256;
        return q;
    }
    if (t < 3 * 256) {
        uint32_t nt = t - 2 * 256;
        return p + (q - p) * (2 * 256 - nt) * 6 / 1536;
    }
    if (t < 4 * 256) {
        return p;
    }
    if (t < 5 * 256) {
        uint32_t nt = t - 4 * 256;
        return p + (q - p) * nt * 6 / 1536;
    }
    /* t in [5, 6) */
    uint32_t nt = t - 5 * 256;
    return p + (q - p) * (2 * 256 - nt) * 6 / 1536;
}

uint32_t hsl_to_rgb(uint16_t h, uint8_t s, uint8_t l) {
    /* h: 0-359, s: 0-100, l: 0-100 */
    if (s == 0) {
        /* Grayscale */
        uint32_t gray = l * 255 / 100;
        return RGB(gray, gray, gray);
    }

    /* Convert to 0-256 scale for calculations */
    uint32_t q;
    if (l < 50) {
        q = (uint32_t)l * (100 + s) * 256 / 5000;  /* l*(1+s) scaled to 0-256 */
    } else {
        q = (uint32_t)(l + s - l * s / 100) * 256 / 100;  /* l+s-l*s scaled to 0-256 */
    }
    uint32_t p = (uint32_t)2 * l * 256 / 100 - q;  /* 2*l - q, scaled to 0-256 */

    /* Hue scaled to 0-1536 (6 * 256) */
    uint32_t hk = (uint32_t)h * 256 / 60;  /* 0-1536 */
    if (hk >= 1536) hk = hk - 1536;

    uint32_t r = hue_to_rgb(p, q, hk + 256) > 255 ? 255 : hue_to_rgb(p, q, hk + 256);
    uint32_t g = hue_to_rgb(p, q, hk) > 255 ? 255 : hue_to_rgb(p, q, hk);
    uint32_t b = hue_to_rgb(p, q, hk >= 1024 ? hk - 1024 : hk + 512) > 255 ? 255 :
                 hue_to_rgb(p, q, hk >= 1024 ? hk - 1024 : hk + 512);

    return RGB(r, g, b);
}

/* ============================================================
 * Gradient support
 * ============================================================ */

uint32_t gradient_sample(const uint32_t* stops, uint32_t count, uint32_t t) {
    /* t: 0-256, sample from array of color stops */
    if (count == 0) return COL_BLACK;
    if (count == 1) return stops[0];
    if (t == 0) return stops[0];
    if (t >= 256) return stops[count - 1];

    /* Find which segment we're in */
    uint32_t segment_size = 256 / (count - 1);
    uint32_t seg = t / segment_size;
    if (seg >= count - 1) seg = count - 2;
    uint32_t local_t = (t - seg * segment_size) * 256 / segment_size;
    return color_lerp(stops[seg], stops[seg + 1], local_t);
}

void gfx_draw_gradient(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t color1, uint32_t color2,
                       gradient_dir_t direction) {
    if (fb.base == NULL || w == 0 || h == 0) return;

    if (direction == GRADIENT_HORIZONTAL) {
        for (uint32_t i = 0; i < w; i++) {
            uint32_t t = i * 256 / w;
            uint32_t c = color_lerp(color1, color2, t);
            /* Draw vertical line */
            uint32_t dy = y;
            uint32_t dh = h;
            if (dy >= fb.height) continue;
            if (dy + dh > fb.height) dh = fb.height - dy;
            uint32_t conv = fb_convert_color(c);
            for (uint32_t j = 0; j < dh; j++) {
                volatile uint32_t* ptr = (volatile uint32_t*)(fb.base + (dy + j) * fb.pitch + (x + i) * 4);
                *ptr = conv;
            }
        }
    } else if (direction == GRADIENT_VERTICAL) {
        for (uint32_t j = 0; j < h; j++) {
            uint32_t t = j * 256 / h;
            uint32_t c = color_lerp(color1, color2, t);
            /* Fill horizontal line */
            uint32_t dx = x;
            uint32_t dw = w;
            if (dx >= fb.width) continue;
            if (dx + dw > fb.width) dw = fb.width - dx;
            uint32_t conv = fb_convert_color(c);
            volatile uint32_t* ptr = (volatile uint32_t*)(fb.base + (y + j) * fb.pitch + dx * 4);
            for (uint32_t i = 0; i < dw; i++) {
                ptr[i] = conv;
            }
        }
    } else { /* GRADIENT_DIAGONAL */
        uint32_t total = w + h;
        if (total == 0) return;
        for (uint32_t j = 0; j < h; j++) {
            for (uint32_t i = 0; i < w; i++) {
                uint32_t t = (i + j) * 256 / total;
                uint32_t c = color_lerp(color1, color2, t);
                uint32_t px = x + i;
                uint32_t py = y + j;
                if (px >= fb.width || py >= fb.height) continue;
                volatile uint32_t* ptr = (volatile uint32_t*)(fb.base + py * fb.pitch + px * 4);
                *ptr = fb_convert_color(c);
            }
        }
    }
}
