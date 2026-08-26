#include "kenux_ui.h"
#include "framebuffer.h"
#include "font.h"
#include "color.h"

typedef struct {
    uint32_t accent;
    uint32_t inactive_title;
    uint32_t desktop;
    uint32_t border;
    uint32_t selection;
} kenux_ui_palette_t;

static uint32_t s_theme = KENUX_UI_THEME_METRO;
static uint32_t s_metro_scheme = KENUX_UI_SCHEME_BLUE;
static uint32_t s_classic_scheme = KENUX_UI_SCHEME_BLUE;

static const kenux_ui_palette_t s_metro_palettes[KENUX_UI_SCHEME_COUNT] = {
    { RGB(0x25, 0x8D, 0xFF), RGB(0x86, 0x98, 0xBC), RGB(0x10, 0x2A, 0x56), RGB(0x78, 0x94, 0xC8), RGB(0xD8, 0xEA, 0xFF) },
    { RGB(0x15, 0xD2, 0xC1), RGB(0x7D, 0x9F, 0xA0), RGB(0x0D, 0x44, 0x4E), RGB(0x74, 0xA9, 0xA8), RGB(0xD9, 0xFA, 0xF6) },
    { RGB(0x2B, 0xD6, 0x75), RGB(0x7F, 0xA2, 0x87), RGB(0x11, 0x43, 0x2C), RGB(0x73, 0xA8, 0x84), RGB(0xE0, 0xF8, 0xEA) },
    { RGB(0x9A, 0x6C, 0xFF), RGB(0x94, 0x86, 0xB8), RGB(0x2F, 0x23, 0x5B), RGB(0x99, 0x7E, 0xCE), RGB(0xEE, 0xE7, 0xFF) },
    { RGB(0xFF, 0x5F, 0x7A), RGB(0xB1, 0x83, 0x8E), RGB(0x55, 0x19, 0x2B), RGB(0xD2, 0x88, 0x98), RGB(0xFF, 0xE7, 0xEC) },
    { RGB(0xA5, 0xB4, 0xFC), RGB(0x93, 0x9A, 0xAD), RGB(0x1D, 0x24, 0x36), RGB(0x86, 0x90, 0xAA), RGB(0xEA, 0xEE, 0xF8) },
};

static const kenux_ui_palette_t s_classic_palettes[KENUX_UI_SCHEME_COUNT] = {
    { RGB(0x00, 0x00, 0x80), RGB(0x80, 0x80, 0x80), RGB(0x00, 0x80, 0x80), RGB(0x00, 0x00, 0x00), RGB(0xD8, 0xD8, 0xFF) },
    { RGB(0x00, 0x80, 0x80), RGB(0x80, 0x80, 0x80), RGB(0x00, 0x60, 0x60), RGB(0x00, 0x00, 0x00), RGB(0xD8, 0xF0, 0xF0) },
    { RGB(0x00, 0x80, 0x00), RGB(0x80, 0x80, 0x80), RGB(0x00, 0x60, 0x20), RGB(0x00, 0x00, 0x00), RGB(0xD8, 0xEF, 0xD8) },
    { RGB(0x80, 0x00, 0x80), RGB(0x80, 0x80, 0x80), RGB(0x60, 0x20, 0x60), RGB(0x00, 0x00, 0x00), RGB(0xEF, 0xD8, 0xEF) },
    { RGB(0x80, 0x00, 0x00), RGB(0x80, 0x80, 0x80), RGB(0x60, 0x20, 0x20), RGB(0x00, 0x00, 0x00), RGB(0xEF, 0xD8, 0xD8) },
    { RGB(0x40, 0x40, 0x40), RGB(0x80, 0x80, 0x80), RGB(0x60, 0x60, 0x60), RGB(0x00, 0x00, 0x00), RGB(0xD8, 0xD8, 0xD8) },
};

static const kenux_ui_palette_t s_md3_palettes[KENUX_UI_SCHEME_COUNT] = {
    { RGB(0xCF, 0xBC, 0xFF), RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99), RGB(0xE9, 0xDD, 0xFF) },
    { RGB(0x82, 0xD8, 0xD0), RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99), RGB(0xD9, 0xFA, 0xF6) },
    { RGB(0x6D, 0xDB, 0x6D), RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99), RGB(0xE0, 0xF8, 0xEA) },
    { RGB(0xCF, 0xBC, 0xFF), RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99), RGB(0xEE, 0xE7, 0xFF) },
    { RGB(0xFF, 0xB4, 0xAB), RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99), RGB(0xFF, 0xE7, 0xEC) },
    { RGB(0xA5, 0xB4, 0xFC), RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99), RGB(0xEA, 0xEE, 0xF8) },
};

static uint32_t normalize_scheme(uint32_t scheme) {
    return scheme < KENUX_UI_SCHEME_COUNT ? scheme : KENUX_UI_SCHEME_BLUE;
}

static bool is_metro(void) {
    return s_theme == KENUX_UI_THEME_METRO || s_theme == KENUX_UI_THEME_MD3;
}

static uint32_t utf8_safe_prefix_len(const char* text, uint32_t len) {
    if (!text) return 0;
    while (len > 0 && (((uint8_t)text[len]) & 0xC0u) == 0x80u) {
        len--;
    }
    return len;
}

static void draw_ascii_char_surface(kenux_ui_surface_t* surface, uint32_t x,
                                    uint32_t y, uint8_t ch, uint32_t fg,
                                    uint32_t bg, bool opaque_bg,
                                    uint32_t max_w) {
    const uint8_t* glyph = font_8x16[ch];
    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            if (col >= max_w) return;
            if (bits & (1u << (7u - col))) {
                kenux_ui_pixel(surface, x + col, y + row, fg);
            } else if (opaque_bg) {
                kenux_ui_pixel(surface, x + col, y + row, bg);
            }
        }
    }
}

uint32_t kenux_ui_theme(void) {
    return s_theme;
}

int kenux_ui_theme_set(uint32_t theme) {
    if (theme != KENUX_UI_THEME_CLASSIC && theme != KENUX_UI_THEME_METRO && theme != KENUX_UI_THEME_MD3) {
        return -1;
    }
    s_theme = theme;
    return 0;
}

uint32_t kenux_ui_theme_color_scheme(uint32_t theme) {
    if (theme == KENUX_UI_THEME_CLASSIC) return s_classic_scheme;
    return s_metro_scheme;
}

uint32_t kenux_ui_theme_active_color_scheme(void) {
    return kenux_ui_theme_color_scheme(s_theme);
}

uint32_t kenux_ui_theme_scheme_accent(uint32_t theme, uint32_t scheme) {
    scheme = normalize_scheme(scheme);
    if (theme == KENUX_UI_THEME_CLASSIC) return s_classic_palettes[scheme].accent;
    return s_metro_palettes[scheme].accent;
}

int kenux_ui_theme_set_color_scheme(uint32_t theme, uint32_t scheme) {
    if (scheme >= KENUX_UI_SCHEME_COUNT) return -1;
    if (theme == KENUX_UI_THEME_CLASSIC) {
        s_classic_scheme = scheme;
        return 0;
    }
    if (theme == KENUX_UI_THEME_METRO) {
        s_metro_scheme = scheme;
        return 0;
    }
    return -1;
}

int kenux_ui_theme_set_appearance(uint32_t theme,
                                  uint32_t metro_scheme,
                                  uint32_t classic_scheme) {
    if (theme != KENUX_UI_THEME_CLASSIC && theme != KENUX_UI_THEME_METRO && theme != KENUX_UI_THEME_MD3) return -1;
    if (metro_scheme >= KENUX_UI_SCHEME_COUNT || classic_scheme >= KENUX_UI_SCHEME_COUNT) return -1;
    s_theme = theme;
    s_metro_scheme = metro_scheme;
    s_classic_scheme = classic_scheme;
    return 0;
}

