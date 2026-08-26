#ifndef GUI_FRAMEBUFFER_H
#define GUI_FRAMEBUFFER_H

#include "types.h"
#include "color.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t* base;
    uint32_t size;
    uint8_t  bpp;
} gui_fb_t;

extern gui_fb_t fb;

/* Initialize GUI framebuffer from kernel fb_info_t */
void gui_fb_init(void);
uint32_t gui_scale_x(uint32_t v);
uint32_t gui_scale_y(uint32_t v);
uint32_t gui_scale_size(uint32_t v);
uint32_t gui_unscale_x(uint32_t v);
uint32_t gui_unscale_y(uint32_t v);

/* GUI internal drawing functions */
void gui_fb_set_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t gui_fb_get_pixel(uint32_t x, uint32_t y);
void gui_fb_clear(uint32_t color);
void gui_fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gui_fb_flush(void);
void gui_fb_flush_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
uint32_t fb_convert_color(uint32_t rgb_color);
void fb_test_pattern(void);

/* Blit a bitmap with alpha blending (0xAARRGGBB pixels) */
void fb_blit_alpha(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t* data);
/* Blit a bitmap scaled to target size with alpha */
void fb_blit_scaled(uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh,
                     const uint32_t* src, uint32_t sw, uint32_t sh);

/* Fill a rounded rectangle with given corner radius */
void fb_fill_rounded_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t color);
void fb_draw_rounded_rect_outline(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                  uint32_t radius, uint32_t color);
/* Fill a rounded rect with alpha-blended top corners only (for titlebars) */
void fb_fill_rounded_top_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                              uint32_t radius, uint32_t color);
/* Draw a soft shadow around a rectangular area */
void fb_draw_window_shadow(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                           uint32_t radius, uint32_t blur, uint8_t alpha);
/* Blend a solid color onto the framebuffer at given alpha */
void fb_blend_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   uint32_t color, uint8_t alpha);
void fb_blend_pixel(uint32_t x, uint32_t y, uint32_t color, uint8_t alpha);

/* Macro mapping: GUI code uses fb_set_pixel etc., actually calls gui_ versions */
#define fb_set_pixel    gui_fb_set_pixel
#define fb_get_pixel    gui_fb_get_pixel
#define fb_clear        gui_fb_clear
#define fb_fill_rect    gui_fb_fill_rect

/* ============================================================
 * Backward-compatible color constants
 * These now use the comprehensive color system from color.h
 * ============================================================ */

/* Legacy color aliases (map to new COL_ prefixed names) */
#define COLOR_BLACK      COL_BLACK
#define COLOR_WHITE      COL_WHITE
#define COLOR_RED        COL_RED
#define COLOR_GREEN      COL_GREEN
#define COLOR_BLUE       COL_BLUE
#define COLOR_YELLOW     COL_YELLOW
#define COLOR_CYAN       COL_CYAN
#define COLOR_MAGENTA    COL_MAGENTA
#define COLOR_GRAY       COL_GRAY
#define COLOR_LIGHTGRAY  COL_LIGHTGRAY
#define COLOR_DARKGRAY   COL_DARKGRAY
#define COLOR_BROWN      COL_BROWN
#define COLOR_ORANGE     COL_ORANGE
#define COLOR_PINK       COL_PINK
#define COLOR_PURPLE     COL_PURPLE

/* Legacy UI color aliases */
#define COLOR_UI_BG              RGB(200, 200, 200)
#define COLOR_UI_TITLEBAR        RGB(0,   0,   128)
#define COLOR_UI_TITLETEXT       RGB(255, 255, 255)
#define COLOR_UI_BORDER          RGB(128, 128, 128)
#define COLOR_UI_LIGHT           RGB(255, 255, 255)
#define COLOR_UI_DARK            RGB(128, 128, 128)
#define COLOR_UI_BUTTON          RGB(200, 200, 200)
#define COLOR_UI_BUTTON_PRESSED  RGB(150, 150, 150)

#endif