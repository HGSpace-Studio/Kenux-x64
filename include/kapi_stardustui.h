#ifndef KAPI_STARDUSTUI_H
#define KAPI_STARDUSTUI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kapi_window.h"
#include "kapi_graphics2d.h"
#include "kapi_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STARDUSTUI_MAX_WINDOWS     64
#define STARDUSTUI_MAX_COMPONENTS  512
#define STARDUSTUI_MAX_FONTS       16
#define STARDUSTUI_THEME_CACHE_SIZE 64
#define STARDUSTUI_MAX_LAYERS      32
#define STARDUSTUI_MAX_DIRTY_RECTS 128
#define STARDUSTUI_ANIMATOR_POOL   256
#define STARDUSTUI_MAX_POPUPS      16
#define STARDUSTUI_MAX_TOOLTIPS    8

typedef enum {
    SDUI_MSG_MOUSE_MOVE = 1,
    SDUI_MSG_LBUTTON_DOWN,
    SDUI_MSG_LBUTTON_UP,
    SDUI_MSG_RBUTTON_DOWN,
    SDUI_MSG_RBUTTON_UP,
    SDUI_MSG_MBUTTON_DOWN,
    SDUI_MSG_MBUTTON_UP,
    SDUI_MSG_SCROLL_UP,
    SDUI_MSG_SCROLL_DOWN,
    SDUI_MSG_KEY_DOWN,
    SDUI_MSG_KEY_UP,
    SDUI_MSG_CHAR_INPUT,
    SDUI_MSG_RESIZE,
    SDUI_MSG_MOVE,
    SDUI_MSG_FOCUS_IN,
    SDUI_MSG_FOCUS_OUT,
    SDUI_MSG_CLOSE,
    SDUI_MSG_PAINT,
    SDUI_MSG_TIMER,
    SDUI_MSG_DRAG_ENTER,
    SDUI_MSG_DRAG_LEAVE,
    SDUI_MSG_DRAG_OVER,
    SDUI_MSG_DROP,
    SDUI_MSG_CUSTOM
} sdui_msg_type_t;

typedef struct {
    int x, y;
} sdui_point_t;

typedef struct {
    int x, y, width, height;
} sdui_rect_t;

typedef struct {
    uint8_t r, g, b, a;
} sdui_color_t;

#define SDUI_COLOR(r,g,b,a) ((sdui_color_t){(uint8_t)(r),(uint8_t)(g),(uint8_t)(b),(uint8_t)(a)})
#define SDUI_COLOR_TO_U32(c) ((uint32_t)((c).a << 24 | (c).r << 16 | (c).g << 8 | (c).b))
#define SDUI_U32_TO_COLOR(v) ({ uint32_t _v=(v); (sdui_color_t){(_v>>16)&0xFF,(_v>>8)&0xFF,_v&0xFF,(_v>>24)&0xFF}; })

typedef struct sdui_theme {
    sdui_color_t background;
    sdui_color_t foreground;
    sdui_color_t accent;
    sdui_color_t text_primary;
    sdui_color_t text_secondary;
    sdui_color_t text_disabled;
    sdui_color_t border;
    sdui_color_t border_light;
    sdui_color_t shadow;
    sdui_color_t highlight;
    sdui_color_t disabled_bg;
    sdui_color_t error;
    sdui_color_t success;
    sdui_color_t warning;
    sdui_color_t info;
    sdui_color_t link;
    sdui_color_t selection_bg;
    sdui_color_t selection_fg;
    int corner_radius;
    int corner_radius_small;
    int corner_radius_large;
    int shadow_offset_x;
    int shadow_offset_y;
    int shadow_blur;
    int border_width;
    int padding;
    int margin;
    int spacing;
    char font_family[64];
    int font_size;
    int font_size_small;
    int font_size_large;
    int title_bar_height;
    int scrollbar_width;
    int icon_size;
} sdui_theme_t;

typedef struct sdui_component sdui_component_t;
typedef struct sdui_window sdui_window_t;

