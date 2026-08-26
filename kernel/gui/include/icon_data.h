#ifndef ICON_DATA_H
#define ICON_DATA_H

#include "types.h"

#define ICON_BITMAP_SIZE 32
#define ICON_BITMAP_PIXELS (32 * 32)

typedef struct {
    const uint32_t* data;
    uint32_t width;
    uint32_t height;
} icon_bitmap_t;

const icon_bitmap_t* icon_get_bitmap(int icon_id);

#endif
