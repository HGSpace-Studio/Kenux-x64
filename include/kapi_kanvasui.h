#ifndef KAPI_KANVASUI_H
#define KAPI_KANVASUI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kapi_window.h"
#include "kapi_graphics2d.h"
#include "kapi_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KANVASUI_MAX_WINDOWS     64
#define KANVASUI_MAX_COMPONENTS  512
#define KANVASUI_MAX_FONTS       16
#define KANVASUI_THEME_CACHE_SIZE 64
#define KANVASUI_MAX_LAYERS      32
#define KANVASUI_MAX_DIRTY_RECTS 128
#define KANVASUI_ANIMATOR_POOL   256
#define KANVASUI_MAX_POPUPS      16
#define KANVASUI_MAX_TOOLTIPS    8

typedef enum {
    KUI_MSG_MOUSE_MOVE = 1,
    KUI_MSG_LBUTTON_DOWN,
    KUI_MSG_LBUTTON_UP,
    KUI_MSG_RBUTTON_DOWN,
    KUI_MSG_RBUTTON_UP,
    KUI_MSG_MBUTTON_DOWN,
    KUI_MSG_MBUTTON_UP,
    KUI_MSG_SCROLL_UP,
    KUI_MSG_SCROLL_DOWN,
    KUI_MSG_KEY_DOWN,
    KUI_MSG_KEY_UP,
    KUI_MSG_CHAR_INPUT,
    KUI_MSG_RESIZE,
    KUI_MSG_MOVE,
    KUI_MSG_FOCUS_IN,
    KUI_MSG_FOCUS_OUT,
    KUI_MSG_CLOSE,
    KUI_MSG_PAINT,
    KUI_MSG_TIMER,
    KUI_MSG_DRAG_ENTER,
    KUI_MSG_DRAG_LEAVE,
    KUI_MSG_DRAG_OVER,
    KUI_MSG_DROP,
    KUI_MSG_CUSTOM
} kui_msg_type_t;

typedef struct {
    int x, y;
} kui_point_t;

typedef struct {
    int x, y, width, height;
} kui_rect_t;

typedef struct {
    uint8_t r, g, b, a;
} kui_color_t;

#define KUI_COLOR(r,g,b,a) ((kui_color_t){(uint8_t)(r),(uint8_t)(g),(uint8_t)(b),(uint8_t)(a)})
#define KUI_COLOR_TO_U32(c) ((uint32_t)((c).a << 24 | (c).r << 16 | (c).g << 8 | (c).b))
#define KUI_U32_TO_COLOR(v) ({ uint32_t _v=(v); (kui_color_t){(_v>>16)&0xFF,(_v>>8)&0xFF,_v&0xFF,(_v>>24)&0xFF}; })

typedef struct kui_theme {
    kui_color_t background;
    kui_color_t foreground;
    kui_color_t accent;
    kui_color_t text_primary;
    kui_color_t text_secondary;
    kui_color_t text_disabled;
    kui_color_t border;
    kui_color_t border_light;
    kui_color_t shadow;
    kui_color_t highlight;
    kui_color_t disabled_bg;
    kui_color_t error;
    kui_color_t success;
    kui_color_t warning;
    kui_color_t info;
    kui_color_t link;
    kui_color_t selection_bg;
    kui_color_t selection_fg;
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
} kui_theme_t;

typedef struct kui_component kui_component_t;
typedef struct kui_window kui_window_t;

typedef void (*kui_draw_fn)(kui_component_t* comp, uint32_t* fb, int stride, int fb_w, int fb_h);
typedef bool (*kui_mouse_move_fn)(kui_component_t* comp, int x, int y);
typedef bool (*kui_button_fn)(kui_component_t* comp, bool down, int x, int y);
typedef bool (*kui_scroll_fn)(kui_component_t* comp, int delta);
typedef bool (*kui_key_fn)(kui_component_t* comp, int key, bool down);
typedef void (*kui_resize_fn)(kui_component_t* comp, int old_w, int old_h, int new_w, int new_h);
typedef void (*kui_msg_handler_fn)(kui_window_t* win, kui_msg_type_t msg, uint64_t p1, uint64_t p2);