typedef void (*sdui_draw_fn)(sdui_component_t* comp, uint32_t* fb, int stride, int fb_w, int fb_h);
typedef bool (*sdui_mouse_move_fn)(sdui_component_t* comp, int x, int y);
typedef bool (*sdui_button_fn)(sdui_component_t* comp, bool down, int x, int y);
typedef bool (*sdui_scroll_fn)(sdui_component_t* comp, int delta);
typedef bool (*sdui_key_fn)(sdui_component_t* comp, int key, bool down);
typedef void (*sdui_resize_fn)(sdui_component_t* comp, int old_w, int old_h, int new_w, int new_h);
typedef void (*sdui_msg_handler_fn)(sdui_window_t* win, sdui_msg_type_t msg, uint64_t p1, uint64_t p2);

struct sdui_component {
    uint32_t id;
    sdui_rect_t bounds;
    sdui_rect_t clip;
    bool visible;
    bool enabled;
    bool focused;
    bool hovered;
    bool needs_redraw;
    bool accepts_focus;
    bool tab_stop;
    int z_order;
    float opacity;
    sdui_draw_fn draw;
    sdui_mouse_move_fn on_mouse_move;
    sdui_button_fn on_left_button;
    sdui_button_fn on_right_button;
    sdui_scroll_fn on_scroll;
    sdui_key_fn on_key;
    sdui_resize_fn on_parent_resize;
    void* user_data;
    char name[64];
    sdui_window_t* parent_window;
    sdui_component_t* next;
    sdui_component_t* prev;
};

typedef struct {
    sdui_component_t base;
    const char* text;
    sdui_color_t bg_color;
    sdui_color_t fg_color;
    sdui_color_t hover_color;
    sdui_color_t pressed_color;
    sdui_color_t disabled_color;
    bool is_pressed;
    bool is_hovered;
    int icon_id;
    void (*on_click)(struct sdui_button*);
} sdui_button_t;

typedef struct {
    sdui_component_t base;
    char* text;
    sdui_color_t text_color;
    int alignment;
    bool word_wrap;
    int max_lines;
} sdui_label_t;

typedef struct {
    sdui_component_t base;
    char* text;
    size_t text_capacity;
    size_t cursor_pos;
    size_t selection_start;
    size_t selection_end;
    bool read_only;
    bool password_mode;
    char password_char;
    int max_chars;
    int scroll_offset;
    void (*on_text_changed)(struct sdui_textbox*);
    void (*on_enter_pressed)(struct sdui_textbox*);
} sdui_textbox_t;

typedef struct {
    sdui_component_t base;
    int min_value;
    int max_value;
    int current_value;
    sdui_color_t bar_color;
    sdui_color_t background_color;
    bool show_percentage;
    bool animated;
    float animation_progress;
} sdui_progressbar_t;

typedef struct {
    sdui_component_t base;
    int min_value;
    int max_value;
    int current_value;
    sdui_color_t track_color;
    sdui_color_t thumb_color;
    int thumb_size;
    bool vertical;
    bool dragging;
    void (*on_value_changed)(struct sdui_slider*, int);
} sdui_slider_t;

typedef struct {
    sdui_component_t base;
    bool checked;
    sdui_color_t check_color;
    sdui_color_t box_color;
    const char* label;
    void (*on_state_changed)(struct sdui_checkbox*, bool);
} sdui_checkbox_t;

typedef struct {
    const char* text;
    void* data;
    bool selected;
    bool enabled;
} sdui_listbox_item_t;

typedef struct {
    sdui_component_t base;
    sdui_listbox_item_t* items;
    int item_count;
    int capacity;
    int selected_index;
    int scroll_offset;
    int visible_items;
    int item_height;
    sdui_color_t item_bg_color;
    sdui_color_t item_selected_color;
    sdui_color_t item_text_color;
    sdui_color_t item_selected_text_color;
    bool multi_select;
    void (*on_selection_changed)(struct sdui_listbox*, int);
    void (*on_item_double_click)(struct sdui_listbox*, int);
} sdui_listbox_t;

typedef struct {
    sdui_component_t base;
    sdui_component_t** children;
    int child_count;
    int capacity;
    sdui_color_t background_color;
    int border_width;
    sdui_color_t border_color;
    int corner_radius;
    bool scrollable;
    int scroll_x;
    int scroll_y;
    int max_scroll_x;
    int max_scroll_y;
} sdui_panel_t;

typedef struct {
    sdui_component_t base;
    uint32_t* pixel_data;
    int image_width;
    int image_height;
    bool stretch_to_fit;
    bool maintain_aspect_ratio;
    sdui_color_t tint_color;
    float alpha;
} sdui_image_t;

