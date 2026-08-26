#ifndef KEGD_PROGRESS_H
#define KEGD_PROGRESS_H

#include "graphics.h"

typedef struct {
    uint32_t x, y;
    uint32_t width, height;
    uint32_t progress;
    uint8_t bg_r, bg_g, bg_b;
    uint8_t bar_r, bar_g, bar_b;
} progress_bar_t;

void init_progress_bar(progress_bar_t *pb, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void set_progress(progress_bar_t *pb, uint32_t percent);

void draw_progress_bar(screen_t *screen, progress_bar_t *pb);

#endif