uint32_t kenux_ui_color(uint32_t role) {
    if (s_theme == KENUX_UI_THEME_CLASSIC) {
        const kenux_ui_palette_t* p = &s_classic_palettes[normalize_scheme(s_classic_scheme)];
        static const uint32_t classic[] = {
            RGB(0x00, 0x00, 0x00), RGB(0xFF, 0xFF, 0xFF), RGB(0xC0, 0xC0, 0xC0),
            RGB(0xDF, 0xDF, 0xDF), RGB(0x80, 0x80, 0x80), RGB(0x00, 0x00, 0x80),
            RGB(0x80, 0x80, 0x80), RGB(0x00, 0x80, 0x80), RGB(0x00, 0x00, 0x00),
            RGB(0x00, 0x00, 0x80), RGB(0xB0, 0x30, 0x30), RGB(0x00, 0x80, 0x00),
            RGB(0xB0, 0x78, 0x00),
        };
        switch (role) {
            case KENUX_UI_COLOR_ACCENT: return p->accent;
            case KENUX_UI_COLOR_TITLE_INACTIVE: return p->inactive_title;
            case KENUX_UI_COLOR_DESKTOP: return p->desktop;
            case KENUX_UI_COLOR_BORDER: return p->border;
            case KENUX_UI_COLOR_SELECTION: return p->selection;
            default: return role < KENUX_UI_COLOR_COUNT ? classic[role] : classic[0];
        }
    }

    if (s_theme == KENUX_UI_THEME_MD3) {
        const kenux_ui_palette_t* p = &s_md3_palettes[normalize_scheme(s_metro_scheme)];
        static const uint32_t md3[] = {
            RGB(0xE6, 0xE1, 0xE6), RGB(0xF7, 0xFA, 0xFF), RGB(0x1C, 0x1B, 0x1E),
            RGB(0x49, 0x45, 0x4E), RGB(0x94, 0x8F, 0x99), RGB(0xCF, 0xBC, 0xFF),
            RGB(0x49, 0x45, 0x4E), RGB(0x1C, 0x1B, 0x1E), RGB(0x94, 0x8F, 0x99),
            RGB(0xCF, 0xBC, 0xFF), RGB(0xFF, 0xB4, 0xAB), RGB(0x6D, 0xDB, 0x6D),
            RGB(0xF5, 0xA5, 0x24),
        };
        switch (role) {
            case KENUX_UI_COLOR_ACCENT: return p->accent;
            case KENUX_UI_COLOR_TITLE_INACTIVE: return p->inactive_title;
            case KENUX_UI_COLOR_DESKTOP: return p->desktop;
            case KENUX_UI_COLOR_BORDER: return p->border;
            case KENUX_UI_COLOR_SELECTION: return p->selection;
            default: return role < KENUX_UI_COLOR_COUNT ? md3[role] : md3[0];
        }
    }

    const kenux_ui_palette_t* p = &s_metro_palettes[normalize_scheme(s_metro_scheme)];
    static const uint32_t metro[] = {
        RGB(0x17, 0x20, 0x33), RGB(0xFF, 0xFF, 0xFF), RGB(0xF7, 0xFA, 0xFF),
        RGB(0xEA, 0xF0, 0xFA), RGB(0x6B, 0x7A, 0x90), RGB(0x25, 0x8D, 0xFF),
        RGB(0x86, 0x98, 0xBC), RGB(0x10, 0x2A, 0x56), RGB(0xB6, 0xC6, 0xDE),
        RGB(0x25, 0x8D, 0xFF), RGB(0xF2, 0x43, 0x5E), RGB(0x1F, 0xB8, 0x69),
        RGB(0xF5, 0xA5, 0x24),
    };
    switch (role) {
        case KENUX_UI_COLOR_ACCENT: return p->accent;
        case KENUX_UI_COLOR_TITLE_INACTIVE: return p->inactive_title;
        case KENUX_UI_COLOR_DESKTOP: return p->desktop;
        case KENUX_UI_COLOR_BORDER: return p->border;
        case KENUX_UI_COLOR_SELECTION: return p->selection;
        default: return role < KENUX_UI_COLOR_COUNT ? metro[role] : metro[0];
    }
}

void kenux_ui_bind(kenux_ui_surface_t* surface, uint32_t* pixels,
                   uint32_t width, uint32_t height, uint32_t stride) {
    if (!surface) return;
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = stride ? stride : width;
    surface->framebuffer_format = false;
}

void kenux_ui_bind_framebuffer(kenux_ui_surface_t* surface) {
    if (!surface) return;
    surface->pixels = (uint32_t*)fb.base;
    surface->width = fb.width;
    surface->height = fb.height;
    surface->stride = fb.pitch / 4;
    surface->framebuffer_format = true;
}

int kenux_ui_hit(uint32_t px, uint32_t py, int32_t x, int32_t y,
                 uint32_t w, uint32_t h) {
    return (int32_t)px >= x && (int32_t)py >= y &&
           (int32_t)px < x + (int32_t)w &&
           (int32_t)py < y + (int32_t)h;
}

void kenux_ui_pixel(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t color) {
    if (!surface || !surface->pixels || x >= surface->width || y >= surface->height) return;
    surface->pixels[(uint64_t)y * surface->stride + x] =
        surface->framebuffer_format ? fb_convert_color(color) : color;
}

void kenux_ui_rect(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                   uint32_t w, uint32_t h, uint32_t color) {
    if (!surface || !surface->pixels || x >= surface->width || y >= surface->height) return;
    if (x + w > surface->width) w = surface->width - x;
    if (y + h > surface->height) h = surface->height - y;
    uint32_t out = surface->framebuffer_format ? fb_convert_color(color) : color;
    for (uint32_t yy = y; yy < y + h; yy++) {
        uint32_t* row = surface->pixels + (uint64_t)yy * surface->stride;
        for (uint32_t xx = x; xx < x + w; xx++) {
            row[xx] = out;
        }
    }
}

void kenux_ui_text(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                   const char* text, uint32_t fg, uint32_t bg) {
    if (!text) return;
    if (surface && surface->framebuffer_format) {
        font_draw_text_bg(x, y, text, fg, bg);
        return;
    }
    uint32_t cx = x;
    for (uint32_t i = 0; text[i]; i++) {
        draw_ascii_char_surface(surface, cx, y, (uint8_t)text[i], fg, bg, true, FONT_WIDTH);
        cx += FONT_WIDTH;
    }
}

void kenux_ui_text_clipped(kenux_ui_surface_t* surface, uint32_t x,
                           uint32_t y, uint32_t w, const char* text,
                           uint32_t fg, uint32_t bg) {
    if (!text || w == 0) return;
    if (surface && surface->framebuffer_format) {
        char tmp[96];
        uint32_t len = font_text_fit_bytes(text, w);
        if (len >= sizeof(tmp)) len = utf8_safe_prefix_len(text, sizeof(tmp) - 1);
        for (uint32_t i = 0; i < len; i++) tmp[i] = text[i];
        tmp[len] = '\0';
        font_draw_text_bg(x, y, tmp, fg, bg);
        return;
    }
    uint32_t max_chars = w / FONT_WIDTH;
    for (uint32_t i = 0; text[i] && i < max_chars; i++) {
        draw_ascii_char_surface(surface, x + i * FONT_WIDTH, y,
                                (uint8_t)text[i], fg, bg, true,
                                FONT_WIDTH);
    }
}

void kenux_ui_text_transparent(kenux_ui_surface_t* surface, uint32_t x,
                               uint32_t y, const char* text, uint32_t fg) {
    if (!surface || !surface->pixels || !text) return;
    if (surface->framebuffer_format) {
        font_draw_text(x, y, text, fg);
        return;
    }
    uint32_t cx = x;
    for (uint32_t i = 0; text[i]; i++) {
        draw_ascii_char_surface(surface, cx, y, (uint8_t)text[i], fg, 0, false, FONT_WIDTH);
        cx += FONT_WIDTH;
    }
}

void kenux_ui_text_transparent_clipped(kenux_ui_surface_t* surface,
                                       uint32_t x, uint32_t y, uint32_t w,
                                       const char* text, uint32_t fg) {
    if (!text || w == 0) return;
    if (surface && surface->framebuffer_format) {
        char tmp[96];
        uint32_t len = font_text_fit_bytes(text, w);
        if (len >= sizeof(tmp)) len = utf8_safe_prefix_len(text, sizeof(tmp) - 1);
        for (uint32_t i = 0; i < len; i++) tmp[i] = text[i];
        tmp[len] = '\0';
        font_draw_text(x, y, tmp, fg);
        return;
    }
    uint32_t max_chars = w / FONT_WIDTH;
    for (uint32_t i = 0; text[i] && i < max_chars; i++) {
        draw_ascii_char_surface(surface, x + i * FONT_WIDTH, y,
                                (uint8_t)text[i], fg, 0, false, FONT_WIDTH);
    }
}