typedef struct {
    const char* text;
    int icon_id;
    bool enabled;
    bool checked;
    bool separator;
    struct sdui_menu_item* submenu;
    int submenu_item_count;
    void (*on_selected)(struct sdui_menu_item*);
} sdui_menu_item_t;

typedef struct {
    sdui_component_t base;
    sdui_menu_item_t** menus;
    int menu_count;
    int open_menu_index;
    int highlighted_item_index;
    sdui_color_t bg_color;
    sdui_color_t text_color;
    sdui_color_t highlight_color;
    sdui_color_t border_color;
} sdui_menubar_t;

typedef struct {
    sdui_component_t base;
    char** sections;
    int section_count;
    sdui_color_t bg_color;
    sdui_color_t text_color;
    sdui_color_t border_color;
} sdui_statusbar_t;

typedef struct {
    const char* title;
    int icon_id;
    bool enabled;
    bool pressed;
    bool toggleable;
    bool toggled;
    void (*on_click)(struct sdui_toolbar_btn*);
} sdui_toolbar_btn_t;

typedef struct {
    sdui_component_t base;
    sdui_toolbar_btn_t* buttons;
    int button_count;
    bool vertical;
    sdui_color_t bg_color;
    sdui_color_t button_color;
    sdui_color_t hover_color;
    sdui_color_t pressed_color;
    int button_size;
    int spacing;
} sdui_toolbar_t;

typedef struct {
    const char* title;
    int icon_id;
    sdui_component_t* content;
    bool closable;
    bool visible;
} sdui_tab_item_t;

typedef struct {
    sdui_component_t base;
    sdui_tab_item_t* tabs;
    int tab_count;
    int active_tab_index;
    sdui_color_t tab_bg_color;
    sdui_color_t tab_active_color;
    sdui_color_t tab_text_color;
    sdui_color_t tab_active_text_color;
    sdui_color_t border_color;
    void (*on_tab_changed)(struct sdui_tabcontrol*, int);
} sdui_tabcontrol_t;

typedef struct sdui_treeview_node {
    const char* text;
    int icon_id;
    bool expanded;
    bool selected;
    bool has_children;
    void* data;
    struct sdui_treeview_node* parent;
    struct sdui_treeview_node** children;
    int child_count;
} sdui_treeview_node_t;

typedef struct {
    sdui_component_t base;
    sdui_treeview_node_t* root_nodes;
    int root_node_count;
    sdui_treeview_node_t* selected_node;
    int indent_size;
    sdui_color_t node_text_color;
    sdui_color_t node_selected_color;
    sdui_color_t line_color;
    void (*on_node_selected)(struct sdui_treeview*, sdui_treeview_node_t*);
} sdui_treeview_t;

typedef struct {
    sdui_component_t base;
    sdui_component_t* pane1;
    sdui_component_t* pane2;
    bool horizontal;
    int splitter_position;
    int splitter_size;
    sdui_color_t splitter_color;
    int min_pane1_size;
    int min_pane2_size;
    void (*on_splitter_moved)(struct sdui_splitter*, int);
} sdui_splitter_t;

typedef struct {
    sdui_component_t base;
    const char* text;
    sdui_window_t* owner;
    int timeout_ms;
    uint64_t show_time;
} sdui_tooltip_t;

typedef struct sdui_animator {
    sdui_component_t* target;
    uint64_t start_time;
    uint64_t duration_ms;
    float start_value;
    float end_value;
    float current_value;
    bool active;
    bool loop;
    void (*easing)(struct sdui_animator*);
    void (*on_complete)(struct sdui_animator*);
    void (*on_update)(struct sdui_animator*, float value);
} sdui_animator_t;

