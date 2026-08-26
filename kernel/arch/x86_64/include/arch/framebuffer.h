#ifndef ARCH_X86_64_FRAMEBUFFER_H
#define ARCH_X86_64_FRAMEBUFFER_H

#include <arch/types.h>
#include <arch/boot.h>

#define GOP_INFO_PROTOCOL_GUID { \
    0x9042A9DE, 0x2394, 0x45C7, \
    {0xA2, 0xDA, 0xDE, 0x44, 0xE3, 0x41, 0x6D, 0xF7} \
}

#define FB_PIXEL_FORMAT_ARGB    0
#define FB_PIXEL_FORMAT_BGRA    1
#define FB_PIXEL_FORMAT_RGB24   2
#define FB_PIXEL_FORMAT_RGB16   3
#define FB_PIXEL_FORMAT_UNKNOWN 4

typedef struct {
    uint64_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pixel_format;
    uint32_t bytes_per_line;
    uint64_t total_size;
    uint8_t* addr;
} __attribute__((packed)) fb_info_t;

typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} __attribute__((packed)) fb_color_t;

#define FB_COLOR(r, g, b, a)  ((uint32_t)(a) << 24 | (uint32_t)(r) << 16 | \
                               (uint32_t)(g) << 8 | (uint32_t)(b))

#define FB_COLOR_BLACK        FB_COLOR(0, 0, 0, 255)
#define FB_COLOR_WHITE        FB_COLOR(255, 255, 255, 255)
#define FB_COLOR_RED          FB_COLOR(255, 0, 0, 255)
#define FB_COLOR_GREEN        FB_COLOR(0, 255, 0, 255)
#define FB_COLOR_BLUE         FB_COLOR(0, 0, 255, 255)
#define FB_COLOR_YELLOW       FB_COLOR(255, 255, 0, 255)
#define FB_COLOR_CYAN         FB_COLOR(0, 255, 255, 255)
#define FB_COLOR_MAGENTA      FB_COLOR(255, 0, 255, 255)
#define FB_COLOR_GRAY         FB_COLOR(128, 128, 128, 255)
#define FB_COLOR_LIGHT_GRAY   FB_COLOR(192, 192, 192, 255)

#define FB_TEXT_FG            FB_COLOR_WHITE
#define FB_TEXT_BG            FB_COLOR_BLACK

#define FB_CURSOR_WIDTH       8
#define FB_CURSOR_HEIGHT      2
#define FB_CURSOR_SCAN_LINE   14

#define FB_CHAR_WIDTH         8
#define FB_CHAR_HEIGHT        16

int fb_init_from_gop(struct FrameBufferConfig *fbc);

fb_info_t* fb_get_info(void);

void fb_set_pixel(uint32_t x, uint32_t y, uint32_t color);

uint32_t fb_get_pixel(uint32_t x, uint32_t y);

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

void fb_draw_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color);

void fb_scroll(uint32_t lines);

void fb_clear(uint32_t color);

void fb_set_cursor(uint32_t x, uint32_t y);

void fb_put_char(uint32_t x, uint32_t y, char ch, uint32_t fg, uint32_t bg);

void fb_puts(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg);

int fb_resolution_set(uint32_t width, uint32_t height);

typedef struct {
    void*    buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} framebuffer_t;

void framebuffer_init(struct FrameBufferConfig *fbc);
framebuffer_t* framebuffer_get(void);
void framebuffer_set_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_clear(uint32_t color);
void framebuffer_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

#endif