struct kui_component {
    uint32_t id;
    kui_rect_t bounds;
    kui_rect_t clip;
    bool visible;
    bool enabled;
    bool focused;
    bool hovered;
    bool needs_redraw;
    bool accepts_focus;
    bool tab_stop;
    int z_order;
    float opacity;
    kui_draw_fn draw;
    kui_mouse_move_fn on_mouse_move;
    kui_button_fn on_left_button;
    kui_button_fn on_right_button;
    kui_scroll_fn on_scroll;
    kui_key_fn on_key;
    kui_resize_fn on_parent_resize;
    void* user_data;
    char name[64];
    kui_window_t* parent_window;
    kui_component_t* next;
    kui_component_t* prev;
};

typedef struct {
    kui_component_t base;
    const char* text;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t hover_color;
    kui_color_t pressed_color;
    kui_color_t disabled_color;
    bool is_pressed;
    bool is_hovered;
    int icon_id;
    void (*on_click)(struct kui_button*);
} kui_button_t;

typedef struct {
    kui_component_t base;
    char* text;
    kui_color_t text_color;
    int alignment;
    bool word_wrap;
    int max_lines;
} kui_label_t;

typedef struct {
    kui_component_t base;
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
    void (*on_text_changed)(struct kui_textbox*);
    void (*on_enter_pressed)(struct kui_textbox*);
} kui_textbox_t;

typedef struct {
    kui_component_t base;
    int min_value;
    int max_value;
    int current_value;
    kui_color_t bar_color;
    kui_color_t background_color;
    bool show_percentage;
    bool animated;
    float animation_progress;
} kui_progressbar_t;

typedef struct {
    kui_component_t base;
    int min_value;
    int max_value;
    int current_value;
    kui_color_t track_color;
    kui_color_t thumb_color;
    int thumb_size;
    bool vertical;
    bool dragging;
    void (*on_value_changed)(struct kui_slider*, int);
} kui_slider_t;

typedef struct {
    kui_component_t base;
    bool checked;
    kui_color_t check_color;
    kui_color_t box_color;
    const char* label;
    void (*on_state_changed)(struct kui_checkbox*, bool);
} kui_checkbox_t;

typedef struct {
    const char* text;
    void* data;
    bool selected;
    bool enabled;
} kui_listbox_item_t;

typedef struct {
    kui_component_t base;
    kui_listbox_item_t* items;
    int item_count;
    int capacity;
    int selected_index;
    int scroll_offset;
    int visible_items;
    int item_height;
    kui_color_t item_bg_color;
    kui_color_t item_selected_color;
    kui_color_t item_text_color;
    kui_color_t item_selected_text_color;
    bool multi_select;
    void (*on_selection_changed)(struct kui_listbox*, int);
    void (*on_item_double_click)(struct kui_listbox*, int);
} kui_listbox_t;

typedef struct {
    kui_component_t base;
    kui_component_t** children;
    int child_count;
    int capacity;
    kui_color_t background_color;
    int border_width;
    kui_color_t border_color;
    int corner_radius;
    bool scrollable;
    int scroll_x;
    int scroll_y;
    int max_scroll_x;
    int max_scroll_y;
} kui_panel_t;

typedef struct {
    kui_component_t base;
    uint32_t* pixel_data;
    int image_width;
    int image_height;
    bool stretch_to_fit;
    bool maintain_aspect_ratio;
    kui_color_t tint_color;
    float alpha;
} kui_image_t;

typedef struct {
    const char* text;
    int icon_id;
    bool enabled;
    bool checked;
    bool separator;
    struct kui_menu_item* submenu;
    int submenu_item_count;
    void (*on_selected)(struct kui_menu_item*);
} kui_menu_item_t;

typedef struct {
    kui_component_t base;
    kui_menu_item_t** menus;
    int menu_count;
    int open_menu_index;
    int highlighted_item_index;
    kui_color_t bg_color;
    kui_color_t text_color;
    kui_color_t highlight_color;
    kui_color_t border_color;
} kui_menubar_t;

typedef struct {
    kui_component_t base;
    char** sections;
    int section_count;
    kui_color_t bg_color;
    kui_color_t text_color;
    kui_color_t border_color;
} kui_statusbar_t;