struct sdui_window {
    uint32_t id;
    char title[256];
    sdui_rect_t client_rect;
    sdui_rect_t window_rect;
    bool visible;
    bool focused;
    bool minimized;
    bool maximized;
    bool resizable;
    bool decorated;
    bool modal;
    bool topmost;
    bool fullscreen;
    bool closing;
    bool dragging;
    sdui_point_t drag_start;
    sdui_component_t* components_head;
    sdui_component_t* components_tail;
    int component_count;
    sdui_menubar_t* menubar;
    sdui_statusbar_t* statusbar;
    sdui_toolbar_t* toolbar;
    sdui_msg_handler_fn message_handler;
    void* user_data;
    int display_layer_id;
    sdui_color_t title_bar_color;
    sdui_color_t client_area_color;
    int title_bar_height;
    int border_width;
    sdui_window_t* next;
    sdui_window_t* prev;
    sdui_window_t* parent;
    sdui_window_t* first_child;
    sdui_window_t* last_child;
    uint32_t* back_buffer;
    uint32_t* front_buffer;
    int buffer_stride;
    sdui_rect_t dirty_rects[STARDUSTUI_MAX_DIRTY_RECTS];
    int dirty_count;
};

typedef struct {
    int screen_width;
    int screen_height;
    int bpp;
    uint32_t* framebuffer;
    int stride;
    sdui_window_t* window_list;
    sdui_window_t* focused_window;
    sdui_window_t* capture_window;
    sdui_window_t* modal_window;
    sdui_window_t* desktop_window;
    sdui_theme_t theme;
    sdui_theme_t theme_cache[STARDUSTUI_THEME_CACHE_SIZE];
    int theme_cache_count;
    sdui_component_t* component_pool[STARDUSTUI_MAX_COMPONENTS];
    int component_pool_count;
    sdui_animator_t animators[STARDUSTUI_ANIMATOR_POOL];
    int animator_count;
    sdui_tooltip_t* active_tooltips[STARDUSTUI_MAX_TOOLTIPS];
    int tooltip_count;
    uint32_t next_component_id;
    uint32_t next_window_id;
    sdui_point_t mouse_pos;
    sdui_component_t* hover_component;
    sdui_component_t* focus_component;
    sdui_component_t* capture_component;
    bool initialized;
} sdui_context_t;

int sdui_init(int screen_w, int screen_h, int bpp, uint32_t* fb, int stride);
void sdui_shutdown(void);

const sdui_theme_t* sdui_get_theme(void);
void sdui_set_theme(const sdui_theme_t* theme);
void sdui_apply_theme_recursive(sdui_window_t* win);

sdui_window_t* sdui_create_window(const char* title, int x, int y, int w, int h,
                                   bool resizable, bool decorated);
void sdui_destroy_window(sdui_window_t* win);
void sdui_show_window(sdui_window_t* win);
void sdui_hide_window(sdui_window_t* win);
void sdui_focus_window(sdui_window_t* win);
void sdui_minimize_window(sdui_window_t* win);
void sdui_maximize_window(sdui_window_t* win);
void sdui_restore_window(sdui_window_t* win);
void sdui_set_window_title(sdui_window_t* win, const char* title);
void sdui_set_window_rect(sdui_window_t* win, int x, int y, int w, int h);
bool sdui_is_window_visible(sdui_window_t* win);
bool sdui_is_window_focused(sdui_window_t* win);

sdui_component_t* sdui_add_component(sdui_window_t* win, sdui_component_t* comp);
void sdui_remove_component(sdui_window_t* win, sdui_component_t* comp);
void sdui_destroy_component(sdui_component_t* comp);
sdui_component_t* sdui_find_component_by_id(uint32_t id);

sdui_button_t* sdui_create_button(const char* text, sdui_rect_t bounds, void (*on_click)(sdui_button_t*));
sdui_label_t* sdui_create_label(const char* text, sdui_rect_t bounds, int alignment);
sdui_textbox_t* sdui_create_textbox(sdui_rect_t bounds, int max_chars, void (*on_changed)(sdui_textbox_t*));
sdui_progressbar_t* sdui_create_progressbar(sdui_rect_t bounds, int min_val, int max_val);
sdui_slider_t* sdui_create_slider(sdui_rect_t bounds, int min_val, int max_val, void (*on_changed)(sdui_slider_t*, int));
sdui_checkbox_t* sdui_create_checkbox(const char* label, sdui_rect_t bounds, void (*on_changed)(sdui_checkbox_t*, bool));
sdui_listbox_t* sdui_create_listbox(sdui_rect_t bounds, int visible_items, void (*on_sel)(sdui_listbox_t*, int));
sdui_panel_t* sdui_create_panel(sdui_rect_t bounds, sdui_color_t bg);
sdui_image_t* sdui_create_image(sdui_rect_t bounds, uint32_t* pixels, int img_w, int img_h);
sdui_menubar_t* sdui_create_menubar(sdui_rect_t bounds);
sdui_statusbar_t* sdui_create_statusbar(sdui_rect_t bounds);
sdui_toolbar_t* sdui_create_toolbar(sdui_rect_t bounds, bool vertical);
sdui_tabcontrol_t* sdui_create_tabcontrol(sdui_rect_t bounds);
sdui_treeview_t* sdui_create_treeview(sdui_rect_t bounds);
sdui_splitter_t* sdui_create_splitter(sdui_rect_t bounds, bool horizontal);
sdui_tooltip_t* sdui_create_tooltip(const char* text, sdui_window_t* owner, int timeout_ms);

