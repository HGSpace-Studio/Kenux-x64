#ifndef KENUX_UI_H
#define KENUX_UI_H

#include "types.h"

/*
 * Kenux UI 基础绘制层
 *
 * 这层提供 Kenux 自己的 surface/theme/widget 抽象，统一使用 Kenux
 * framebuffer/font/color 后端，避免把用户态 ABI、GUI IPC 和文件配置
 * 依赖硬搬进内核。
 */

#define KENUX_UI_THEME_CLASSIC 0u
#define KENUX_UI_THEME_METRO   1u
#define KENUX_UI_THEME_MD3     2u

#define KENUX_UI_SCHEME_BLUE     0u
#define KENUX_UI_SCHEME_TEAL     1u
#define KENUX_UI_SCHEME_GREEN    2u
#define KENUX_UI_SCHEME_PURPLE   3u
#define KENUX_UI_SCHEME_RED      4u
#define KENUX_UI_SCHEME_GRAPHITE 5u
#define KENUX_UI_SCHEME_COUNT    6u

typedef enum {
    KENUX_UI_COLOR_TEXT = 0,
    KENUX_UI_COLOR_CONTENT,
    KENUX_UI_COLOR_SURFACE,
    KENUX_UI_COLOR_SUBTLE,
    KENUX_UI_COLOR_MUTED,
    KENUX_UI_COLOR_ACCENT,
    KENUX_UI_COLOR_TITLE_INACTIVE,
    KENUX_UI_COLOR_DESKTOP,
    KENUX_UI_COLOR_BORDER,
    KENUX_UI_COLOR_SELECTION,
    KENUX_UI_COLOR_DANGER,
    KENUX_UI_COLOR_SUCCESS,
    KENUX_UI_COLOR_WARNING,
    KENUX_UI_COLOR_COUNT
} kenux_ui_color_role_t;

#define KENUX_UI_BUTTON_PRESSED  0x01u
#define KENUX_UI_BUTTON_ACTIVE   0x02u
#define KENUX_UI_BUTTON_DISABLED 0x04u

#define KENUX_UI_MENU_SEPARATOR  0x01u
#define KENUX_UI_MENU_SELECTED   0x02u
#define KENUX_UI_MENU_DISABLED   0x04u

#define KENUX_UI_SCROLLBAR_DISABLED 0x01u
#define KENUX_UI_TREEVIEW_MAX_ITEMS 64u
#define KENUX_UI_KEY_ENTER          28u

#define KENUX_UI_FONT_H          24u
#define KENUX_UI_BUTTON_H        40u

typedef struct {
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    bool framebuffer_format;
} kenux_ui_surface_t;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
} kenux_ui_rect_t;

typedef struct {
    const char* label;
    uint32_t id;
    uint32_t flags;
} kenux_ui_context_menu_item_t;

typedef struct {
    const char* label;
    uint32_t width;
} kenux_ui_list_column_t;

typedef struct {
    uint32_t row_count;
    uint32_t visible_rows;
    uint32_t row_height;
    uint32_t scroll;
    int32_t selected;
    uint8_t focused;
} kenux_ui_listview_state_t;

typedef struct {
    uint32_t id;
    uint32_t parent_id;
    const char* const* cells;
    uint32_t flags;
} kenux_ui_treeview_item_t;

typedef struct {
    uint32_t visible_rows;
    uint32_t row_height;
    uint32_t scroll;
    uint32_t selected_id;
    uint32_t visible_count;
    uint32_t collapsed_count;
    uint32_t visible_indices[KENUX_UI_TREEVIEW_MAX_ITEMS];
    uint32_t collapsed_ids[KENUX_UI_TREEVIEW_MAX_ITEMS];
    uint8_t visible_depths[KENUX_UI_TREEVIEW_MAX_ITEMS];
    uint8_t focused;
    uint8_t has_selection;
} kenux_ui_treeview_state_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t gap;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t row_h;
} kenux_ui_layout_t;

uint32_t kenux_ui_theme(void);
int kenux_ui_theme_set(uint32_t theme);
uint32_t kenux_ui_theme_color_scheme(uint32_t theme);
uint32_t kenux_ui_theme_active_color_scheme(void);
uint32_t kenux_ui_theme_scheme_accent(uint32_t theme, uint32_t scheme);
int kenux_ui_theme_set_color_scheme(uint32_t theme, uint32_t scheme);
int kenux_ui_theme_set_appearance(uint32_t theme,
                                  uint32_t metro_scheme,
                                  uint32_t classic_scheme);
uint32_t kenux_ui_color(uint32_t role);

void kenux_ui_bind(kenux_ui_surface_t* surface, uint32_t* pixels,
                   uint32_t width, uint32_t height, uint32_t stride);
void kenux_ui_bind_framebuffer(kenux_ui_surface_t* surface);
int kenux_ui_hit(uint32_t px, uint32_t py, int32_t x, int32_t y,
                 uint32_t w, uint32_t h);
void kenux_ui_pixel(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t color);
void kenux_ui_rect(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                   uint32_t w, uint32_t h, uint32_t color);
void kenux_ui_text(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                   const char* text, uint32_t fg, uint32_t bg);
void kenux_ui_text_clipped(kenux_ui_surface_t* surface, uint32_t x,
                           uint32_t y, uint32_t w, const char* text,
                           uint32_t fg, uint32_t bg);
void kenux_ui_text_transparent(kenux_ui_surface_t* surface, uint32_t x,
                               uint32_t y, const char* text, uint32_t fg);
void kenux_ui_text_transparent_clipped(kenux_ui_surface_t* surface,
                                       uint32_t x, uint32_t y, uint32_t w,
                                       const char* text, uint32_t fg);

