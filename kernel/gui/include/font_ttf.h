#ifndef FONT_TTF_H
#define FONT_TTF_H

#include "types.h"

#define KENUX_TTF_PIXEL_HEIGHT 24

bool font_ttf_init(void);
bool font_ttf_available(void);

bool font_ttf_draw_codepoint(uint32_t* x, uint32_t y, uint32_t code, uint32_t color);
bool font_ttf_draw_codepoint_bg(uint32_t* x, uint32_t y, uint32_t code,
                                uint32_t fg, uint32_t bg);
uint32_t font_ttf_codepoint_width(uint32_t code);

#endif
