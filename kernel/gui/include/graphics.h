#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "types.h"

void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_draw_filled_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void gfx_draw_circle(int32_t xc, int32_t yc, int32_t r, uint32_t color);
void gfx_draw_filled_circle(int32_t xc, int32_t yc, int32_t r, uint32_t color);
void gfx_draw_hline(uint32_t x, uint32_t y, uint32_t w, uint32_t color);
void gfx_draw_vline(uint32_t x, uint32_t y, uint32_t h, uint32_t color);
void gfx_draw_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);

#endif
