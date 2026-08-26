#ifndef KANVAS_DESKTOP_H
#define KANVAS_DESKTOP_H

#include "kapi_kanvasui.h"
#include "kapi_window.h"
#include "kapi_graphics2d.h"

#define KANVAS_DESKTOP_MAX_ICONS    48
#define KANVAS_DESKTOP_MAX_WINDOWS  64
#define KANVAS_DESKTOP_MAX_TASKBAR  24
#define KANVAS_DESKTOP_MAX_TRAY     16

#define KANVAS_TASKBAR_HEIGHT       48
#define KANVAS_TASKBAR_ICON_SIZE    32
#define KANVAS_TASKBAR_PADDING      8
#define KANVAS_TASKBAR_RADIUS       0

#define KANVAS_START_MENU_WIDTH     420
#define KANVAS_START_MENU_HEIGHT    520
#define KANVAS_START_MENU_COL1_W    180
#define KANVAS_START_MENU_COL2_W    140
#define KANVAS_START_MENU_COL3_W    100

#define KANVAS_TITLEBAR_HEIGHT      36
#define KANVAS_WINDOW_RADIUS        10
#define KANVAS_WINDOW_SHADOW_BLUR   16
#define KANVAS_WINDOW_SHADOW_ALPHA  50
#define KANVAS_BORDER_WIDTH         1

#define KANVAS_STATUSBAR_HEIGHT     28

typedef enum {
    KANVAS_DESKTOP_STATE_NORMAL = 0,
    KANVAS_DESKTOP_STATE_START_OPEN,
    KANVAS_DESKTOP_STATE_CONTEXT_MENU,
    KANVAS_DESKTOP_STATE_DRAGGING,
    KANVAS_DESKTOP_STATE_RESIZING
} kanvas_desktop_state_t;

typedef enum {
    KANVAS_APP_CATEGORY_SYSTEM = 0,
    KANVAS_APP_CATEGORY_UTILITY,
    KANVAS_APP_CATEGORY_MEDIA,
    KANVAS_APP_CATEGORY_OFFICE,
    KANVAS_APP_CATEGORY_DEV,
    KANVAS_APP_CATEGORY_GAME,
    KANVAS_APP_CATEGORY_NET,
    KANVAS_APP_CATEGORY_COUNT
} kanvas_app_category_t;

typedef struct {
    char name[64];
    char exec[128];
    char icon_path[128];
    int icon_id;
    kanvas_app_category_t category;
    bool pinned;
    bool running;
    kui_window_t* window;
} kanvas_app_entry_t;

typedef struct {
    char label[64];
    int icon_id;
    void (*callback)(void);
    bool separator;
    bool enabled;
} kanvas_menu_entry_t;

typedef struct {
    bool visible;
    int x, y;
    int width, height;
    kanvas_menu_entry_t items[32];
    int item_count;
    int hovered_index;
} kanvas_context_menu_t;

typedef struct {
    bool visible;
    int x, y;
    kanvas_app_entry_t* pinned[KANVAS_DESKTOP_MAX_TASKBAR];
    kanvas_app_entry_t* running[KANVAS_DESKTOP_MAX_TASKBAR];
    int pinned_count;
    int running_count;
    int hovered_index;
    bool start_hovered;
} kanvas_taskbar_t;

typedef struct {
    bool visible;
    int x, y;
    char search_text[128];
    int cursor_pos;
    kanvas_app_entry_t* results[32];
    int result_count;
    int selected_index;
    kanvas_app_entry_t* all_apps[KANVAS_DESKTOP_MAX_ICONS];
    int all_app_count;
} kanvas_start_menu_t;

typedef struct {
    char name[32];
    uint32_t icon_data[32*32];
    bool has_notification;
    void (*on_click)(void);
} kanvas_tray_item_t;

typedef struct {
    kanvas_tray_item_t items[KANVAS_DESKTOP_MAX_TRAY];
    int count;
    int hovered_index;
} kanvas_system_tray_t;

typedef struct {
    char name[64];
    int x, y;
    int icon_id;
    bool selected;
    bool dragging;
    int drag_offset_x, drag_offset_y;
} kanvas_desktop_icon_t;