void kenux_ui_bevel(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t fill, uint32_t flags);
void kenux_ui_inset(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t fill);
void kenux_ui_button(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, const char* label,
                     uint32_t flags);
void kenux_ui_panel(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t color);
void kenux_ui_checkbox(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       const char* label, int checked, uint32_t flags);
void kenux_ui_progress(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t value, uint32_t max);
void kenux_ui_listview_header(kenux_ui_surface_t* surface, uint32_t x,
                              uint32_t y, uint32_t w,
                              const kenux_ui_list_column_t* cols,
                              uint32_t count);
void kenux_ui_listview_row(kenux_ui_surface_t* surface, uint32_t x,
                           uint32_t y, uint32_t w,
                           const kenux_ui_list_column_t* cols,
                           const char* const cells[], uint32_t count,
                           uint32_t flags);
void kenux_ui_listview_state_init(kenux_ui_listview_state_t* state,
                                  uint32_t visible_rows,
                                  uint32_t row_height);
void kenux_ui_listview_state_set_count(kenux_ui_listview_state_t* state,
                                       uint32_t row_count);
int kenux_ui_listview_state_handle_key(kenux_ui_listview_state_t* state,
                                       uint8_t keycode,
                                       uint32_t* activated);
int kenux_ui_listview_state_handle_mouse(kenux_ui_listview_state_t* state,
                                         int32_t px, int32_t py,
                                         uint32_t x, uint32_t rows_y,
                                         uint32_t w,
                                         uint32_t* activated);
int kenux_ui_listview_state_handle_wheel(kenux_ui_listview_state_t* state,
                                         int32_t wheel_delta);
void kenux_ui_treeview_state_init(kenux_ui_treeview_state_t* state,
                                  uint32_t visible_rows,
                                  uint32_t row_height);
void kenux_ui_treeview_state_set_viewport(kenux_ui_treeview_state_t* state,
                                          uint32_t visible_rows);
void kenux_ui_treeview_state_sync(kenux_ui_treeview_state_t* state,
                                  const kenux_ui_treeview_item_t* items,
                                  uint32_t count);
void kenux_ui_treeview(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       uint32_t w, const kenux_ui_list_column_t* cols,
                       uint32_t col_count,
                       const kenux_ui_treeview_item_t* items,
                       uint32_t count,
                       kenux_ui_treeview_state_t* state);
int kenux_ui_treeview_state_handle_key(kenux_ui_treeview_state_t* state,
                                       const kenux_ui_treeview_item_t* items,
                                       uint32_t count, uint8_t keycode,
                                       uint32_t* activated);
int kenux_ui_treeview_state_handle_mouse(kenux_ui_treeview_state_t* state,
                                         const kenux_ui_treeview_item_t* items,
                                         uint32_t count, int32_t px,
                                         int32_t py, uint32_t x,
                                         uint32_t rows_y, uint32_t w,
                                         uint32_t* activated);
int kenux_ui_treeview_state_handle_wheel(kenux_ui_treeview_state_t* state,
                                         int32_t wheel_delta);
void kenux_ui_vscrollbar(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, uint32_t value,
                         uint32_t max, uint32_t page, uint32_t flags);
void kenux_ui_hscrollbar(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, uint32_t value,
                         uint32_t max, uint32_t page, uint32_t flags);
void kenux_ui_scroll_view_frame(kenux_ui_surface_t* surface, uint32_t x,
                                uint32_t y, uint32_t w, uint32_t h);
int kenux_ui_vscrollbar_handle_mouse(uint32_t* value, uint32_t max,
                                     uint32_t page, uint32_t x,
                                     uint32_t y, uint32_t w,
                                     uint32_t h, int32_t px, int32_t py);
int kenux_ui_vscrollbar_handle_wheel(uint32_t* value, uint32_t max,
                                     uint32_t page, int32_t wheel_delta);
int kenux_ui_hscrollbar_handle_mouse(uint32_t* value, uint32_t max,
                                     uint32_t page, uint32_t x,
                                     uint32_t y, uint32_t w,
                                     uint32_t h, int32_t px, int32_t py);

uint32_t kenux_ui_context_menu_height(uint32_t count);
void kenux_ui_context_menu(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                           uint32_t w,
                           const kenux_ui_context_menu_item_t* items,
                           uint32_t count);
int kenux_ui_context_menu_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                              uint32_t w,
                              const kenux_ui_context_menu_item_t* items,
                              uint32_t count, uint32_t* out_id);

void kenux_ui_layout_begin(kenux_ui_layout_t* layout, uint32_t x, uint32_t y,
                           uint32_t w, uint32_t h, uint32_t gap);
kenux_ui_rect_t kenux_ui_layout_next(kenux_ui_layout_t* layout,
                                     uint32_t preferred_w,
                                     uint32_t preferred_h);
uint32_t kenux_ui_anim_progress(uint64_t now, uint64_t start,
                                uint64_t duration_ms);
uint32_t kenux_ui_anim_ease_out(uint32_t progress);
uint32_t kenux_ui_anim_lerp(uint32_t from, uint32_t to, uint32_t progress);

void kenux_ui_md3_button(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, const char* label,
                         uint32_t flags);
void kenux_ui_md3_card(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, uint32_t elevation);
void kenux_ui_md3_chip(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                       const char* label, int selected, uint32_t flags);
void kenux_ui_md3_fab(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                      uint32_t size, const char* icon_label);
void kenux_ui_md3_snackbar(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                           uint32_t w, const char* message);
void kenux_ui_md3_divider(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                          uint32_t w);
void kenux_ui_md3_switch(kenux_ui_surface_t* surface, uint32_t x, uint32_t y,
                         int on, uint32_t flags);

#endif /* KENUX_UI_H */