void kenux_ui_bevel(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t fill, uint32_t flags) {
    if (w == 0 || h == 0) return;
    if (is_metro()) {
        uint32_t border = (flags & KENUX_UI_BUTTON_PRESSED)
                              ? kenux_ui_color(KENUX_UI_COLOR_ACCENT)
                              : kenux_ui_color(KENUX_UI_COLOR_BORDER);
        if (surface && surface->framebuffer_format) {
            fb_fill_rounded_rect(x, y, w, h, 12, fill);
            fb_blend_rect(x + 2, y + 2, w > 4 ? w - 4 : w, h / 2,
                          RGB(0xFF, 0xFF, 0xFF), 26);
        } else {
            kenux_ui_rect(surface, x, y, w, h, fill);
        }
        if (w > 3 && h > 3) {
            kenux_ui_rect(surface, x + 2, y, w - 4, 1, color_lighten(border, 55));
            kenux_ui_rect(surface, x + 2, y + h - 1, w - 4, 1, color_darken(border, 35));
            kenux_ui_rect(surface, x, y + 2, 1, h - 4, color_lighten(border, 35));
            kenux_ui_rect(surface, x + w - 1, y + 2, 1, h - 4, color_darken(border, 35));
        }
        return;
    }

    uint32_t pressed = flags & KENUX_UI_BUTTON_PRESSED;
    uint32_t tl = pressed ? kenux_ui_color(KENUX_UI_COLOR_MUTED)
                          : kenux_ui_color(KENUX_UI_COLOR_CONTENT);
    uint32_t br = pressed ? kenux_ui_color(KENUX_UI_COLOR_CONTENT)
                          : kenux_ui_color(KENUX_UI_COLOR_TEXT);
    kenux_ui_rect(surface, x, y, w, h, fill);
    kenux_ui_rect(surface, x, y, w, 1, tl);
    kenux_ui_rect(surface, x, y, 1, h, tl);
    kenux_ui_rect(surface, x + w - 1, y, 1, h, br);
    kenux_ui_rect(surface, x, y + h - 1, w, 1, br);
    if (w > 3 && h > 3) {
        kenux_ui_rect(surface, x + 1, y + 1, w - 2, 1,
                      pressed ? kenux_ui_color(KENUX_UI_COLOR_TEXT)
                              : kenux_ui_color(KENUX_UI_COLOR_SUBTLE));
        kenux_ui_rect(surface, x + 1, y + 1, 1, h - 2,
                      pressed ? kenux_ui_color(KENUX_UI_COLOR_TEXT)
                              : kenux_ui_color(KENUX_UI_COLOR_SUBTLE));
        kenux_ui_rect(surface, x + w - 2, y + 1, 1, h - 2,
                      pressed ? kenux_ui_color(KENUX_UI_COLOR_SUBTLE)
                              : kenux_ui_color(KENUX_UI_COLOR_MUTED));
        kenux_ui_rect(surface, x + 1, y + h - 2, w - 2, 1,
                      pressed ? kenux_ui_color(KENUX_UI_COLOR_SUBTLE)
                              : kenux_ui_color(KENUX_UI_COLOR_MUTED));
    }
}

void kenux_ui_inset(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t fill) {
    if (w == 0 || h == 0) return;
    if (surface && surface->framebuffer_format) {
        fb_fill_rounded_rect(x, y, w, h, 7, fill);
    } else {
        kenux_ui_rect(surface, x, y, w, h, fill);
    }
    kenux_ui_rect(surface, x, y, w, 1, color_lighten(kenux_ui_color(KENUX_UI_COLOR_BORDER), 35));
    kenux_ui_rect(surface, x, y + h - 1, w, 1, color_lighten(kenux_ui_color(KENUX_UI_COLOR_BORDER), 35));
}

void kenux_ui_button(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const char* label,
                     uint32_t flags) {
    uint32_t disabled = flags & KENUX_UI_BUTTON_DISABLED;
    uint32_t pressed = flags & KENUX_UI_BUTTON_PRESSED;
    uint32_t active = flags & KENUX_UI_BUTTON_ACTIVE;
    uint32_t fill = disabled ? RGB(0xEA, 0xEE, 0xF5)
                             : (active ? kenux_ui_color(KENUX_UI_COLOR_ACCENT)
                                       : RGB(0xF4, 0xF8, 0xFF));
    uint32_t text = disabled ? kenux_ui_color(KENUX_UI_COLOR_MUTED)
                             : (active ? kenux_ui_color(KENUX_UI_COLOR_CONTENT)
                                       : kenux_ui_color(KENUX_UI_COLOR_TEXT));
    uint32_t radius = h > 18 ? h / 2 : 9;
    if (surface && surface->framebuffer_format) {
        if (w > 4 && h > 4) {
            fb_fill_rounded_rect(x + 2, y + 3, w, h, radius, RGB(0x08, 0x10, 0x22));
        }
        fb_fill_rounded_rect(x, y, w, h, radius, pressed ? color_darken(fill, 12) : fill);
        fb_blend_rect(x + 3, y + 2, w > 6 ? w - 6 : w, h / 2, RGB(0xFF, 0xFF, 0xFF), active ? 20 : 34);
    } else {
        kenux_ui_rect(surface, x, y, w, h, fill);
    }
    if (w > 4) {
        kenux_ui_rect(surface, x + 2, y, w - 4, 1, color_lighten(fill, 45));
        kenux_ui_rect(surface, x + 2, y + h - 1, w - 4, 1, color_darken(fill, 25));
    }
    if (!label) return;
    uint32_t text_w = font_text_width(label);
    uint32_t tx = text_w < w ? x + (w - text_w) / 2 : x + 4;
    uint32_t ty = KENUX_UI_FONT_H < h ? y + (h - KENUX_UI_FONT_H) / 2 : y + 2;
    if (pressed) {
        tx++;
        ty++;
    }
    kenux_ui_text_transparent_clipped(surface, tx, ty, w > 8 ? w - 8 : w, label, text);
}

void kenux_ui_panel(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t color) {
    if (surface && surface->framebuffer_format) {
        fb_fill_rounded_rect(x + 4, y + 5, w, h, 16, RGB(0x08, 0x10, 0x22));
        fb_fill_rounded_rect(x, y, w, h, 16, color);
        fb_blend_rect(x + 3, y + 3, w > 6 ? w - 6 : w, h / 2,
                      RGB(0xFF, 0xFF, 0xFF), 16);
        if (w > 6) {
            kenux_ui_rect(surface, x + 3, y, w - 6, 1,
                          color_lighten(kenux_ui_color(KENUX_UI_COLOR_BORDER), 50));
        }
    } else {
        kenux_ui_bevel(surface, x, y, w, h, color, 0);
    }
}

void kenux_ui_checkbox(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       const char* label, int checked, uint32_t flags) {
    uint32_t disabled = flags & KENUX_UI_BUTTON_DISABLED;
    uint32_t box = KENUX_UI_FONT_H;
    uint32_t bg = disabled ? kenux_ui_color(KENUX_UI_COLOR_SUBTLE)
                           : kenux_ui_color(KENUX_UI_COLOR_CONTENT);
    kenux_ui_inset(surface, x, y + 2, box, box, bg);
    if (checked) {
        uint32_t check = disabled ? kenux_ui_color(KENUX_UI_COLOR_MUTED)
                                  : kenux_ui_color(KENUX_UI_COLOR_ACCENT);
        kenux_ui_rect(surface, x + 4, y + 9, 3, 3, check);
        kenux_ui_rect(surface, x + 7, y + 12, 3, 3, check);
        kenux_ui_rect(surface, x + 10, y + 6, 3, 9, check);
    }
    if (label) {
        kenux_ui_text_transparent(surface, x + box + 6, y + 2, label,
                                  disabled ? kenux_ui_color(KENUX_UI_COLOR_MUTED)
                                           : kenux_ui_color(KENUX_UI_COLOR_TEXT));
    }
}

void kenux_ui_progress(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t value, uint32_t max) {
    if (surface && surface->framebuffer_format) {
        fb_fill_rounded_rect(x, y, w, h, h / 2, RGB(0xE8, 0xEF, 0xFA));
    } else {
        kenux_ui_inset(surface, x, y, w, h, kenux_ui_color(KENUX_UI_COLOR_CONTENT));
    }
    if (w <= 4 || h <= 4 || max == 0) return;
    if (value > max) value = max;
    uint32_t fill = ((w - 6) * value) / max;
    if (fill) {
        if (surface && surface->framebuffer_format) {
            fb_fill_rounded_rect(x + 3, y + 3, fill, h - 6, (h - 6) / 2,
                                 kenux_ui_color(KENUX_UI_COLOR_ACCENT));
            fb_blend_rect(x + 3, y + 3, fill, (h - 6) / 2, RGB(0xFF, 0xFF, 0xFF), 28);
        } else {
            kenux_ui_rect(surface, x + 2, y + 2, fill, h - 4,
                          kenux_ui_color(KENUX_UI_COLOR_ACCENT));
        }
    }
}

