#include "widget.h"
#include "window.h"
#include "font.h"
#include "msf.h"
#include "kenux_ui.h"

#define MAX_WIDGETS 512
#define MAX_STRINGS 8192
#define MAX_DATA 256

static widget_t widget_pool[MAX_WIDGETS];
static uint32_t widget_pool_idx = 0;

static char string_pool[MAX_STRINGS];
static uint32_t string_pool_idx = 0;

static checkbox_data_t checkbox_pool[MAX_DATA];
static uint32_t checkbox_pool_idx = 0;

static progressbar_data_t progressbar_pool[MAX_DATA];
static uint32_t progressbar_pool_idx = 0;

static uint32_t strlen8(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static char* strdup8(const char* s) {
    uint32_t len = strlen8(s);
    if (string_pool_idx + len + 1 > MAX_STRINGS) return NULL;
    char* buf = &string_pool[string_pool_idx];
    for (uint32_t i = 0; i <= len; i++) {
        buf[i] = s[i];
    }
    string_pool_idx += len + 1;
    return buf;
}

widget_t* widget_create_button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                                const char* text, widget_callback_t on_click, void* user_data) {
    if (widget_pool_idx >= MAX_WIDGETS) return NULL;
    widget_t* wgt = &widget_pool[widget_pool_idx++];
    wgt->type = WIDGET_BUTTON;
    wgt->x = gui_scale_x(x);
    wgt->y = gui_scale_y(y);
    wgt->width = gui_scale_x(w);
    wgt->height = gui_scale_y(h);
    wgt->visible = true;
    wgt->enabled = true;
    wgt->focused = false;
    wgt->text = strdup8(text);
    wgt->fg_color = msf_settings.button_text;
    wgt->bg_color = msf_settings.button_bg;
    wgt->on_click = on_click;
    wgt->on_focus = NULL;
    wgt->user_data = user_data;
    wgt->next = NULL;
    return wgt;
}

widget_t* widget_create_label(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                               const char* text, uint32_t color) {
    if (widget_pool_idx >= MAX_WIDGETS) return NULL;
    widget_t* wgt = &widget_pool[widget_pool_idx++];
    wgt->type = WIDGET_LABEL;
    wgt->x = gui_scale_x(x);
    wgt->y = gui_scale_y(y);
    wgt->width = gui_scale_x(w);
    wgt->height = gui_scale_y(h);
    wgt->visible = true;
    wgt->enabled = true;
    wgt->focused = false;
    wgt->text = strdup8(text);
    wgt->fg_color = color;
    wgt->bg_color = 0;
    wgt->on_click = NULL;
    wgt->on_focus = NULL;
    wgt->user_data = NULL;
    wgt->next = NULL;
    return wgt;
}

widget_t* widget_create_textbox(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                                 const char* text) {
    if (widget_pool_idx >= MAX_WIDGETS) return NULL;
    widget_t* wgt = &widget_pool[widget_pool_idx++];
    wgt->type = WIDGET_TEXTBOX;
    wgt->x = gui_scale_x(x);
    wgt->y = gui_scale_y(y);
    wgt->width = gui_scale_x(w);
    wgt->height = gui_scale_y(h);
    wgt->visible = true;
    wgt->enabled = true;
    wgt->focused = false;
    wgt->text = strdup8(text);
    wgt->fg_color = msf_settings.font_color;
    wgt->bg_color = COLOR_WHITE;
    wgt->on_click = NULL;
    wgt->on_focus = NULL;
    wgt->user_data = NULL;
    wgt->next = NULL;
    return wgt;
}

widget_t* widget_create_checkbox(uint32_t x, uint32_t y, const char* text, bool checked) {
    if (widget_pool_idx >= MAX_WIDGETS) return NULL;
    widget_t* wgt = &widget_pool[widget_pool_idx++];
    wgt->type = WIDGET_CHECKBOX;
    wgt->x = gui_scale_x(x);
    wgt->y = gui_scale_y(y);
    wgt->width = gui_scale_x(FONT_WIDTH * strlen8(text) + 20);
    wgt->height = gui_scale_y(FONT_HEIGHT + 4);
    wgt->visible = true;
    wgt->enabled = true;
    wgt->focused = false;
    wgt->text = strdup8(text);
    wgt->fg_color = msf_settings.font_color;
    wgt->bg_color = COLOR_WHITE;
    wgt->on_click = NULL;
    wgt->on_focus = NULL;
    
    if (checkbox_pool_idx >= MAX_DATA) return NULL;
    checkbox_data_t* data = &checkbox_pool[checkbox_pool_idx++];
    data->checked = checked;
    wgt->user_data = data;
    
    wgt->next = NULL;
    return wgt;
}

widget_t* widget_create_progressbar(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                     uint32_t min, uint32_t max, uint32_t value) {
    if (widget_pool_idx >= MAX_WIDGETS) return NULL;
    widget_t* wgt = &widget_pool[widget_pool_idx++];
    wgt->type = WIDGET_PROGRESSBAR;
    wgt->x = gui_scale_x(x);
    wgt->y = gui_scale_y(y);
    wgt->width = gui_scale_x(w);
    wgt->height = gui_scale_y(h);
    wgt->visible = true;
    wgt->enabled = true;
    wgt->focused = false;
    wgt->text = NULL;
    wgt->fg_color = msf_settings.accent_color;
    wgt->bg_color = COLOR_WHITE;
    wgt->on_click = NULL;
    wgt->on_focus = NULL;
    
    if (progressbar_pool_idx >= MAX_DATA) return NULL;
    progressbar_data_t* data = &progressbar_pool[progressbar_pool_idx++];
    data->min = min;
    data->max = max;
    data->value = value;
    wgt->user_data = data;
    
    wgt->next = NULL;
    return wgt;
}

