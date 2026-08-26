#ifndef WIDGET_H
#define WIDGET_H

#include "types.h"

typedef struct widget_t widget_t;
typedef struct window_t window_t;

typedef enum {
    WIDGET_BUTTON,
    WIDGET_LABEL,
    WIDGET_TEXTBOX,
    WIDGET_CHECKBOX,
    WIDGET_PROGRESSBAR
} widget_type_t;

typedef void (*widget_callback_t)(widget_t* widget, void* user_data);

struct widget_t {
    widget_type_t type;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    bool visible;
    bool enabled;
    bool focused;
    char* text;
    uint32_t fg_color;
    uint32_t bg_color;
    widget_callback_t on_click;
    widget_callback_t on_focus;
    void* user_data;
    widget_t* next;
};

typedef struct {
    uint32_t min;
    uint32_t max;
    uint32_t value;
} progressbar_data_t;

typedef struct {
    bool checked;
} checkbox_data_t;

widget_t* widget_create_button(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                                const char* text, widget_callback_t on_click, void* user_data);
widget_t* widget_create_label(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                               const char* text, uint32_t color);
widget_t* widget_create_textbox(uint32_t x, uint32_t y, uint32_t w, uint32_t h, 
                                 const char* text);
widget_t* widget_create_checkbox(uint32_t x, uint32_t y, const char* text, bool checked);
widget_t* widget_create_progressbar(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                     uint32_t min, uint32_t max, uint32_t value);
void widget_destroy(widget_t* widget);
void widget_paint(widget_t* widget, window_t* window);
bool widget_point_inside(widget_t* widget, uint32_t px, uint32_t py);
void widget_set_text(widget_t* widget, const char* text);
void widget_set_progress(widget_t* widget, uint32_t value);

#endif