typedef struct {
    const char* title;
    int icon_id;
    bool enabled;
    bool pressed;
    bool toggleable;
    bool toggled;
    void (*on_click)(struct kui_toolbar_btn*);
} kui_toolbar_btn_t;

typedef struct {
    kui_component_t base;
    kui_toolbar_btn_t* buttons;
    int button_count;
    bool vertical;
    kui_color_t bg_color;
    kui_color_t button_color;
    kui_color_t hover_color;
    kui_color_t pressed_color;
    int button_size;
    int spacing;
} kui_toolbar_t;

typedef struct {
    const char* title;
    int icon_id;
    kui_component_t* content;
    bool closable;
    bool visible;
} kui_tab_item_t;

typedef struct {
    kui_component_t base;
    kui_tab_item_t* tabs;
    int tab_count;
    int active_tab_index;
    kui_color_t tab_bg_color;
    kui_color_t tab_active_color;
    kui_color_t tab_text_color;
    kui_color_t tab_active_text_color;
    kui_color_t border_color;
    void (*on_tab_changed)(struct kui_tabcontrol*, int);
} kui_tabcontrol_t;

typedef struct kui_treeview_node {
    const char* text;
    int icon_id;
    bool expanded;
    bool selected;
    bool has_children;
    void* data;
    struct kui_treeview_node* parent;
    struct kui_treeview_node** children;
    int child_count;
} kui_treeview_node_t;

typedef struct {
    kui_component_t base;
    kui_treeview_node_t* root_nodes;
    int root_node_count;
    kui_treeview_node_t* selected_node;
    int indent_size;
    kui_color_t node_text_color;
    kui_color_t node_selected_color;
    kui_color_t line_color;
    void (*on_node_selected)(struct kui_treeview*, kui_treeview_node_t*);
} kui_treeview_t;

typedef struct {
    kui_component_t base;
    kui_component_t* pane1;
    kui_component_t* pane2;
    bool horizontal;
    int splitter_position;
    int splitter_size;
    kui_color_t splitter_color;
    int min_pane1_size;
    int min_pane2_size;
    void (*on_splitter_moved)(struct kui_splitter*, int);
} kui_splitter_t;

typedef struct {
    kui_component_t base;
    const char* text;
    kui_window_t* owner;
    int timeout_ms;
    uint64_t show_time;
} kui_tooltip_t;

typedef struct kui_animator {
    kui_component_t* target;
    uint64_t start_time;
    uint64_t duration_ms;
    float start_value;
    float end_value;
    float current_value;
    bool active;
    bool loop;
    void (*easing)(struct kui_animator*);
    void (*on_complete)(struct kui_animator*);
    void (*on_update)(struct kui_animator*, float value);
} kui_animator_t;

struct kui_window {
    uint32_t id;
    char title[256];
    kui_rect_t client_rect;
    kui_rect_t window_rect;
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
    kui_point_t drag_start;
    kui_component_t* components_head;
    kui_component_t* components_tail;
    int component_count;
    kui_menubar_t* menubar;
    kui_statusbar_t* statusbar;
    kui_toolbar_t* toolbar;
    kui_msg_handler_fn message_handler;
    void* user_data;
    int display_layer_id;
    kui_color_t title_bar_color;
    kui_color_t client_area_color;
    int title_bar_height;
    int border_width;
    kui_window_t* next;
    kui_window_t* prev;
    kui_window_t* parent;
    kui_window_t* first_child;
    kui_window_t* last_child;
    uint32_t* back_buffer;
    uint32_t* front_buffer;
    int buffer_stride;
    kui_rect_t dirty_rects[KANVASUI_MAX_DIRTY_RECTS];
    int dirty_count;
};

