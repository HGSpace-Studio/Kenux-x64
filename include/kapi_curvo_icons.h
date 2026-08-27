#ifndef KAPI_CURVO_ICONS_H
#define KAPI_CURVO_ICONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CURVO_ICON_TOTAL           606
#define CURVO_ICON_SIZE_64         64
#define CURVO_ICON_SIZE_128        128
#define CURVO_ICON_SIZE_256        256

#define CURVO_CAT_UI              0
#define CURVO_CAT_MEDIA_TECH      1
#define CURVO_CAT_EDIT            2
#define CURVO_CAT_SHAPE_MATH      3
#define CURVO_CAT_EMOJI           4
#define CURVO_CAT_GAME            5
#define CURVO_CAT_OBJECT          6
#define CURVO_CAT_NATURE          7
#define CURVO_CAT_COUNT           8

typedef struct {
    uint32_t id;
    const char* name_cn;
    const char* name_en;
    uint32_t category;
    bool has_64;
    bool has_128;
    bool has_256;
    bool has_svg;
} curvo_icon_entry_t;

typedef struct {
    uint32_t id;
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    bool loaded;
} curvo_icon_bitmap_t;

typedef struct {
    curvo_icon_entry_t entries[CURVO_ICON_TOTAL];
    curvo_icon_bitmap_t cache[64];
    uint32_t cache_count;
    bool initialized;
    const char* base_path;
} curvo_icon_package_t;

int kapi_curvo_init(const char* icon_base_path);
void kapi_curvo_shutdown(void);

const curvo_icon_entry_t* kapi_curvo_lookup_by_name(const char* name_cn);
const curvo_icon_entry_t* kapi_curvo_lookup_by_id(uint32_t id);
const curvo_icon_entry_t* kapi_curvo_get_all(uint32_t* count);

const uint32_t* kapi_curvo_load_bitmap(uint32_t icon_id, uint32_t size, uint32_t* out_w, uint32_t* out_h);
void kapi_curvo_release_bitmap(uint32_t icon_id);

void kapi_curvo_draw_icon(uint32_t* fb, int stride, int fb_w, int fb_h,
                           int x, int y, uint32_t icon_id, uint32_t size,
                           uint32_t tint_color, uint8_t alpha);

void kapi_curvo_draw_icon_scaled(uint32_t* fb, int stride, int fb_w, int fb_h,
                                  int dst_x, int dst_y, int dst_w, int dst_h,
                                  uint32_t icon_id, uint32_t tint_color, uint8_t alpha);

uint32_t kapi_curvo_count_by_category(uint32_t category);
const curvo_icon_entry_t* kapi_curvo_get_by_category(uint32_t category, uint32_t* count);

const char* kapi_curvo_category_name(uint32_t category);

#ifdef __cplusplus
}
#endif

#endif