void kenux_ui_listview_header(kenux_ui_surface_t* surface, uint32_t x,
                              uint32_t y, uint32_t w,
                              const kenux_ui_list_column_t* cols,
                              uint32_t count) {
    uint32_t cx = x;
    uint32_t header_h = KENUX_UI_FONT_H + 8u;
    kenux_ui_bevel(surface, x, y, w, header_h,
                   kenux_ui_color(KENUX_UI_COLOR_SUBTLE), 0);
    for (uint32_t i = 0; i < count && cx < x + w; i++) {
        uint32_t cw = cols && cols[i].width ? cols[i].width : (x + w - cx);
        if (cx + cw > x + w) cw = x + w - cx;
        kenux_ui_text_transparent_clipped(surface, cx + 6, y + 4,
                                          cw > 12 ? cw - 12 : cw,
                                          cols && cols[i].label ? cols[i].label : "",
                                          kenux_ui_color(KENUX_UI_COLOR_TEXT));
        if (i + 1 < count && cw > 1) {
            kenux_ui_rect(surface, cx + cw - 1, y + 2, 1,
                          header_h - 4,
                          kenux_ui_color(KENUX_UI_COLOR_BORDER));
        }
        cx += cw;
    }
}

void kenux_ui_listview_row(kenux_ui_surface_t* surface, uint32_t x,
                           uint32_t y, uint32_t w,
                           const kenux_ui_list_column_t* cols,
                           const char* const cells[], uint32_t count,
                           uint32_t flags) {
    uint32_t selected = flags & KENUX_UI_MENU_SELECTED;
    uint32_t bg = selected ? kenux_ui_color(KENUX_UI_COLOR_ACCENT)
                           : kenux_ui_color(KENUX_UI_COLOR_CONTENT);
    uint32_t fg = selected ? kenux_ui_color(KENUX_UI_COLOR_CONTENT)
                           : kenux_ui_color(KENUX_UI_COLOR_TEXT);
    uint32_t cx = x;
    uint32_t row_h = KENUX_UI_FONT_H + 4u;
    kenux_ui_rect(surface, x, y, w, row_h, bg);
    for (uint32_t i = 0; i < count && cx < x + w; i++) {
        uint32_t cw = cols && cols[i].width ? cols[i].width : (x + w - cx);
        if (cx + cw > x + w) cw = x + w - cx;
        kenux_ui_text_clipped(surface, cx + 4, y + 2,
                              cw > 8 ? cw - 8 : cw,
                              cells && cells[i] ? cells[i] : "", fg, bg);
        cx += cw;
    }
}

void kenux_ui_listview_state_init(kenux_ui_listview_state_t* state,
                                  uint32_t visible_rows,
                                  uint32_t row_height) {
    if (!state) return;
    state->row_count = 0;
    state->visible_rows = visible_rows;
    state->row_height = row_height ? row_height : (KENUX_UI_FONT_H + 8u);
    state->scroll = 0;
    state->selected = -1;
    state->focused = 0;
}

static void listview_clamp(kenux_ui_listview_state_t* state) {
    if (!state) return;
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    uint32_t max_scroll = state->row_count > visible ? state->row_count - visible : 0;
    if (state->scroll > max_scroll) state->scroll = max_scroll;
    if (state->row_count == 0) {
        state->selected = -1;
        state->scroll = 0;
        return;
    }
    if (state->selected >= (int32_t)state->row_count) {
        state->selected = (int32_t)state->row_count - 1;
    }
}

static void listview_ensure_selected_visible(kenux_ui_listview_state_t* state) {
    if (!state || state->selected < 0) return;
    uint32_t selected = (uint32_t)state->selected;
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    if (selected < state->scroll) {
        state->scroll = selected;
    } else if (selected >= state->scroll + visible) {
        state->scroll = selected - visible + 1u;
    }
    listview_clamp(state);
}

void kenux_ui_listview_state_set_count(kenux_ui_listview_state_t* state,
                                       uint32_t row_count) {
    if (!state) return;
    state->row_count = row_count;
    listview_clamp(state);
}

int kenux_ui_listview_state_handle_key(kenux_ui_listview_state_t* state,
                                       uint8_t keycode,
                                       uint32_t* activated) {
    if (activated) *activated = 0;
    if (!state || !state->focused || state->row_count == 0) return 0;
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    if (state->selected < 0) state->selected = 0;
    switch (keycode) {
        case 72:
            if (state->selected > 0) {
                --state->selected;
                listview_ensure_selected_visible(state);
                return 1;
            }
            return 0;
        case 80:
            if ((uint32_t)state->selected + 1u < state->row_count) {
                ++state->selected;
                listview_ensure_selected_visible(state);
                return 1;
            }
            return 0;
        case 73:
            state->selected = (uint32_t)state->selected > visible
                                  ? state->selected - (int32_t)visible : 0;
            listview_ensure_selected_visible(state);
            return 1;
        case 81:
            state->selected = (uint32_t)state->selected + visible < state->row_count
                                  ? state->selected + (int32_t)visible
                                  : (int32_t)state->row_count - 1;
            listview_ensure_selected_visible(state);
            return 1;
        case 71:
            state->selected = 0;
            listview_ensure_selected_visible(state);
            return 1;
        case 79:
            state->selected = (int32_t)state->row_count - 1;
            listview_ensure_selected_visible(state);
            return 1;
        case KENUX_UI_KEY_ENTER:
            if (activated && state->selected >= 0) *activated = 1;
            return state->selected >= 0;
        default:
            return 0;
    }
}

int kenux_ui_listview_state_handle_mouse(kenux_ui_listview_state_t* state,
                                         int32_t px, int32_t py,
                                         uint32_t x, uint32_t rows_y,
                                         uint32_t w,
                                         uint32_t* activated) {
    if (activated) *activated = 0;
    if (!state) return 0;
    if (!kenux_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x,
                      (int32_t)rows_y, w,
                      state->visible_rows * state->row_height)) {
        state->focused = 0;
        return 0;
    }
    state->focused = 1;
    uint32_t row = ((uint32_t)py - rows_y) / state->row_height;
    uint32_t index = state->scroll + row;
    if (index >= state->row_count) return 1;
    if (state->selected == (int32_t)index && activated) *activated = 1;
    state->selected = (int32_t)index;
    listview_ensure_selected_visible(state);
    return 1;
}

int kenux_ui_listview_state_handle_wheel(kenux_ui_listview_state_t* state,
                                         int32_t wheel_delta) {
    if (!state || wheel_delta == 0) return 0;
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    if (state->row_count <= visible) return 0;
    uint32_t old = state->scroll;
    uint32_t max_scroll = state->row_count - visible;
    uint32_t steps = wheel_delta < 0 ? (uint32_t)(-wheel_delta) : (uint32_t)wheel_delta;
    if (steps == 0) steps = 1;
    if (wheel_delta > 0) {
        state->scroll = state->scroll > steps ? state->scroll - steps : 0;
    } else {
        state->scroll = state->scroll + steps < max_scroll ? state->scroll + steps : max_scroll;
    }
    return old != state->scroll;
}

static uint32_t treeview_count(uint32_t count) {
    return count > KENUX_UI_TREEVIEW_MAX_ITEMS ? KENUX_UI_TREEVIEW_MAX_ITEMS : count;
}

static int treeview_item_index(const kenux_ui_treeview_item_t* items,
                               uint32_t count, uint32_t id) {
    for (uint32_t i = 0; i < count; i++) {
        if (items[i].id == id) return (int)i;
    }
    return -1;
}

static int treeview_parent_index(const kenux_ui_treeview_item_t* items,
                                 uint32_t count, uint32_t index) {
    if (!items || index >= count) return -1;
    uint32_t parent_id = items[index].parent_id;
    if (parent_id == 0 || parent_id == items[index].id) return -1;
    return treeview_item_index(items, count, parent_id);
}

