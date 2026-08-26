#ifndef EMOJI_FONT_HEADER_H
#define EMOJI_FONT_HEADER_H

#include "types.h"

#define EMOJI_FONT_W 16
#define EMOJI_FONT_H 16
#define EMOJI_FONT_COUNT 149

extern const uint8_t emoji_font[149][32];
extern const uint32_t emoji_code_table[149];

int32_t emoji_font_lookup(uint32_t code);
void emoji_font_draw_char(uint32_t x, uint32_t y, uint32_t code, uint32_t color);

#endif
