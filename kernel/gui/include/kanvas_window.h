#ifndef KANVAS_WINDOW_H
#define KANVAS_WINDOW_H

#include "kapi_kanvasui.h"
#include "kapi_window.h"
#include "kapi_graphics2d.h"

#define KANVAS_WIN_TITLEBAR_H     36
#define KANVAS_WIN_BORDER_W       1
#define KANVAS_WIN_RADIUS         12
#define KANVAS_WIN_SHADOW_BLUR    20
#define KANVAS_WIN_SHADOW_ALPHA   40
#define KANVAS_WIN_CTRL_SIZE      14
#define KANVAS_WIN_CTRL_SPACING   8
#define KANVAS_WIN_MIN_W          200
#define KANVAS_WIN_MIN_H          120
#define KANVAS_WIN_RESIZE_MARGIN  6

typedef enum {
    KANVAS_WIN_CTRL_NONE = 0,
    KANVAS_WIN_CTRL_MINIMIZE,
    KANVAS_WIN_CTRL_MAXIMIZE,
    KANVAS_WIN_CTRL_CLOSE
} kanvas_win_ctrl_t;

typedef enum {
    KANVAS_RESIZE_NONE = 0,
    KANVAS_RESIZE_N,
    KANVAS_RESIZE_S,
    KANVAS_RESIZE_E,
    KANVAS_RESIZE_W,
    KANVAS_RESIZE_NE,
    KANVAS_RESIZE_NW,
    KANVAS_RESIZE_SE,
    KANVAS_RESIZE_SW
} kanvas_resize_edge_t;

typedef struct kanvas_window kanvas_window_t;

typedef void (*kanvas_win_paint_fn)(kanvas_window_t* win, uint32_t* fb, int stride, int w, int h);
typedef void (*kanvas_win_event_fn)(kanvas_window_t* win, kui_msg_type_t msg, uint64_t p1, uint64_t p2);

struct kanvas_window {
    uint32_t id;
    char title[256];
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
    bool visible;
    bool focused;
    bool minimized;
    bool maximized;
    bool resizable;
    bool decorated;
    bool modal;
    bool topmost;
    bool closing;
    int prev_x, prev_y, prev_w, prev_h;
    kui_color_t titlebar_bg;
    kui_color_t titlebar_fg;
    kui_color_t client_bg;
    kui_color_t border_color;
    kanvas_win_ctrl_t hover_ctrl;
    kanvas_resize_edge_t resize_edge;
    kui_component_t* components_head;
    int component_count;
    kanvas_win_paint_fn on_paint;
    kanvas_win_event_fn on_event;
    void* user_data;
    uint32_t* back_buffer;
    uint32_t* front_buffer;
    int buffer_stride;
    kui_rect_t dirty_rects[16];
    int dirty_count;
    kanvas_window_t* next;
    kanvas_window_t* prev;
    kanvas_window_t* parent;
};

kanvas_window_t* kanvas_window_create(const char* title, int x, int y, int w, int h,
                                       bool resizable, bool decorated);
void kanvas_window_destroy(kanvas_window_t* win);
void kanvas_window_paint(kanvas_window_t* win, uint32_t* fb, int stride, int screen_w, int screen_h);
void kanvas_window_paint_titlebar(kanvas_window_t* win, uint32_t* fb, int stride, int fw, int fh);
void kanvas_window_paint_shadow(kanvas_window_t* win, uint32_t* fb, int stride, int fw, int fh);
void kanvas_window_paint_border(kanvas_window_t* win, uint32_t* fb, int stride, int fw, int fh);

bool kanvas_window_hit_test(kanvas_window_t* win, int px, int py);
bool kanvas_window_titlebar_hit(kanvas_window_t* win, int px, int py);
kanvas_win_ctrl_t kanvas_window_hit_ctrl(kanvas_window_t* win, int px, int py);
kanvas_resize_edge_t kanvas_window_hit_resize(kanvas_window_t* win, int px, int py);

void kanvas_window_minimize(kanvas_window_t* win);
void kanvas_window_maximize(kanvas_window_t* win);
void kanvas_window_restore(kanvas_window_t* win);
void kanvas_window_close(kanvas_window_t* win);
void kanvas_window_set_title(kanvas_window_t* win, const char* title);
void kanvas_window_set_rect(kanvas_window_t* win, int x, int y, int w, int h);
void kanvas_window_get_content_rect(kanvas_window_t* win, int* x, int* y, int* w, int* h);

void kanvas_window_add_component(kanvas_window_t* win, kui_component_t* comp);
void kanvas_window_remove_component(kanvas_window_t* win, kui_component_t* comp);

void kanvas_window_invalidate(kanvas_window_t* win);
void kanvas_window_invalidate_rect(kanvas_window_t* win, int x, int y, int w, int h);

#endif