static int treeview_has_children(const kenux_ui_treeview_item_t* items,
                                 uint32_t count, uint32_t index) {
    if (!items || index >= count || items[index].id == 0) return 0;
    for (uint32_t i = 0; i < count; i++) {
        if (i != index && items[i].parent_id == items[index].id) return 1;
    }
    return 0;
}

static int treeview_is_collapsed(const kenux_ui_treeview_state_t* state,
                                 uint32_t id) {
    if (!state) return 0;
    for (uint32_t i = 0; i < state->collapsed_count; i++) {
        if (state->collapsed_ids[i] == id) return 1;
    }
    return 0;
}

static void treeview_remove_collapsed(kenux_ui_treeview_state_t* state,
                                      uint32_t id) {
    if (!state) return;
    for (uint32_t i = 0; i < state->collapsed_count; i++) {
        if (state->collapsed_ids[i] == id) {
            for (uint32_t next = i + 1u; next < state->collapsed_count; next++) {
                state->collapsed_ids[next - 1u] = state->collapsed_ids[next];
            }
            --state->collapsed_count;
            return;
        }
    }
}

static void treeview_set_collapsed(kenux_ui_treeview_state_t* state,
                                   uint32_t id, int collapsed) {
    if (!state) return;
    if (!collapsed) {
        treeview_remove_collapsed(state, id);
        return;
    }
    if (treeview_is_collapsed(state, id) ||
        state->collapsed_count >= KENUX_UI_TREEVIEW_MAX_ITEMS) {
        return;
    }
    state->collapsed_ids[state->collapsed_count++] = id;
}

static void treeview_prune_collapsed(kenux_ui_treeview_state_t* state,
                                     const kenux_ui_treeview_item_t* items,
                                     uint32_t count) {
    if (!state) return;
    uint32_t kept = 0;
    if (state->collapsed_count > KENUX_UI_TREEVIEW_MAX_ITEMS) {
        state->collapsed_count = KENUX_UI_TREEVIEW_MAX_ITEMS;
    }
    for (uint32_t i = 0; i < state->collapsed_count; i++) {
        int index = treeview_item_index(items, count, state->collapsed_ids[i]);
        if (index >= 0 && treeview_has_children(items, count, (uint32_t)index)) {
            state->collapsed_ids[kept++] = state->collapsed_ids[i];
        }
    }
    state->collapsed_count = kept;
}

static void treeview_append_visible(kenux_ui_treeview_state_t* state,
                                    const kenux_ui_treeview_item_t* items,
                                    uint32_t count, uint32_t index,
                                    uint8_t depth,
                                    uint8_t visited[KENUX_UI_TREEVIEW_MAX_ITEMS]) {
    if (!state || !items || index >= count || visited[index] ||
        state->visible_count >= KENUX_UI_TREEVIEW_MAX_ITEMS) {
        return;
    }
    visited[index] = 1;
    state->visible_indices[state->visible_count] = index;
    state->visible_depths[state->visible_count] = depth;
    ++state->visible_count;
    if (treeview_is_collapsed(state, items[index].id)) return;
    for (uint32_t i = 0; i < count; i++) {
        if (!visited[i] && items[index].id != 0 &&
            items[i].parent_id == items[index].id) {
            treeview_append_visible(state, items, count, i,
                                    depth < 31u ? depth + 1u : depth,
                                    visited);
        }
    }
}

static int treeview_selected_row(const kenux_ui_treeview_state_t* state,
                                 const kenux_ui_treeview_item_t* items) {
    if (!state || !items || !state->has_selection) return -1;
    for (uint32_t i = 0; i < state->visible_count; i++) {
        if (items[state->visible_indices[i]].id == state->selected_id) return (int)i;
    }
    return -1;
}

static void treeview_clamp(kenux_ui_treeview_state_t* state,
                           const kenux_ui_treeview_item_t* items) {
    if (!state) return;
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    if (state->visible_count == 0) {
        state->scroll = 0;
        state->has_selection = 0;
        return;
    }
    uint32_t max_scroll = state->visible_count > visible ? state->visible_count - visible : 0;
    if (state->scroll > max_scroll) state->scroll = max_scroll;
    int selected_row = treeview_selected_row(state, items);
    if (selected_row < 0) {
        state->selected_id = items[state->visible_indices[0]].id;
        state->has_selection = 1;
        selected_row = 0;
    }
    if ((uint32_t)selected_row < state->scroll) {
        state->scroll = (uint32_t)selected_row;
    } else if ((uint32_t)selected_row >= state->scroll + visible) {
        state->scroll = (uint32_t)selected_row - visible + 1u;
    }
}

static void treeview_select_row(kenux_ui_treeview_state_t* state,
                                const kenux_ui_treeview_item_t* items,
                                uint32_t row) {
    if (!state || !items || row >= state->visible_count) return;
    state->selected_id = items[state->visible_indices[row]].id;
    state->has_selection = 1;
    treeview_clamp(state, items);
}

void kenux_ui_treeview_state_init(kenux_ui_treeview_state_t* state,
                                  uint32_t visible_rows,
                                  uint32_t row_height) {
    if (!state) return;
    for (uint32_t i = 0; i < sizeof(*state); i++) {
        ((uint8_t*)state)[i] = 0;
    }
    state->visible_rows = visible_rows ? visible_rows : 1u;
    state->row_height = row_height ? row_height : KENUX_UI_FONT_H + 8u;
}

void kenux_ui_treeview_state_set_viewport(kenux_ui_treeview_state_t* state,
                                          uint32_t visible_rows) {
    if (!state) return;
    state->visible_rows = visible_rows ? visible_rows : 1u;
}

void kenux_ui_treeview_state_sync(kenux_ui_treeview_state_t* state,
                                  const kenux_ui_treeview_item_t* items,
                                  uint32_t count) {
    uint8_t visited[KENUX_UI_TREEVIEW_MAX_ITEMS] = {0};
    count = treeview_count(count);
    if (!state) return;
    state->visible_count = 0;
    if (!items || count == 0) {
        treeview_clamp(state, items);
        return;
    }
    treeview_prune_collapsed(state, items, count);
    for (uint32_t i = 0; i < count; i++) {
        if (treeview_parent_index(items, count, i) < 0) {
            treeview_append_visible(state, items, count, i, 0, visited);
        }
    }
    for (uint32_t i = 0; i < count; i++) {
        treeview_append_visible(state, items, count, i, 0, visited);
    }
    treeview_clamp(state, items);
}

void kenux_ui_treeview(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       uint32_t w, const kenux_ui_list_column_t* cols,
                       uint32_t col_count,
                       const kenux_ui_treeview_item_t* items,
                       uint32_t count,
                       kenux_ui_treeview_state_t* state) {
    if (!surface || !state) return;
    if (state->row_height < KENUX_UI_FONT_H + 4u) {
        state->row_height = KENUX_UI_FONT_H + 4u;
    }
    kenux_ui_treeview_state_sync(state, items, count);
    uint32_t row_h = state->row_height;
    uint32_t rows_y = y + KENUX_UI_FONT_H + 12u;
    kenux_ui_listview_header(surface, x, y, w, cols, col_count);
    uint32_t rows = state->visible_count > state->scroll
                        ? state->visible_count - state->scroll : 0;
    if (rows > state->visible_rows) rows = state->visible_rows;
    for (uint32_t row = 0; row < rows; row++) {
        uint32_t visible_index = state->scroll + row;
        uint32_t item_index = state->visible_indices[visible_index];
        uint32_t depth = state->visible_depths[visible_index];
        uint32_t selected = state->has_selection &&
                            items[item_index].id == state->selected_id;
        uint32_t bg = selected ? kenux_ui_color(KENUX_UI_COLOR_ACCENT)
                               : kenux_ui_color(KENUX_UI_COLOR_CONTENT);
        uint32_t fg = selected ? kenux_ui_color(KENUX_UI_COLOR_CONTENT)
                               : kenux_ui_color(KENUX_UI_COLOR_TEXT);
        uint32_t row_y = rows_y + row * row_h;
        uint32_t cx = x;
        uint32_t indent = depth * 14u;
        int has_children = treeview_has_children(items, treeview_count(count), item_index);
        kenux_ui_rect(surface, x, row_y, w, row_h, bg);
        for (uint32_t col = 0; col < col_count && cx < x + w; col++) {
            uint32_t cw = cols && cols[col].width ? cols[col].width : x + w - cx;
            uint32_t text_x = cx + 4u;
            if (cx + cw > x + w) cw = x + w - cx;
            if (col == 0) {
                uint32_t glyph_x = cx + 4u + indent;
                text_x = glyph_x + 16u;
                if (has_children && glyph_x + 10u < cx + cw) {
                    kenux_ui_rect(surface, glyph_x, row_y + 6u, 10u, 10u, bg);
                    kenux_ui_rect(surface, glyph_x, row_y + 6u, 10u, 1u, fg);
                    kenux_ui_rect(surface, glyph_x, row_y + 15u, 10u, 1u, fg);
                    kenux_ui_rect(surface, glyph_x, row_y + 6u, 1u, 10u, fg);
                    kenux_ui_rect(surface, glyph_x + 9u, row_y + 6u, 1u, 10u, fg);
                    kenux_ui_rect(surface, glyph_x + 2u, row_y + 10u, 6u, 1u, fg);
                    if (treeview_is_collapsed(state, items[item_index].id)) {
                        kenux_ui_rect(surface, glyph_x + 5u, row_y + 8u, 1u, 6u, fg);
                    }
                }
            }
            if (text_x < cx + cw) {
                kenux_ui_text_clipped(surface, text_x, row_y + 4u,
                                      cx + cw - text_x,
                                      items[item_index].cells &&
                                              items[item_index].cells[col]
                                          ? items[item_index].cells[col] : "",
                                      fg, bg);
            }
            cx += cw;
        }
    }
}

