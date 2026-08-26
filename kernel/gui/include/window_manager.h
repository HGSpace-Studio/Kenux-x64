#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "types.h"
#include "window.h"
#include "widget.h"

typedef enum {
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_WINDOW_CLOSE,
    EVENT_PAINT
} event_type_t;

typedef struct {
    event_type_t type;
    uint32_t x;
    uint32_t y;
    uint8_t button;
    uint16_t key_code;
    uint16_t key_char;
    window_t* window;
    widget_t* widget;
} event_t;

typedef struct {
    window_t* windows;
    window_t* active_window;
    uint32_t mouse_x;
    uint32_t mouse_y;
    bool mouse_left_down;
    bool mouse_right_down;
    bool dragging;
    int32_t drag_offset_x;
    int32_t drag_offset_y;
    window_t* drag_window;
    uint32_t desktop_color;
    bool need_full_repaint;
    bool needs_flush;
} window_manager_t;

extern window_manager_t wm;

void wm_init(uint32_t desktop_color);
void wm_add_window(window_t* win);
void wm_remove_window(window_t* win);
void wm_set_active(window_t* win);
void wm_paint();
void wm_paint_fast(void);
void wm_paint_cursor_only(void);
void wm_invalidate_cursor_cache(void);
void wm_invalidate_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void wm_invalidate_window(window_t* win);
void wm_flush_dirty(void);
void wm_handle_mouse_move(uint32_t x, uint32_t y);
void wm_handle_mouse_down(uint8_t button, uint32_t x, uint32_t y);
void wm_handle_mouse_up(uint8_t button, uint32_t x, uint32_t y);
void wm_handle_key(uint16_t key_code, uint16_t key_char);
window_t* wm_get_window_at(uint32_t x, uint32_t y);
void wm_draw_mouse();

#endif
