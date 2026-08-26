#include "kapi_cursor.h"
#include "kapi.h"
#include <string.h>

static struct {
    uint32_t current_type;
    int visible;
    int32_t pos_x, pos_y;
    int dragging;
    uint32_t drag_cursor;
    kapi_cursor_t custom[KAPI_CURSOR_MAX_CUSTOM];
    int custom_count;
    struct {
        int window_id;
        uint32_t cursor_type;
    } window_cursors[256];
    int window_cursor_count;
} cursor_state;

int kapi_cursor_init(void)
{
    memset(&cursor_state, 0, sizeof(cursor_state));
    cursor_state.current_type = KAPI_CURSOR_DEFAULT;
    cursor_state.visible = 1;
    return 0;
}

void kapi_cursor_cleanup(void)
{
    for (int i = 0; i < cursor_state.custom_count; i++) {
        kapi_cursor_t* c = &cursor_state.custom[i];
        if (c->is_custom && c->data.image.pixels) {
            kapi_kfree(c->data.image.pixels);
            c->data.image.pixels = NULL;
        }
    }
    cursor_state.custom_count = 0;
}

int kapi_cursor_set_system(uint32_t cursor_type)
{
    if (cursor_type > KAPI_CURSOR_HIDDEN && cursor_type != KAPI_CURSOR_HIDDEN) return -1;
    cursor_state.current_type = cursor_type;
    return 0;
}

int kapi_cursor_get_current(uint32_t* cursor_type)
{
    if (!cursor_type) return -1;
    *cursor_type = cursor_state.current_type;
    return 0;
}

int kapi_cursor_create_custom(const kapi_cursor_bitmap_t* bitmap)
{
    if (!bitmap || cursor_state.custom_count >= KAPI_CURSOR_MAX_CUSTOM) return -1;
    kapi_cursor_t* c = &cursor_state.custom[cursor_state.custom_count];
    c->id = 1000 + cursor_state.custom_count;
    c->type = 0;
    c->is_custom = 1;
    c->data.bitmap = *bitmap;
    c->ref_count = 1;
    cursor_state.custom_count++;
    return c->id;
}

int kapi_cursor_create_from_image(const kapi_cursor_image_t* image)
{
    if (!image || cursor_state.custom_count >= KAPI_CURSOR_MAX_CUSTOM) return -1;
    kapi_cursor_t* c = &cursor_state.custom[cursor_state.custom_count];
    c->id = 1000 + cursor_state.custom_count;
    c->type = 0;
    c->is_custom = 1;
    size_t sz = (size_t)image->width * image->height * 4;
    c->data.image.pixels = (uint32_t*)kapi_kmalloc(sz);
    if (!c->data.image.pixels) return -1;
    memcpy(c->data.image.pixels, image->pixels, sz);
    c->data.image.hotspot_x = image->hotspot_x;
    c->data.image.hotspot_y = image->hotspot_y;
    c->data.image.width = image->width;
    c->data.image.height = image->height;
    c->ref_count = 1;
    cursor_state.custom_count++;
    return c->id;
}

int kapi_cursor_create_from_data(const uint32_t* pixels, uint32_t w, uint32_t h,
                                 int32_t hotspot_x, int32_t hotspot_y)
{
    if (!pixels || cursor_state.custom_count >= KAPI_CURSOR_MAX_CUSTOM) return -1;
    kapi_cursor_image_t img;
    img.pixels = (uint32_t*)pixels;
    img.hotspot_x = hotspot_x;
    img.hotspot_y = hotspot_y;
    img.width = w;
    img.height = h;
    return kapi_cursor_create_from_image(&img);
}

int kapi_cursor_destroy_custom(uint32_t cursor_id)
{
    for (int i = 0; i < cursor_state.custom_count; i++) {
        if (cursor_state.custom[i].id == cursor_id) {
            kapi_cursor_t* c = &cursor_state.custom[i];
            if (c->is_custom && c->data.image.pixels) {
                kapi_kfree(c->data.image.pixels);
            }
            cursor_state.custom[i] = cursor_state.custom[--cursor_state.custom_count];
            return 0;
        }
    }
    return -1;
}

int kapi_cursor_set(uint32_t cursor_id)
{
    for (int i = 0; i < cursor_state.custom_count; i++) {
        if (cursor_state.custom[i].id == cursor_id) {
            cursor_state.current_type = cursor_id;
            return 0;
        }
    }
    return -1;
}

int kapi_cursor_reset(void)
{
    cursor_state.current_type = KAPI_CURSOR_DEFAULT;
    return 0;
}

int kapi_cursor_show(void) { cursor_state.visible = 1; return 0; }
int kapi_cursor_hide(void) { cursor_state.visible = 0; return 0; }
int kapi_cursor_is_visible(void) { return cursor_state.visible; }

int kapi_cursor_get_position(int32_t* x, int32_t* y)
{
    if (x) *x = cursor_state.pos_x;
    if (y) *y = cursor_state.pos_y;
    return 0;
}

int kapi_cursor_set_position(int32_t x, int32_t y)
{
    cursor_state.pos_x = x;
    cursor_state.pos_y = y;
    return 0;
}

int kapi_cursor_set_window_cursor(int window_id, uint32_t cursor_type)
{
    for (int i = 0; i < cursor_state.window_cursor_count; i++) {
        if (cursor_state.window_cursors[i].window_id == window_id) {
            cursor_state.window_cursors[i].cursor_type = cursor_type;
            return 0;
        }
    }
    if (cursor_state.window_cursor_count >= 256) return -1;
    int idx = cursor_state.window_cursor_count++;
    cursor_state.window_cursors[idx].window_id = window_id;
    cursor_state.window_cursors[idx].cursor_type = cursor_type;
    return 0;
}

int kapi_cursor_get_window_cursor(int window_id, uint32_t* cursor_type)
{
    for (int i = 0; i < cursor_state.window_cursor_count; i++) {
        if (cursor_state.window_cursors[i].window_id == window_id) {
            if (cursor_type) *cursor_type = cursor_state.window_cursors[i].cursor_type;
            return 0;
        }
    }
    if (cursor_type) *cursor_type = KAPI_CURSOR_DEFAULT;
    return 0;
}

int kapi_cursor_begin_drag(uint32_t cursor_id)
{
    cursor_state.dragging = 1;
    cursor_state.drag_cursor = cursor_id;
    cursor_state.current_type = cursor_id;
    return 0;
}

int kapi_cursor_end_drag(void)
{
    cursor_state.dragging = 0;
    cursor_state.current_type = KAPI_CURSOR_DEFAULT;
    return 0;
}