typedef struct {
    int screen_width;
    int screen_height;
    int bpp;
    uint32_t* framebuffer;
    int stride;
    kui_window_t* window_list;
    kui_window_t* focused_window;
    kui_window_t* capture_window;
    kui_window_t* modal_window;
    kui_window_t* desktop_window;
    kui_theme_t theme;
    kui_theme_t theme_cache[KANVASUI_THEME_CACHE_SIZE];
    int theme_cache_count;
    kui_component_t* component_pool[KANVASUI_MAX_COMPONENTS];
    int component_pool_count;
    kui_animator_t animators[KANVASUI_ANIMATOR_POOL];
    int animator_count;
    kui_tooltip_t* active_tooltips[KANVASUI_MAX_TOOLTIPS];
    int tooltip_count;
    uint32_t next_component_id;
    uint32_t next_window_id;
    kui_point_t mouse_pos;
    kui_component_t* hover_component;
    kui_component_t* focus_component;
    kui_component_t* capture_component;
    bool initialized;
} kui_context_t;

int kui_init(int screen_w, int screen_h, int bpp, uint32_t* fb, int stride);
void kui_shutdown(void);

const kui_theme_t* kui_get_theme(void);
void kui_set_theme(const kui_theme_t* theme);
void kui_apply_theme_recursive(kui_window_t* win);

kui_window_t* kui_create_window(const char* title, int x, int y, int w, int h,
                                 bool resizable, bool decorated);
void kui_destroy_window(kui_window_t* win);
void kui_show_window(kui_window_t* win);
void kui_hide_window(kui_window_t* win);
void kui_focus_window(kui_window_t* win);
void kui_minimize_window(kui_window_t* win);
void kui_maximize_window(kui_window_t* win);
void kui_restore_window(kui_window_t* win);
void kui_set_window_title(kui_window_t* win, const char* title);
void kui_set_window_rect(kui_window_t* win, int x, int y, int w, int h);
bool kui_is_window_visible(kui_window_t* win);
bool kui_is_window_focused(kui_window_t* win);

kui_component_t* kui_add_component(kui_window_t* win, kui_component_t* comp);
void kui_remove_component(kui_window_t* win, kui_component_t* comp);
void kui_destroy_component(kui_component_t* comp);
kui_component_t* kui_find_component_by_id(uint32_t id);

kui_button_t* kui_create_button(const char* text, kui_rect_t bounds, void (*on_click)(kui_button_t*));
kui_label_t* kui_create_label(const char* text, kui_rect_t bounds, int alignment);
kui_textbox_t* kui_create_textbox(kui_rect_t bounds, int max_chars, void (*on_changed)(kui_textbox_t*));
kui_progressbar_t* kui_create_progressbar(kui_rect_t bounds, int min_val, int max_val);
kui_slider_t* kui_create_slider(kui_rect_t bounds, int min_val, int max_val, void (*on_changed)(kui_slider_t*, int));
kui_checkbox_t* kui_create_checkbox(const char* label, kui_rect_t bounds, void (*on_changed)(kui_checkbox_t*, bool));
kui_listbox_t* kui_create_listbox(kui_rect_t bounds, int visible_items, void (*on_sel)(kui_listbox_t*, int));
kui_panel_t* kui_create_panel(kui_rect_t bounds, kui_color_t bg);
kui_image_t* kui_create_image(kui_rect_t bounds, uint32_t* pixels, int img_w, int img_h);
kui_menubar_t* kui_create_menubar(kui_rect_t bounds);
kui_statusbar_t* kui_create_statusbar(kui_rect_t bounds);
kui_toolbar_t* kui_create_toolbar(kui_rect_t bounds, bool vertical);
kui_tabcontrol_t* kui_create_tabcontrol(kui_rect_t bounds);
kui_treeview_t* kui_create_treeview(kui_rect_t bounds);
kui_splitter_t* kui_create_splitter(kui_rect_t bounds, bool horizontal);
kui_tooltip_t* kui_create_tooltip(const char* text, kui_window_t* owner, int timeout_ms);

