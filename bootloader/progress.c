#include "progress.h"

void init_progress_bar(progress_bar_t *pb, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    pb->x = x;
    pb->y = y;
    pb->width = width;
    pb->height = height;
    pb->progress = 0;
    pb->bg_r = 40;
    pb->bg_g = 40;
    pb->bg_b = 40;
    pb->bar_r = 0;
    pb->bar_g = 120;
    pb->bar_b = 215;
}

void set_progress(progress_bar_t *pb, uint32_t percent) {
    if (percent > 100) percent = 100;
    pb->progress = percent;
}

void draw_progress_bar(screen_t *screen, progress_bar_t *pb) {
    if (!screen || !pb) return;
    
    uint32_t radius = pb->height / 2;
    
    draw_rect(screen, pb->x + radius, pb->y, pb->width - radius * 2, pb->height, pb->bg_r, pb->bg_g, pb->bg_b);
    draw_rect(screen, pb->x, pb->y + radius, pb->width, pb->height - radius * 2, pb->bg_r, pb->bg_g, pb->bg_b);
    
    draw_filled_circle(screen, pb->x + radius, pb->y + radius, radius, pb->bg_r, pb->bg_g, pb->bg_b);
    draw_filled_circle(screen, pb->x + pb->width - radius, pb->y + radius, radius, pb->bg_r, pb->bg_g, pb->bg_b);
    
    uint32_t fill_width = (pb->width * pb->progress) / 100;
    if (fill_width > 0) {
        uint32_t fill_radius = pb->height / 2;
        
        if (fill_width <= fill_radius * 2) {
            draw_filled_circle(screen, pb->x + fill_radius, pb->y + fill_radius, fill_radius, pb->bar_r, pb->bar_g, pb->bar_b);
            draw_filled_circle(screen, pb->x + pb->width - fill_radius, pb->y + fill_radius, fill_radius, pb->bar_r, pb->bar_g, pb->bar_b);
        } else {
            draw_rect(screen, pb->x + fill_radius, pb->y, fill_width - fill_radius * 2, pb->height, pb->bar_r, pb->bar_g, pb->bar_b);
            draw_rect(screen, pb->x, pb->y + fill_radius, fill_width, pb->height - fill_radius * 2, pb->bar_r, pb->bar_g, pb->bar_b);
            
            draw_filled_circle(screen, pb->x + fill_radius, pb->y + fill_radius, fill_radius, pb->bar_r, pb->bar_g, pb->bar_b);
            draw_filled_circle(screen, pb->x + pb->width - fill_radius, pb->y + fill_radius, fill_radius, pb->bar_r, pb->bar_g, pb->bar_b);
        }
    }
    
    for (uint32_t i = 0; i < pb->width; i++) {
        draw_pixel(screen, pb->x + i, pb->y + 1, 80, 80, 80);
    }
    
    for (uint32_t i = 0; i < pb->width; i++) {
        draw_pixel(screen, pb->x + i, pb->y + pb->height - 2, 20, 20, 20);
    }
}