int kenux_ui_treeview_state_handle_key(kenux_ui_treeview_state_t* state,
                                       const kenux_ui_treeview_item_t* items,
                                       uint32_t count, uint8_t keycode,
                                       uint32_t* activated) {
    if (activated) *activated = 0;
    if (!state || !state->focused) return 0;
    kenux_ui_treeview_state_sync(state, items, count);
    if (state->visible_count == 0) return 0;
    int selected_row = treeview_selected_row(state, items);
    if (selected_row < 0) return 0;
    uint32_t selected_index = state->visible_indices[selected_row];
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    switch (keycode) {
        case 72:
            if (selected_row > 0) {
                treeview_select_row(state, items, (uint32_t)selected_row - 1u);
                return 1;
            }
            return 0;
        case 80:
            if ((uint32_t)selected_row + 1u < state->visible_count) {
                treeview_select_row(state, items, (uint32_t)selected_row + 1u);
                return 1;
            }
            return 0;
        case 73:
            treeview_select_row(state, items,
                                (uint32_t)selected_row > visible
                                    ? (uint32_t)selected_row - visible : 0);
            return 1;
        case 81:
            treeview_select_row(state, items,
                                (uint32_t)selected_row + visible < state->visible_count
                                    ? (uint32_t)selected_row + visible
                                    : state->visible_count - 1u);
            return 1;
        case 71:
            treeview_select_row(state, items, 0);
            return 1;
        case 79:
            treeview_select_row(state, items, state->visible_count - 1u);
            return 1;
        case 75: {
            if (treeview_has_children(items, treeview_count(count), selected_index) &&
                !treeview_is_collapsed(state, items[selected_index].id)) {
                treeview_set_collapsed(state, items[selected_index].id, 1);
                kenux_ui_treeview_state_sync(state, items, count);
                return 1;
            }
            int parent = treeview_parent_index(items, treeview_count(count), selected_index);
            if (parent >= 0) {
                state->selected_id = items[parent].id;
                state->has_selection = 1;
                kenux_ui_treeview_state_sync(state, items, count);
                return 1;
            }
            return 0;
        }
        case 77:
            if (treeview_has_children(items, treeview_count(count), selected_index)) {
                if (treeview_is_collapsed(state, items[selected_index].id)) {
                    treeview_set_collapsed(state, items[selected_index].id, 0);
                    kenux_ui_treeview_state_sync(state, items, count);
                } else if ((uint32_t)selected_row + 1u < state->visible_count &&
                           state->visible_depths[selected_row + 1] >
                               state->visible_depths[selected_row]) {
                    treeview_select_row(state, items, (uint32_t)selected_row + 1u);
                }
                return 1;
            }
            return 0;
        case KENUX_UI_KEY_ENTER:
            if (treeview_has_children(items, treeview_count(count), selected_index)) {
                treeview_set_collapsed(state, items[selected_index].id,
                                       !treeview_is_collapsed(state, items[selected_index].id));
                kenux_ui_treeview_state_sync(state, items, count);
            }
            if (activated) *activated = 1;
            return 1;
        default:
            return 0;
    }
}

int kenux_ui_treeview_state_handle_mouse(kenux_ui_treeview_state_t* state,
                                         const kenux_ui_treeview_item_t* items,
                                         uint32_t count, int32_t px,
                                         int32_t py, uint32_t x,
                                         uint32_t rows_y, uint32_t w,
                                         uint32_t* activated) {
    if (activated) *activated = 0;
    if (!state) return 0;
    kenux_ui_treeview_state_sync(state, items, count);
    if (!kenux_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x,
                      (int32_t)rows_y, w,
                      state->visible_rows * state->row_height)) {
        state->focused = 0;
        return 0;
    }
    state->focused = 1;
    uint32_t row = ((uint32_t)py - rows_y) / state->row_height;
    uint32_t visible_index = state->scroll + row;
    if (visible_index >= state->visible_count) return 1;
    uint32_t item_index = state->visible_indices[visible_index];
    uint32_t glyph_x = x + 4u + state->visible_depths[visible_index] * 14u;
    if (treeview_has_children(items, treeview_count(count), item_index) &&
        px >= (int32_t)glyph_x && px < (int32_t)(glyph_x + 12u)) {
        state->selected_id = items[item_index].id;
        state->has_selection = 1;
        treeview_set_collapsed(state, items[item_index].id,
                               !treeview_is_collapsed(state, items[item_index].id));
        kenux_ui_treeview_state_sync(state, items, count);
        return 1;
    }
    if (state->has_selection && state->selected_id == items[item_index].id && activated) {
        *activated = 1;
    }
    treeview_select_row(state, items, visible_index);
    return 1;
}

int kenux_ui_treeview_state_handle_wheel(kenux_ui_treeview_state_t* state,
                                         int32_t wheel_delta) {
    if (!state || wheel_delta == 0) return 0;
    uint32_t visible = state->visible_rows ? state->visible_rows : 1u;
    if (state->visible_count <= visible) return 0;
    uint32_t old = state->scroll;
    uint32_t max_scroll = state->visible_count - visible;
    uint32_t steps = wheel_delta < 0 ? (uint32_t)(-wheel_delta) : (uint32_t)wheel_delta;
    if (steps == 0) steps = 1;
    if (wheel_delta > 0) {
        state->scroll = state->scroll > steps ? state->scroll - steps : 0;
    } else {
        state->scroll = state->scroll + steps < max_scroll ? state->scroll + steps : max_scroll;
    }
    return old != state->scroll;
}

static uint32_t scrollbar_thumb_pos(uint32_t track_len, uint32_t value,
                                    uint32_t max, uint32_t page,
                                    uint32_t* thumb_len) {
    if (!thumb_len) return 0;
    if (track_len < 8) {
        *thumb_len = track_len;
        return 0;
    }
    if (max <= page || max == 0) {
        *thumb_len = track_len;
        return 0;
    }
    uint32_t len = (track_len * page) / max;
    if (len < 12) len = 12;
    if (len > track_len) len = track_len;
    uint32_t range = track_len - len;
    if (value > max - page) value = max - page;
    *thumb_len = len;
    return range ? (range * value) / (max - page) : 0;
}

void kenux_ui_vscrollbar(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, uint32_t value,
                         uint32_t max, uint32_t page, uint32_t flags) {
    uint32_t arrow_h = w < h / 2u ? w : h / 2u;
    uint32_t track_y = y + arrow_h;
    uint32_t track_h = h > arrow_h * 2u ? h - arrow_h * 2u : 0;
    uint32_t disabled = flags & KENUX_UI_SCROLLBAR_DISABLED;
    kenux_ui_button(surface, x, y, w, arrow_h, "^",
                    disabled ? KENUX_UI_BUTTON_DISABLED : 0);
    kenux_ui_button(surface, x, y + h - arrow_h, w, arrow_h, "v",
                    disabled ? KENUX_UI_BUTTON_DISABLED : 0);
    kenux_ui_inset(surface, x, track_y, w, track_h,
                   kenux_ui_color(KENUX_UI_COLOR_SUBTLE));
    if (disabled || track_h == 0) return;
    uint32_t thumb_h;
    uint32_t thumb_y = scrollbar_thumb_pos(track_h, value, max, page, &thumb_h);
    kenux_ui_bevel(surface, x + 2, track_y + thumb_y, w > 4 ? w - 4 : w,
                   thumb_h, kenux_ui_color(KENUX_UI_COLOR_SURFACE), 0);
}

