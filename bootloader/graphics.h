#ifndef KEGD_GRAPHICS_H
#define KEGD_GRAPHICS_H

#include "efi.h"

typedef struct {
    efi_graphics_output_t *gop;
    uint32_t width;
    uint32_t height;
    uint64_t frame_buffer;
    uint64_t buffer_size;
    uint32_t pixels_per_scanline;
    int pixel_format;
} screen_t;

efi_status_t init_graphics(efi_system_table_t *st, screen_t *screen);

void draw_pixel(screen_t *screen, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);

void draw_rect(screen_t *screen, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b);

void draw_circle(screen_t *screen, uint32_t cx, uint32_t cy, uint32_t radius, uint8_t r, uint8_t g, uint8_t b);

void draw_filled_circle(screen_t *screen, uint32_t cx, uint32_t cy, uint32_t radius, uint8_t r, uint8_t g, uint8_t b);

void *memcpy(void *dest, const void *src, uint64_t n);

void *memset(void *s, uint32_t c, uint64_t n);

void draw_char(screen_t *screen, uint32_t x, uint32_t y, char c, uint8_t r, uint8_t g, uint8_t b);

void draw_string(screen_t *screen, uint32_t x, uint32_t y, const char *str, uint8_t r, uint8_t g, uint8_t b);

#endif