#include "graphics.h"
#include "framebuffer.h"

void gfx_draw_hline(uint32_t x, uint32_t y, uint32_t w, uint32_t color) {
    fb_fill_rect(x, y, w, 1, color);
}

void gfx_draw_vline(uint32_t x, uint32_t y, uint32_t h, uint32_t color) {
    fb_fill_rect(x, y, 1, h, color);
}

void gfx_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    gfx_draw_hline(x, y, w, color);
    gfx_draw_hline(x, y + h - 1, w, color);
    gfx_draw_vline(x, y, h, color);
    gfx_draw_vline(x + w - 1, y, h, color);
}

void gfx_draw_filled_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    fb_fill_rect(x, y, w, h, color);
}

void gfx_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = x1 - x0;
    int32_t dy = y1 - y0;
    int32_t dx_abs = dx < 0 ? -dx : dx;
    int32_t dy_abs = dy < 0 ? -dy : dy;
    
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx_abs - dy_abs;
    int32_t e2;
    
    while (1) {
        fb_set_pixel((uint32_t)x0, (uint32_t)y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy_abs) { err -= dy_abs; x0 += sx; }
        if (e2 < dx_abs)  { err += dx_abs; y0 += sy; }
    }
}

void gfx_draw_circle(int32_t xc, int32_t yc, int32_t r, uint32_t color) {
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    
    while (x <= y) {
        fb_set_pixel(xc + x, yc + y, color);
        fb_set_pixel(xc - x, yc + y, color);
        fb_set_pixel(xc + x, yc - y, color);
        fb_set_pixel(xc - x, yc - y, color);
        fb_set_pixel(xc + y, yc + x, color);
        fb_set_pixel(xc - y, yc + x, color);
        fb_set_pixel(xc + y, yc - x, color);
        fb_set_pixel(xc - y, yc - x, color);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void gfx_draw_filled_circle(int32_t xc, int32_t yc, int32_t r, uint32_t color) {
    for (int32_t y = -r; y <= r; y++) {
        for (int32_t x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                fb_set_pixel(xc + x, yc + y, color);
            }
        }
    }
}

void gfx_draw_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
    gfx_draw_line(x0, y0, x1, y1, color);
    gfx_draw_line(x1, y1, x2, y2, color);
    gfx_draw_line(x2, y2, x0, y0, color);
}