void kenux_ui_hscrollbar(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, uint32_t value,
                         uint32_t max, uint32_t page, uint32_t flags) {
    uint32_t arrow_w = h < w / 2u ? h : w / 2u;
    uint32_t track_x = x + arrow_w;
    uint32_t track_w = w > arrow_w * 2u ? w - arrow_w * 2u : 0;
    uint32_t disabled = flags & KENUX_UI_SCROLLBAR_DISABLED;
    kenux_ui_button(surface, x, y, arrow_w, h, "<",
                    disabled ? KENUX_UI_BUTTON_DISABLED : 0);
    kenux_ui_button(surface, x + w - arrow_w, y, arrow_w, h, ">",
                    disabled ? KENUX_UI_BUTTON_DISABLED : 0);
    kenux_ui_inset(surface, track_x, y, track_w, h,
                   kenux_ui_color(KENUX_UI_COLOR_SUBTLE));
    if (disabled || track_w == 0) return;
    uint32_t thumb_w;
    uint32_t thumb_x = scrollbar_thumb_pos(track_w, value, max, page, &thumb_w);
    kenux_ui_bevel(surface, track_x + thumb_x, y + 2, thumb_w,
                   h > 4 ? h - 4 : h,
                   kenux_ui_color(KENUX_UI_COLOR_SURFACE), 0);
}

void kenux_ui_scroll_view_frame(kenux_ui_surface_t* surface, uint32_t x,
                                uint32_t y, uint32_t w, uint32_t h) {
    kenux_ui_inset(surface, x, y, w, h,
                   kenux_ui_color(KENUX_UI_COLOR_CONTENT));
}

int kenux_ui_vscrollbar_handle_mouse(uint32_t* value, uint32_t max,
                                     uint32_t page, uint32_t x,
                                     uint32_t y, uint32_t w,
                                     uint32_t h, int32_t px, int32_t py) {
    if (!value || max <= page ||
        !kenux_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x, (int32_t)y, w, h)) {
        return 0;
    }
    uint32_t old = *value;
    uint32_t max_value = max - page;
    uint32_t arrow_h = w < h / 2u ? w : h / 2u;
    if (py < (int32_t)(y + arrow_h)) {
        if (*value > 0) --(*value);
    } else if (py >= (int32_t)(y + h - arrow_h)) {
        if (*value < max_value) ++(*value);
    } else if (py < (int32_t)(y + h / 2u)) {
        *value = *value > page ? *value - page : 0;
    } else {
        *value = *value + page < max_value ? *value + page : max_value;
    }
    return old != *value;
}

int kenux_ui_vscrollbar_handle_wheel(uint32_t* value, uint32_t max,
                                     uint32_t page, int32_t wheel_delta) {
    if (!value || max <= page || wheel_delta == 0) return 0;
    uint32_t old = *value;
    uint32_t max_value = max - page;
    uint32_t steps = wheel_delta < 0 ? (uint32_t)(-wheel_delta) : (uint32_t)wheel_delta;
    if (steps == 0) steps = 1;
    if (wheel_delta > 0) {
        *value = *value > steps ? *value - steps : 0;
    } else {
        *value = *value + steps < max_value ? *value + steps : max_value;
    }
    return old != *value;
}

int kenux_ui_hscrollbar_handle_mouse(uint32_t* value, uint32_t max,
                                     uint32_t page, uint32_t x,
                                     uint32_t y, uint32_t w,
                                     uint32_t h, int32_t px, int32_t py) {
    if (!value || max <= page ||
        !kenux_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x, (int32_t)y, w, h)) {
        return 0;
    }
    uint32_t old = *value;
    uint32_t max_value = max - page;
    uint32_t arrow_w = h < w / 2u ? h : w / 2u;
    if (px < (int32_t)(x + arrow_w)) {
        if (*value > 0) --(*value);
    } else if (px >= (int32_t)(x + w - arrow_w)) {
        if (*value < max_value) ++(*value);
    } else if (px < (int32_t)(x + w / 2u)) {
        *value = *value > page ? *value - page : 0;
    } else {
        *value = *value + page < max_value ? *value + page : max_value;
    }
    return old != *value;
}

uint32_t kenux_ui_context_menu_height(uint32_t count) {
    return 8 + count * (KENUX_UI_FONT_H + 8);
}

void kenux_ui_context_menu(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                           uint32_t w,
                           const kenux_ui_context_menu_item_t* items,
                           uint32_t count) {
    uint32_t row_h = KENUX_UI_FONT_H + 8;
    uint32_t h = kenux_ui_context_menu_height(count);
    kenux_ui_bevel(surface, x, y, w, h, kenux_ui_color(KENUX_UI_COLOR_SURFACE), 0);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t row_y = y + 4 + i * row_h;
        uint32_t flags = items ? items[i].flags : KENUX_UI_MENU_DISABLED;
        const char* label = items ? items[i].label : "";
        if (flags & KENUX_UI_MENU_SEPARATOR) {
            kenux_ui_rect(surface, x + 4, row_y + row_h / 2, w > 8 ? w - 8 : w, 1,
                          kenux_ui_color(KENUX_UI_COLOR_MUTED));
            kenux_ui_rect(surface, x + 4, row_y + row_h / 2 + 1, w > 8 ? w - 8 : w, 1,
                          kenux_ui_color(KENUX_UI_COLOR_CONTENT));
            continue;
        }
        if ((flags & KENUX_UI_MENU_SELECTED) && !(flags & KENUX_UI_MENU_DISABLED)) {
            kenux_ui_rect(surface, x + 2, row_y, w > 4 ? w - 4 : w, row_h,
                          kenux_ui_color(KENUX_UI_COLOR_SELECTION));
        }
        kenux_ui_text_transparent_clipped(surface, x + 8, row_y + 4,
                                          w > 16 ? w - 16 : w, label ? label : "",
                                          (flags & KENUX_UI_MENU_DISABLED)
                                              ? kenux_ui_color(KENUX_UI_COLOR_MUTED)
                                              : kenux_ui_color(KENUX_UI_COLOR_TEXT));
    }
}

int kenux_ui_context_menu_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                              uint32_t w,
                              const kenux_ui_context_menu_item_t* items,
                              uint32_t count, uint32_t* out_id) {
    uint32_t row_h = KENUX_UI_FONT_H + 8;
    uint32_t h = kenux_ui_context_menu_height(count);
    if (out_id) *out_id = 0;
    if (!kenux_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x, (int32_t)y, w, h)) return 0;
    if (py < (int32_t)y + 4) return 1;
    uint32_t index = ((uint32_t)py - y - 4) / row_h;
    if (!items || index >= count ||
        (items[index].flags & (KENUX_UI_MENU_SEPARATOR | KENUX_UI_MENU_DISABLED))) {
        return 1;
    }
    if (out_id) *out_id = items[index].id;
    return 1;
}

void kenux_ui_layout_begin(kenux_ui_layout_t* layout, uint32_t x, uint32_t y,
                           uint32_t w, uint32_t h, uint32_t gap) {
    if (!layout) return;
    layout->x = x;
    layout->y = y;
    layout->w = w;
    layout->h = h;
    layout->gap = gap;
    layout->cursor_x = x;
    layout->cursor_y = y;
    layout->row_h = 0;
}

kenux_ui_rect_t kenux_ui_layout_next(kenux_ui_layout_t* layout,
                                     uint32_t preferred_w,
                                     uint32_t preferred_h) {
    kenux_ui_rect_t rect = { 0, 0, 0, 0 };
    if (!layout || layout->w == 0 || layout->h == 0) return rect;
    if (preferred_h == 0) preferred_h = KENUX_UI_BUTTON_H;
    if (preferred_w == 0 || preferred_w > layout->w) preferred_w = layout->w;
    uint32_t right = layout->x + layout->w;
    if (layout->cursor_x != layout->x && layout->cursor_x + preferred_w > right) {
        layout->cursor_x = layout->x;
        layout->cursor_y += layout->row_h + layout->gap;
        layout->row_h = 0;
    }
    if (layout->cursor_y >= layout->y + layout->h) return rect;
    if (layout->cursor_y + preferred_h > layout->y + layout->h) {
        preferred_h = layout->y + layout->h - layout->cursor_y;
    }
    rect.x = (int32_t)layout->cursor_x;
    rect.y = (int32_t)layout->cursor_y;
    rect.w = preferred_w;
    rect.h = preferred_h;
    layout->cursor_x += preferred_w + layout->gap;
    if (preferred_h > layout->row_h) layout->row_h = preferred_h;
    return rect;
}

