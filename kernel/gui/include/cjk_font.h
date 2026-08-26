#ifndef CJK_FONT_HEADER_H
#define CJK_FONT_HEADER_H

#include "types.h"

#define CJK_FONT_W 16
#define CJK_FONT_H 16
#define CJK_FONT_COUNT 6763

extern const uint8_t cjk_font[6763][32];
extern const uint32_t cjk_code_table[6763];

/* Binary search for CJK char index. Returns -1 if not found. */
int32_t cjk_font_lookup(uint32_t code);
void cjk_font_draw_char(uint32_t x, uint32_t y, uint32_t code, uint32_t color);

#endif
