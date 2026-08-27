#ifndef KANVAS_TASKBAR_H
#define KANVAS_TASKBAR_H

#include "kapi_kanvasui.h"
#include "kapi_graphics2d.h"

#define KANVAS_TASKBAR_H_PX         48
#define KANVAS_TASKBAR_ICON_SZ      32
#define KANVAS_TASKBAR_PAD          8
#define KANVAS_TASKBAR_START_W      48
#define KANVAS_TASKBAR_ITEM_W       44
#define KANVAS_TASKBAR_ITEM_H       40
#define KANVAS_TASKBAR_MAX_ITEMS    24
#define KANVAS_TASKBAR_MAX_TRAY     16
#define KANVAS_TASKBAR_RADIUS       0
#define KANVAS_TASKBAR_POSITION_TOP 1
#define KANVAS_TASKBAR_BLUR_BEHIND  1

typedef struct {
    char name[64];
    int icon_id;
    bool pinned;
    bool running;
    bool active;
    bool hovered;
    void* app_data;
} kanvas_taskbar_item_t;

typedef struct {
    char name[32];
    int icon_id;
    bool has_notification;
    int notification_count;
    bool hovered;
    void (*on_click)(void);
} kanvas_tray_icon_t;

typedef struct {
    int x, y;
    int width;
    bool visible;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t accent_color;
    kui_color_t hover_color;
    kui_color_t active_color;
    kui_color_t start_bg;
    kui_color_t start_fg;
    bool start_hovered;
    bool start_pressed;
    kanvas_taskbar_item_t items[KANVAS_TASKBAR_MAX_ITEMS];
    int item_count;
    kanvas_tray_icon_t tray[KANVAS_TASKBAR_MAX_TRAY];
    int tray_count;
    int hovered_item;
    int hovered_tray;
    int clock_x;
    char clock_text[32];
    char date_text[32];
} kanvas_taskbar_t;

kanvas_taskbar_t* kanvas_taskbar_create(int x, int y, int width);
void kanvas_taskbar_destroy(kanvas_taskbar_t* tb);
void kanvas_taskbar_paint(kanvas_taskbar_t* tb, uint32_t* fb, int stride, int fw, int fh);
void kanvas_taskbar_handle_mouse(kanvas_taskbar_t* tb, int mx, int my, bool left_down, bool left_up);
void kanvas_taskbar_update_clock(kanvas_taskbar_t* tb, uint64_t now_ms);

int kanvas_taskbar_add_item(kanvas_taskbar_t* tb, const char* name, int icon_id, bool pinned, void* app_data);
void kanvas_taskbar_remove_item(kanvas_taskbar_t* tb, int index);
void kanvas_taskbar_set_item_running(kanvas_taskbar_t* tb, int index, bool running);
void kanvas_taskbar_set_item_active(kanvas_taskbar_t* tb, int index, bool active);

int kanvas_taskbar_add_tray(kanvas_taskbar_t* tb, const char* name, int icon_id, void (*on_click)(void));
void kanvas_taskbar_remove_tray(kanvas_taskbar_t* tb, int index);
void kanvas_taskbar_tray_notify(kanvas_taskbar_t* tb, int index, int count);

#endif