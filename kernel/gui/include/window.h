#ifndef WINDOW_H
#define WINDOW_H

#include "types.h"
#include "widget.h"

#define TITLEBAR_HEIGHT 44
#define BORDER_WIDTH 1
#define STATUSBAR_HEIGHT 30
#define WIN_CTRL_BTN_SIZE 22
#define WIN_CTRL_BTN_SPACING 10
#define WINDOW_CORNER_RADIUS 12
#define WINDOW_SHADOW_BLUR 12
#define WINDOW_SHADOW_ALPHA 42

typedef enum {
    WIN_CTRL_NONE = 0,
    WIN_CTRL_MINIMIZE,
    WIN_CTRL_MAXIMIZE,
    WIN_CTRL_CLOSE
} win_ctrl_hit_t;

typedef struct {
    bool minimized;
    bool maximized;
    uint32_t prev_x;
    uint32_t prev_y;
    uint32_t prev_w;
    uint32_t prev_h;
    bool has_statusbar;
    char statusbar_text[64];
    win_ctrl_hit_t hover_ctrl;
} window_state_t;

struct window_t;

typedef void (*content_click_cb_t)(struct window_t* win, uint32_t x, uint32_t y, void* user_data);

struct window_t {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    char* title;
    bool visible;
    bool has_border;
    bool has_titlebar;
    bool movable;
    bool resizable;
    bool closable;
    bool minimizable;
    bool maximizable;
    uint32_t bg_color;
    uint32_t titlebar_color;
    uint32_t title_text_color;
    widget_t* widgets;
    widget_t* focused_widget;
    window_t* next;
    window_state_t state;
    content_click_cb_t on_content_click;
    void* content_click_data;
};

window_t* window_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char* title);
void window_destroy(window_t* win);
void window_paint(window_t* win);
void window_paint_widgets(window_t* win);
void window_add_widget(window_t* win, widget_t* widget);
void window_remove_widget(window_t* win, widget_t* widget);
widget_t* window_get_widget_at(window_t* win, uint32_t x, uint32_t y);
bool window_point_inside(window_t* win, uint32_t px, uint32_t py);
bool window_titlebar_point_inside(window_t* win, uint32_t px, uint32_t py);
win_ctrl_hit_t window_hit_control(window_t* win, uint32_t px, uint32_t py);
void window_minimize(window_t* win);
void window_maximize(window_t* win);
void window_restore(window_t* win);
void window_close(window_t* win);
void window_set_statusbar(window_t* win, const char* text);
void window_get_content_rect(window_t* win, uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h);

#endif