void kui_draw_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color);
void kui_draw_filled_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color);
void kui_draw_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, int radius, uint32_t color);
void kui_draw_rounded_rect_outline(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, int radius, uint32_t color);
void kui_draw_line(uint32_t* fb, int stride, int fb_w, int fb_h, int x0, int y0, int x1, int y1, uint32_t color);
void kui_draw_circle(uint32_t* fb, int stride, int fb_w, int fb_h, int cx, int cy, int r, uint32_t color, bool filled);
void kui_draw_ellipse(uint32_t* fb, int stride, int fb_w, int fb_h, int cx, int cy, int rx, int ry, uint32_t color, bool filled);
void kui_draw_text(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, const char* text, uint32_t color, int font_size, int bold);
void kui_draw_text_clipped(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, const char* text, uint32_t color, int font_size, int bold, int clip_x, int clip_y, int clip_w, int clip_h);
void kui_draw_icon(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int icon_id, int size, uint32_t color);
void kui_draw_checkmark(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int size, uint32_t color);
void kui_draw_shadow(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, int offset_x, int offset_y, int blur, uint8_t alpha);
void kui_draw_gradient_v(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color_top, uint32_t color_bottom);
void kui_draw_gradient_h(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color_left, uint32_t color_right);
void kui_draw_alpha_rect(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void kui_draw_image(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, uint32_t* img, int img_w, int img_h, int img_stride);
void kui_draw_image_scaled(uint32_t* fb, int stride, int fb_w, int fb_h, int x, int y, int w, int h, uint32_t* img, int img_w, int img_h, int img_stride);
void kui_blit(uint32_t* dst, int dst_stride, int dst_x, int dst_y, uint32_t* src, int src_stride, int src_x, int src_y, int w, int h);
void kui_blit_alpha(uint32_t* dst, int dst_stride, int dst_x, int dst_y, uint32_t* src, int src_stride, int src_x, int src_y, int w, int h, uint8_t alpha);

bool kui_point_in_rect(int x, int y, kui_rect_t r);
bool kui_rect_intersect(kui_rect_t a, kui_rect_t b);
kui_rect_t kui_rect_union(kui_rect_t a, kui_rect_t b);
kui_rect_t kui_rect_clip(kui_rect_t a, kui_rect_t b);

void kui_invalidate(kui_component_t* comp);
void kui_invalidate_rect(kui_window_t* win, kui_rect_t rect);
void kui_invalidate_all(kui_window_t* win);

void kui_process_mouse_move(int x, int y);
void kui_process_mouse_button(int button, bool down, int x, int y);
void kui_process_scroll(int delta, int x, int y);
void kui_process_key(int key, bool down, uint32_t modifiers);
void kui_process_char(uint32_t ch);

void kui_render(void);
void kui_render_window(kui_window_t* win);
void kui_present(void);
void kui_update(void);

kui_animator_t* kui_animate(kui_component_t* target, float from, float to, uint64_t duration_ms, void (*on_update)(kui_animator_t*, float));
void kui_animator_update_all(uint64_t now_ms);
void kui_animator_cancel(kui_animator_t* anim);

void kui_show_tooltip(kui_tooltip_t* tip);
void kui_hide_tooltip(kui_tooltip_t* tip);
void kui_update_tooltips(uint64_t now_ms);

kui_context_t* kui_get_context(void);

int kui_window_init(void);

void kui_button_set_draw(kui_button_t* btn);
void kui_label_set_draw(kui_label_t* lbl);
void kui_textbox_set_draw(kui_textbox_t* tb);
void kui_progressbar_set_draw(kui_progressbar_t* pb);
void kui_slider_set_draw(kui_slider_t* sl);
void kui_checkbox_set_draw(kui_checkbox_t* cb);
void kui_listbox_set_draw(kui_listbox_t* lb);
void kui_panel_set_draw(kui_panel_t* p);
void kui_image_set_draw(kui_image_t* img);
void kui_menubar_set_draw(kui_menubar_t* mb);
void kui_statusbar_set_draw(kui_statusbar_t* sb);
void kui_toolbar_set_draw(kui_toolbar_t* tb);
void kui_tabcontrol_set_draw(kui_tabcontrol_t* tc);
void kui_treeview_set_draw(kui_treeview_t* tv);
void kui_splitter_set_draw(kui_splitter_t* sp);
void kui_tooltip_set_draw(kui_tooltip_t* tt);

void kui_dispatch_event(kui_window_t* win, kui_msg_type_t msg, uint64_t p1, uint64_t p2);

uint32_t kui_col32(kui_color_t c);

#ifdef __cplusplus
}
#endif

#endif