typedef struct {
    kui_color_t desktop_bg;
    kui_color_t desktop_fg;
    kui_color_t taskbar_bg;
    kui_color_t taskbar_fg;
    kui_color_t taskbar_accent;
    kui_color_t titlebar_bg;
    kui_color_t titlebar_fg;
    kui_color_t titlebar_active_bg;
    kui_color_t titlebar_active_fg;
    kui_color_t window_bg;
    kui_color_t window_fg;
    kui_color_t start_menu_bg;
    kui_color_t start_menu_fg;
    kui_color_t start_menu_hover;
    kui_color_t context_menu_bg;
    kui_color_t context_menu_fg;
    kui_color_t context_menu_hover;
    kui_color_t statusbar_bg;
    kui_color_t statusbar_fg;
    kui_color_t accent;
    kui_color_t border;
    kui_color_t shadow;
    kui_color_t selection;
    kui_color_t danger;
    kui_color_t success;
    kui_color_t warning;
    int corner_radius;
    int font_size;
    int font_size_small;
    int font_size_large;
    char font_family[64];
} kanvas_desktop_theme_t;

typedef struct {
    kui_context_t* ui;
    kanvas_desktop_state_t state;
    kanvas_desktop_theme_t theme;
    kanvas_desktop_icon_t icons[KANVAS_DESKTOP_MAX_ICONS];
    int icon_count;
    kanvas_taskbar_t taskbar;
    kanvas_start_menu_t start_menu;
    kanvas_context_menu_t context_menu;
    kanvas_system_tray_t tray;
    kui_window_t* windows[KANVAS_DESKTOP_MAX_WINDOWS];
    int window_count;
    kui_window_t* active_window;
    kui_window_t* desktop_window;
    int screen_width;
    int screen_height;
    int mouse_x, mouse_y;
    bool mouse_left_down;
    bool mouse_right_down;
    int drag_window_idx;
    int drag_offset_x, drag_offset_y;
    int resize_edge;
    uint32_t wallpaper_color;
    uint32_t* wallpaper_data;
    int wallpaper_w, wallpaper_h;
    uint64_t last_tick;
    bool needs_repaint;
} kanvas_desktop_t;

kanvas_desktop_t* kanvas_desktop_create(int screen_w, int screen_h, uint32_t* fb, int stride);
void kanvas_desktop_destroy(kanvas_desktop_t* desk);
void kanvas_desktop_run(kanvas_desktop_t* desk);
void kanvas_desktop_paint(kanvas_desktop_t* desk);
void kanvas_desktop_handle_mouse(kanvas_desktop_t* desk, int x, int y, bool left, bool right, bool mid);
void kanvas_desktop_handle_key(kanvas_desktop_t* desk, int key, bool down, uint32_t mods);
void kanvas_desktop_handle_scroll(kanvas_desktop_t* desk, int delta);

void kanvas_desktop_add_icon(kanvas_desktop_t* desk, const char* name, int icon_id, int x, int y);
void kanvas_desktop_remove_icon(kanvas_desktop_t* desk, int index);

void kanvas_desktop_open_start_menu(kanvas_desktop_t* desk);
void kanvas_desktop_close_start_menu(kanvas_desktop_t* desk);
void kanvas_desktop_toggle_start_menu(kanvas_desktop_t* desk);

void kanvas_desktop_open_context_menu(kanvas_desktop_t* desk, int x, int y);
void kanvas_desktop_close_context_menu(kanvas_desktop_t* desk);

void kanvas_desktop_launch_app(kanvas_desktop_t* desk, kanvas_app_entry_t* app);
void kanvas_desktop_close_app(kanvas_desktop_t* desk, kui_window_t* win);
void kanvas_desktop_focus_window(kanvas_desktop_t* desk, kui_window_t* win);

void kanvas_desktop_set_wallpaper_color(kanvas_desktop_t* desk, uint32_t color);
void kanvas_desktop_set_wallpaper_image(kanvas_desktop_t* desk, uint32_t* data, int w, int h);

void kanvas_desktop_apply_theme_md3_dark(kanvas_desktop_t* desk);
void kanvas_desktop_apply_theme_md3_light(kanvas_desktop_t* desk);
void kanvas_desktop_apply_theme_classic(kanvas_desktop_t* desk);

void kanvas_desktop_tray_add(kanvas_desktop_t* desk, const char* name, void (*on_click)(void));
void kanvas_desktop_tray_remove(kanvas_desktop_t* desk, int index);

void kanvas_desktop_update(kanvas_desktop_t* desk, uint64_t now_ms);

#endif