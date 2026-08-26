#ifndef KAPI_CURSOR_H
#define KAPI_CURSOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CURSOR_DEFAULT       0
#define KAPI_CURSOR_ARROW         1
#define KAPI_CURSOR_IBEAM         2
#define KAPI_CURSOR_WAIT          3
#define KAPI_CURSOR_CROSSHAIR     4
#define KAPI_CURSOR_SIZE_WE       5
#define KAPI_CURSOR_SIZE_NS       6
#define KAPI_CURSOR_SIZE_NWSE     7
#define KAPI_CURSOR_SIZE_NESW     8
#define KAPI_CURSOR_SIZE_ALL      9
#define KAPI_CURSOR_HAND         10
#define KAPI_CURSOR_NOT_ALLOWED  11
#define KAPI_CURSOR_GRAB         12
#define KAPI_CURSOR_GRABBING     13
#define KAPI_CURSOR_HELP         14
#define KAPI_CURSOR_PROGRESS     15
#define KAPI_CURSOR_CELL         16
#define KAPI_CURSOR_ZOOM_IN      17
#define KAPI_CURSOR_ZOOM_OUT     18
#define KAPI_CURSOR_HIDDEN       99

#define KAPI_CURSOR_MAX_CUSTOM   64
#define KAPI_CURSOR_BITMAP_SIZE  (32 * 32)

typedef struct kapi_cursor_bitmap {
    uint8_t and_mask[KAPI_CURSOR_BITMAP_SIZE / 8];
    uint8_t xor_mask[KAPI_CURSOR_BITMAP_SIZE / 8];
    int32_t hotspot_x;
    int32_t hotspot_y;
    uint32_t width;
    uint32_t height;
} kapi_cursor_bitmap_t;

typedef struct kapi_cursor_image {
    uint32_t* pixels;
    int32_t hotspot_x;
    int32_t hotspot_y;
    uint32_t width;
    uint32_t height;
} kapi_cursor_image_t;

typedef struct kapi_cursor {
    uint32_t id;
    uint32_t type;
    int is_custom;
    union {
        kapi_cursor_bitmap_t bitmap;
        kapi_cursor_image_t image;
    } data;
    int ref_count;
} kapi_cursor_t;

int kapi_cursor_init(void);
void kapi_cursor_cleanup(void);

int kapi_cursor_set_system(uint32_t cursor_type);
int kapi_cursor_get_current(uint32_t* cursor_type);

int kapi_cursor_create_custom(const kapi_cursor_bitmap_t* bitmap);
int kapi_cursor_create_from_image(const kapi_cursor_image_t* image);
int kapi_cursor_create_from_data(const uint32_t* pixels, uint32_t w, uint32_t h,
                                 int32_t hotspot_x, int32_t hotspot_y);
int kapi_cursor_destroy_custom(uint32_t cursor_id);

int kapi_cursor_set(uint32_t cursor_id);
int kapi_cursor_reset(void);

int kapi_cursor_show(void);
int kapi_cursor_hide(void);
int kapi_cursor_is_visible(void);

int kapi_cursor_get_position(int32_t* x, int32_t* y);
int kapi_cursor_set_position(int32_t x, int32_t y);

int kapi_cursor_set_window_cursor(int window_id, uint32_t cursor_id);
int kapi_cursor_get_window_cursor(int window_id, uint32_t* cursor_id);

int kapi_cursor_begin_drag(uint32_t cursor_id);
int kapi_cursor_end_drag(void);

#ifdef __cplusplus
}
#endif

#endif