uint32_t kenux_ui_anim_progress(uint64_t now, uint64_t start,
                                uint64_t duration_ms) {
    if (duration_ms == 0) return 1000;
    if (now <= start) return 0;
    uint64_t elapsed = now - start;
    if (elapsed >= duration_ms) return 1000;
    return (uint32_t)((elapsed * 1000u) / duration_ms);
}

uint32_t kenux_ui_anim_ease_out(uint32_t progress) {
    if (progress > 1000) progress = 1000;
    return (progress * (2000 - progress)) / 1000;
}

uint32_t kenux_ui_anim_lerp(uint32_t from, uint32_t to, uint32_t progress) {
    if (progress > 1000) progress = 1000;
    if (to >= from) return from + ((to - from) * progress) / 1000;
    return from - ((from - to) * progress) / 1000;
}

void kenux_ui_md3_button(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, const char* label,
                         uint32_t flags)
{
    uint32_t radius = 20;
    uint32_t accent = kenux_ui_color(KENUX_UI_COLOR_ACCENT);
    uint32_t text_color = RGB(0xFF, 0xFF, 0xFF);
    uint32_t bg = accent;

    if (flags & KENUX_UI_BUTTON_DISABLED) {
        bg = kenux_ui_color(KENUX_UI_COLOR_MUTED);
        text_color = kenux_ui_color(KENUX_UI_COLOR_SUBTLE);
    } else if (flags & KENUX_UI_BUTTON_PRESSED) {
        bg = color_darken(accent, 30);
    } else if (flags & KENUX_UI_BUTTON_ACTIVE) {
        bg = color_lighten(accent, 20);
    }

    if (surface && surface->pixels) {
        fb_fill_rounded_rect(x, y, w, h, radius, bg);
        if (!(flags & KENUX_UI_BUTTON_DISABLED)) {
            fb_blend_rect(x + 4, y + 2, w - 8, h / 2, RGB(255, 255, 255), 15);
        }
    }

    if (label) {
        uint32_t tw = font_text_width(label);
        uint32_t tx = x + (w > tw ? (w - tw) / 2 : 0);
        uint32_t ty = y + (h > 16 ? (h - 16) / 2 : 0);
        kenux_ui_text_transparent(surface, tx, ty, label, text_color);
    }
}

void kenux_ui_md3_card(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t elevation)
{
    uint32_t surface_color = kenux_ui_color(KENUX_UI_COLOR_SURFACE);
    uint32_t radius = 12;

    if (!surface || !surface->pixels) return;

    if (elevation > 0) {
        uint32_t shadow_alpha = 20 + elevation * 8;
        if (shadow_alpha > 80) shadow_alpha = 80;
        fb_fill_rounded_rect(x + elevation, y + elevation, w, h, radius,
                              RGB(0, 0, 0));
        fb_blend_rect(x + elevation, y + elevation, w, h,
                      RGB(0, 0, 0), shadow_alpha);
    }

    fb_fill_rounded_rect(x, y, w, h, radius, surface_color);
    fb_blend_rect(x + 4, y + 2, w - 8, h / 3, RGB(255, 255, 255), 6);
}

void kenux_ui_md3_chip(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       const char* label, int selected, uint32_t flags)
{
    uint32_t radius = 8;
    uint32_t h = 32;
    uint32_t bg, text_color;

    if (selected) {
        bg = kenux_ui_color(KENUX_UI_COLOR_ACCENT);
        text_color = RGB(0xFF, 0xFF, 0xFF);
    } else {
        bg = kenux_ui_color(KENUX_UI_COLOR_SURFACE);
        text_color = kenux_ui_color(KENUX_UI_COLOR_TEXT);
    }

    if (flags & KENUX_UI_BUTTON_DISABLED) {
        bg = kenux_ui_color(KENUX_UI_COLOR_MUTED);
        text_color = kenux_ui_color(KENUX_UI_COLOR_SUBTLE);
    }

    uint32_t w = 12;
    if (label) w += font_text_width(label);
    w += 12;
    if (w < 48) w = 48;

    if (surface && surface->pixels) {
        fb_fill_rounded_rect(x, y, w, h, radius, bg);
        uint32_t border = kenux_ui_color(KENUX_UI_COLOR_BORDER);
        fb_draw_rounded_rect_outline(x, y, w, h, radius, border);
    }

    if (label) {
        kenux_ui_text_transparent(surface, x + 12, y + (h > 16 ? (h - 16) / 2 : 0),
                                  label, text_color);
    }
}

void kenux_ui_md3_fab(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                      uint32_t size, const char* icon_label)
{
    uint32_t accent = kenux_ui_color(KENUX_UI_COLOR_ACCENT);
    uint32_t radius = size / 2;

    if (!surface || !surface->pixels) return;

    fb_fill_rounded_rect(x + 3, y + 3, size, size, radius, RGB(0, 0, 0));
    fb_blend_rect(x + 3, y + 3, size, size, RGB(0, 0, 0), 40);

    fb_fill_rounded_rect(x, y, size, size, radius, accent);
    fb_blend_rect(x + 4, y + 2, size - 8, size / 2, RGB(255, 255, 255), 20);

    if (icon_label) {
        uint32_t tw = font_text_width(icon_label);
        uint32_t tx = x + (size > tw ? (size - tw) / 2 : 0);
        uint32_t ty = y + (size > 16 ? (size - 16) / 2 : 0);
        kenux_ui_text_transparent(surface, tx, ty, icon_label, RGB(0xFF, 0xFF, 0xFF));
    }
}

void kenux_ui_md3_snackbar(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                           uint32_t w, const char* message)
{
    uint32_t h = 48;
    uint32_t radius = 8;
    uint32_t bg = RGB(0x31, 0x30, 0x33);
    uint32_t text_color = RGB(0xE6, 0xE1, 0xE6);

    if (!surface || !surface->pixels) return;

    fb_fill_rounded_rect(x, y, w, h, radius, bg);

    if (message) {
        kenux_ui_text_transparent(surface, x + 16, y + (h > 16 ? (h - 16) / 2 : 0),
                                  message, text_color);
    }
}

void kenux_ui_md3_divider(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                          uint32_t w)
{
    uint32_t color = kenux_ui_color(KENUX_UI_COLOR_BORDER);
    if (surface && surface->pixels) {
        for (uint32_t i = 0; i < w; i++) {
            kenux_ui_pixel(surface, x + i, y, color);
        }
    }
}

void kenux_ui_md3_switch(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         int on, uint32_t flags)
{
    uint32_t track_w = 52;
    uint32_t track_h = 32;
    uint32_t thumb_size = 24;
    uint32_t radius = track_h / 2;

    if (!surface || !surface->pixels) return;

    uint32_t track_color, thumb_color;
    uint32_t thumb_x;

    if (on) {
        track_color = kenux_ui_color(KENUX_UI_COLOR_ACCENT);
        thumb_color = RGB(0xFF, 0xFF, 0xFF);
        thumb_x = x + track_w - thumb_size - 4;
    } else {
        track_color = kenux_ui_color(KENUX_UI_COLOR_MUTED);
        thumb_color = kenux_ui_color(KENUX_UI_COLOR_BORDER);
        thumb_x = x + 4;
    }

    if (flags & KENUX_UI_BUTTON_DISABLED) {
        track_color = kenux_ui_color(KENUX_UI_COLOR_SUBTLE);
        thumb_color = kenux_ui_color(KENUX_UI_COLOR_MUTED);
    }

    fb_fill_rounded_rect(x, y, track_w, track_h, radius, track_color);

    uint32_t thumb_y = y + (track_h - thumb_size) / 2;
    fb_fill_rounded_rect(thumb_x, thumb_y, thumb_size, thumb_size,
                          thumb_size / 2, thumb_color);
    if (on && !(flags & KENUX_UI_BUTTON_DISABLED)) {
        fb_blend_rect(thumb_x + 2, thumb_y + 1, thumb_size - 4, thumb_size / 2,
                      RGB(255, 255, 255), 15);
    }
}