#ifndef FONT_H
#define FONT_H

#include "types.h"

#define FONT_WIDTH    8
#define FONT_HEIGHT   16

extern const uint8_t font_8x16[256][16];

void font_draw_char(uint32_t x, uint32_t y, char c, uint32_t color);
void font_draw_text(uint32_t x, uint32_t y, const char* str, uint32_t color);
void font_draw_text_bg(uint32_t x, uint32_t y, const char* str, uint32_t fg, uint32_t bg);
uint32_t font_text_width(const char* str);
uint32_t font_text_fit_bytes(const char* str, uint32_t max_width);

#endif