void widget_destroy(widget_t* widget) {
}

static void paint_button(widget_t* wgt, uint32_t win_x, uint32_t win_y) {
    uint32_t px = win_x + wgt->x;
    uint32_t py = win_y + wgt->y;
    kenux_ui_surface_t surface;
    uint32_t flags = 0;
    if (!wgt->enabled) flags |= KENUX_UI_BUTTON_DISABLED;
    if (wgt->focused) flags |= KENUX_UI_BUTTON_ACTIVE;
    kenux_ui_bind_framebuffer(&surface);
    kenux_ui_button(&surface, px, py, wgt->width, wgt->height, wgt->text, flags);
}

static void paint_label(widget_t* wgt, uint32_t win_x, uint32_t win_y) {
    uint32_t px = win_x + wgt->x;
    uint32_t py = win_y + wgt->y;
    
    if (wgt->text) {
        font_draw_text(px, py, wgt->text, wgt->fg_color);
    }
}

static void paint_textbox(widget_t* wgt, uint32_t win_x, uint32_t win_y) {
    uint32_t px = win_x + wgt->x;
    uint32_t py = win_y + wgt->y;
    kenux_ui_surface_t surface;
    kenux_ui_bind_framebuffer(&surface);
    kenux_ui_inset(&surface, px, py, wgt->width, wgt->height,
                   kenux_ui_color(KENUX_UI_COLOR_CONTENT));
    if (wgt->focused) {
        kenux_ui_rect(&surface, px, py, wgt->width, 1, kenux_ui_color(KENUX_UI_COLOR_ACCENT));
        kenux_ui_rect(&surface, px, py, 1, wgt->height, kenux_ui_color(KENUX_UI_COLOR_ACCENT));
        kenux_ui_rect(&surface, px + wgt->width - 1, py, 1, wgt->height, kenux_ui_color(KENUX_UI_COLOR_ACCENT));
        kenux_ui_rect(&surface, px, py + wgt->height - 1, wgt->width, 1, kenux_ui_color(KENUX_UI_COLOR_ACCENT));
    }
    if (wgt->text) {
        kenux_ui_text_transparent_clipped(&surface, px + 6,
                                          py + (wgt->height > KENUX_UI_FONT_H ? (wgt->height - KENUX_UI_FONT_H) / 2 : 2),
                                          wgt->width > 12 ? wgt->width - 12 : wgt->width,
                                          wgt->text,
                                          kenux_ui_color(KENUX_UI_COLOR_TEXT));
    }
    if (wgt->focused) {
        uint32_t text_w = wgt->text ? font_text_width(wgt->text) : 0;
        uint32_t cursor_x = px + 6 + text_w;
        uint32_t cursor_y = py + (wgt->height > KENUX_UI_FONT_H ? (wgt->height - KENUX_UI_FONT_H) / 2 : 2);
        kenux_ui_rect(&surface, cursor_x, cursor_y, 2, KENUX_UI_FONT_H,
                      kenux_ui_color(KENUX_UI_COLOR_ACCENT));
    }
}

static void paint_checkbox(widget_t* wgt, uint32_t win_x, uint32_t win_y) {
    uint32_t px = win_x + wgt->x;
    uint32_t py = win_y + wgt->y;
    checkbox_data_t* data = (checkbox_data_t*)wgt->user_data;
    kenux_ui_surface_t surface;
    uint32_t flags = wgt->enabled ? 0 : KENUX_UI_BUTTON_DISABLED;
    kenux_ui_bind_framebuffer(&surface);
    kenux_ui_checkbox(&surface, px, py, wgt->text, data ? data->checked : 0, flags);
}

static void paint_progressbar(widget_t* wgt, uint32_t win_x, uint32_t win_y) {
    uint32_t px = win_x + wgt->x;
    uint32_t py = win_y + wgt->y;
    progressbar_data_t* data = (progressbar_data_t*)wgt->user_data;
    kenux_ui_surface_t surface;
    uint32_t value = 0;
    uint32_t max = 100;
    if (data) {
        max = data->max > data->min ? data->max - data->min : 1;
        value = data->value > data->min ? data->value - data->min : 0;
    }
    kenux_ui_bind_framebuffer(&surface);
    kenux_ui_progress(&surface, px, py, wgt->width, wgt->height, value, max);
}

void widget_paint(widget_t* wgt, window_t* window) {
    if (!wgt->visible) return;
    
    uint32_t content_x = window->x + BORDER_WIDTH;
    uint32_t content_y = window->y + BORDER_WIDTH + TITLEBAR_HEIGHT;
    
    switch (wgt->type) {
        case WIDGET_BUTTON:
            paint_button(wgt, content_x, content_y);
            break;
        case WIDGET_LABEL:
            paint_label(wgt, content_x, content_y);
            break;
        case WIDGET_TEXTBOX:
            paint_textbox(wgt, content_x, content_y);
            break;
        case WIDGET_CHECKBOX:
            paint_checkbox(wgt, content_x, content_y);
            break;
        case WIDGET_PROGRESSBAR:
            paint_progressbar(wgt, content_x, content_y);
            break;
    }
}

bool widget_point_inside(widget_t* wgt, uint32_t px, uint32_t py) {
    return (px >= wgt->x && px < wgt->x + wgt->width &&
            py >= wgt->y && py < wgt->y + wgt->height);
}

void widget_set_text(widget_t* wgt, const char* text) {
    wgt->text = strdup8(text);
}

void widget_set_progress(widget_t* wgt, uint32_t value) {
    if (wgt->type == WIDGET_PROGRESSBAR) {
        progressbar_data_t* data = (progressbar_data_t*)wgt->user_data;
        data->value = value;
    }
}
