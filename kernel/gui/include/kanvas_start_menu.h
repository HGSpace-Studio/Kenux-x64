#ifndef KANVAS_START_MENU_H
#define KANVAS_START_MENU_H

#include "kapi_kanvasui.h"
#include "kapi_graphics2d.h"

#define KANVAS_START_W              420
#define KANVAS_START_H              520
#define KANVAS_START_SEARCH_H       44
#define KANVAS_START_ITEM_H         36
#define KANVAS_START_ICON_SZ        24
#define KANVAS_START_PAD            12
#define KANVAS_START_RADIUS         16
#define KANVAS_START_COL_APPS       0
#define KANVAS_START_COL_SYSTEM     1
#define KANVAS_START_COL_POWER      2
#define KANVAS_START_MAX_RESULTS    32
#define KANVAS_START_MAX_ALL        128

typedef enum {
    KANVAS_START_SECTION_PINNED = 0,
    KANVAS_START_SECTION_ALL,
    KANVAS_START_SECTION_SYSTEM,
    KANVAS_START_SECTION_POWER
} kanvas_start_section_t;

typedef struct {
    char name[64];
    char exec[128];
    int icon_id;
    kanvas_start_section_t section;
    bool hovered;
    void* app_data;
} kanvas_start_item_t;

typedef struct kanvas_start_menu_s {
    bool visible;
    int x, y;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t hover_color;
    kui_color_t accent_color;
    kui_color_t search_bg;
    kui_color_t search_fg;
    kui_color_t divider_color;
    kui_color_t power_off_color;
    kui_color_t power_restart_color;
    kui_color_t power_sleep_color;
    char search_text[128];
    int cursor_pos;
    kanvas_start_item_t all_items[KANVAS_START_MAX_ALL];
    int all_count;
    kanvas_start_item_t* filtered[KANVAS_START_MAX_RESULTS];
    int filtered_count;
    int selected_index;
    int scroll_offset;
    int max_visible;
    bool search_focused;
} kanvas_start_menu_t;

kanvas_start_menu_t* kanvas_start_menu_create(int x, int y);
void kanvas_start_menu_destroy(kanvas_start_menu_t* sm);
void kanvas_start_menu_paint(kanvas_start_menu_t* sm, uint32_t* fb, int stride, int fw, int fh);
void kanvas_start_menu_handle_mouse(kanvas_start_menu_t* sm, int mx, int my, bool left_down, bool left_up);
void kanvas_start_menu_handle_key(kanvas_start_menu_t* sm, int key, bool down, uint32_t mods);
void kanvas_start_menu_handle_char(kanvas_start_menu_t* sm, uint32_t ch);

int kanvas_start_menu_add_item(kanvas_start_menu_t* sm, const char* name, const char* exec,
                                int icon_id, kanvas_start_section_t section, void* app_data);
void kanvas_start_menu_filter(kanvas_start_menu_t* sm);
void kanvas_start_menu_show(kanvas_start_menu_t* sm);
void kanvas_start_menu_hide(kanvas_start_menu_t* sm);

#endif