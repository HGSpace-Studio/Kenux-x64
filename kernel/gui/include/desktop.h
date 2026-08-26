#ifndef DESKTOP_H
#define DESKTOP_H

#include "types.h"
#include "window.h"
#include "widget.h"
#include "icon.h"

/* === Left-side vertical taskbar (Ubuntu Dock style) === */
#define TASKBAR_MARGIN_LEFT    0    /* flush to left edge like Ubuntu */
#define TASKBAR_MARGIN_TOP     0
#define TASKBAR_MARGIN_BOTTOM  0
#define TASKBAR_WIDTH          56   /* slightly wider for icon padding */
#define TASKBAR_RADIUS         0    /* flat panel, no rounded corners */
#define TASKBAR_ICON_SIZE      32
#define TASKBAR_ICON_SPACING   8

/* Start button sits at top of taskbar */
#define START_BTN_SIZE         36   /* fits inside 48px taskbar with padding */

/* === Three-column start menu === */
#define START_MENU_WIDTH       480
#define START_MENU_HEIGHT      400
#define START_MENU_COL_APPS    160  /* left column: common apps */
#define START_MENU_COL_SYS     180  /* middle column: system apps */
#define START_MENU_COL_POWER   140  /* right column: power + user */

#define MAX_DESKTOP_ICONS  32
#define MAX_MENU_ITEMS     24   /* increased for three-column layout */
#define MAX_TASKBAR_BUTTONS 16

/* App dock entries (pinned + running) */
#define MAX_DOCK_APPS      12

typedef enum {
    CURSOR_DEFAULT = 0,
    CURSOR_TEXT,
    CURSOR_BUSY,
    CURSOR_LINK,
    CURSOR_HELP,
    CURSOR_MOVE,
    CURSOR_RESIZE_NS,
    CURSOR_RESIZE_EW,
    CURSOR_RESIZE_NESW,
    CURSOR_RESIZE_NWSE,
    CURSOR_PRECISION,
    CURSOR_UNAVAILABLE
} cursor_type_t;

typedef void (*menu_callback_t)(void);

typedef struct {
    char label[32];
    icon_id_t icon;
    menu_callback_t callback;
    bool separator;
} menu_item_t;

/* Context menu (right-click desktop) */
typedef struct {
    bool visible;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    menu_item_t items[MAX_MENU_ITEMS];
    uint32_t item_count;
    int32_t selected_index;
} menu_t;

/* Start menu item with column assignment */
typedef struct {
    char label[32];
    icon_id_t icon;
    menu_callback_t callback;
    uint8_t column;   /* 0=apps, 1=system, 2=power */
    bool separator;
} start_menu_item_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    char label[24];
    icon_id_t icon;
    menu_callback_t callback;
    bool selected;
} desktop_icon_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    char label[32];
    window_t* window;
    bool active;
} taskbar_button_t;

/* Dock app entry — pinned apps + running windows */
typedef struct {
    icon_id_t icon;
    char label[24];
    menu_callback_t callback;
    window_t* window;   /* NULL if pinned-only */
    bool running;
} dock_app_t;

typedef struct {
    desktop_icon_t icons[MAX_DESKTOP_ICONS];
    uint32_t icon_count;
    int32_t selected_icon;
    bool show_icons;           /* design: icons hidden by default */
    uint32_t wallpaper_color1;
    uint32_t wallpaper_color2;
    menu_t context_menu;
    start_menu_item_t start_menu_items[MAX_MENU_ITEMS];
    uint32_t start_menu_count;
    bool start_menu_open;
    int32_t start_menu_selected;
    taskbar_button_t taskbar_buttons[MAX_TASKBAR_BUTTONS];
    uint32_t taskbar_button_count;
    dock_app_t dock_apps[MAX_DOCK_APPS];
    uint32_t dock_count;
    cursor_type_t cursor_type;
    uint32_t clock_hour;
    uint32_t clock_minute;
    /* Taskbar geometry cache (computed at paint) */
    uint32_t tb_x, tb_y, tb_w, tb_h;
    /* Start menu geometry cache */
    uint32_t sm_x, sm_y, sm_w, sm_h;
    /* Desktop icon drag state */
    bool icon_dragging;
    int32_t icon_drag_idx;
    int32_t icon_drag_offset_x;
    int32_t icon_drag_offset_y;
    bool icon_drag_moved;
} desktop_t;

extern desktop_t desktop;

void desktop_init(void);
void desktop_paint(void);
void desktop_paint_no_wallpaper(void);
void desktop_paint_region(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void desktop_paint_overlays(void);
void desktop_add_icon(uint32_t x, uint32_t y, const char* label, icon_id_t icon);
void desktop_add_icon_with_callback(uint32_t x, uint32_t y, const char* label, icon_id_t icon, menu_callback_t callback);
void desktop_show_icons(bool show);
int32_t desktop_get_icon_at(uint32_t x, uint32_t y);
void desktop_handle_click(uint32_t x, uint32_t y, uint8_t button);
void desktop_handle_double_click(uint32_t x, uint32_t y);
bool desktop_handle_context_menu(uint32_t x, uint32_t y);

/* Desktop icon drag (Windows-style) */
bool desktop_icon_drag_start(uint32_t x, uint32_t y);
void desktop_icon_drag_move(uint32_t x, uint32_t y);
void desktop_icon_drag_end(void);

/* Left-side vertical capsule taskbar */
void taskbar_paint(void);
void taskbar_add_window(window_t* win);
void taskbar_remove_window(window_t* win);
void taskbar_update_active(window_t* win);
bool taskbar_handle_click(uint32_t x, uint32_t y);
bool taskbar_point_inside(uint32_t x, uint32_t y);
void taskbar_update_clock(uint32_t hour, uint32_t minute);
void dock_add_app(icon_id_t icon, const char* label, menu_callback_t cb);

/* Three-column start menu */
void start_menu_init(void);
void start_menu_paint(void);
bool start_menu_handle_click(uint32_t x, uint32_t y);
bool start_menu_point_inside(uint32_t x, uint32_t y);
void start_menu_add_item(const char* label, icon_id_t icon, menu_callback_t callback, uint8_t column);
void start_menu_toggle(void);

/* Context menu */
void context_menu_init(void);
void context_menu_show(uint32_t x, uint32_t y);
void context_menu_paint(void);
bool context_menu_handle_click(uint32_t x, uint32_t y);
bool context_menu_point_inside(uint32_t x, uint32_t y);
void context_menu_add_item(const char* label, icon_id_t icon, menu_callback_t callback, bool separator);

void cursor_set_type(cursor_type_t type);
void cursor_paint(uint32_t x, uint32_t y, cursor_type_t type);

#endif