void sdui_draw_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color);
void sdui_draw_filled_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color);
void sdui_draw_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, int radius, uint32_t color);
void sdui_draw_rounded_rect_outline(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, int radius, uint32_t color);
void sdui_draw_line(uint32_t* fb, int stride, int fb_w, int fb_h, int x0, int y0, int x1, int y1, uint32_t color);
void sdui_draw_circle(uint32_t* fb, int stride, int fb_w, int fb_h, int cx, int cy, int r, uint32_t color, bool filled);
void sdui_draw_ellipse(uint32_t* fb, int stride, int fb_w, int fb_h, int cx, int cy, int rx, int ry, uint32_t color, bool filled);
void sdui_draw_text(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, const char* text, uint32_t color, int font_size, int bold);
void sdui_draw_text_clipped(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, const char* text, uint32_t color, int font_size, int bold, int clip_x, int clip_y, int clip_w, int clip_h);
void sdui_draw_icon(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int icon_id, int size, uint32_t color);
void sdui_draw_checkmark(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int size, uint32_t color);
void sdui_draw_shadow(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, int offset_x, int offset_y, int blur, uint8_t alpha);
void sdui_draw_gradient_v(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color_top, uint32_t color_bottom);
void sdui_draw_gradient_h(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color_left, uint32_t color_right);
void sdui_draw_alpha_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void sdui_draw_image(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, uint32_t* img, int img_w, int img_h, int img_stride);
void sdui_draw_image_scaled(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t* img, int img_w, int img_h, int img_stride);
void sdui_blit(uint32_t* dst, int dst_stride, int dst_x, int dst_y, uint32_t* src, int src_stride, int src_x, int src_y, int w, int h);
void sdui_blit_alpha(uint32_t* dst, int dst_stride, int dst_x, int dst_y, uint32_t* src, int src_stride, int src_x, int src_y, int w, int h, uint8_t alpha);

bool sdui_point_in_rect(int x, int y, sdui_rect_t r);
bool sdui_rect_intersect(sdui_rect_t a, sdui_rect_t b);
sdui_rect_t sdui_rect_union(sdui_rect_t a, sdui_rect_t b);
sdui_rect_t sdui_rect_clip(sdui_rect_t a, sdui_rect_t b);

void sdui_invalidate(sdui_component_t* comp);
void sdui_invalidate_rect(sdui_window_t* win, sdui_rect_t rect);
void sdui_invalidate_all(sdui_window_t* win);

void sdui_process_mouse_move(int x, int y);
void sdui_process_mouse_button(int button, bool down, int x, int y);
void sdui_process_scroll(int delta, int x, int y);
void sdui_process_key(int key, bool down, uint32_t modifiers);
void sdui_process_char(uint32_t ch);

void sdui_render(void);
void sdui_render_window(sdui_window_t* win);
void sdui_present(void);
void sdui_update(void);

sdui_animator_t* sdui_animate(sdui_component_t* target, float from, float to, uint64_t duration_ms, void (*on_update)(sdui_animator_t*, float));
void sdui_animator_update_all(uint64_t now_ms);
void sdui_animator_cancel(sdui_animator_t* anim);

void sdui_show_tooltip(sdui_tooltip_t* tip);
void sdui_hide_tooltip(sdui_tooltip_t* tip);
void sdui_update_tooltips(uint64_t now_ms);

sdui_context_t* sdui_get_context(void);

int sdui_window_init(void);

#ifdef __cplusplus
}
#endif

#endif