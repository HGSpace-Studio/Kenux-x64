#include <arch/framebuffer.h>
#include <string.h>
#include <arch/spinlock.h>

#define STARDUSTUI_MAX_WINDOWS 32
#define STARDUSTUI_MAX_COMPONENTS 256
#define STARDUSTUI_MAX_FONTS 16
#define STARDUSTUI_THEME_CACHE_SIZE 1024

typedef enum {
    WINDOW_MSG_MOUSE_MOVE = 1,
    WINDOW_MSG_LEFT_BUTTON_DOWN,
    WINDOW_MSG_LEFT_BUTTON_UP,
    WINDOW_MSG_RIGHT_BUTTON_DOWN,
    WINDOW_MSG_RIGHT_BUTTON_UP,
    WINDOW_MSG_MIDDLE_BUTTON_DOWN,
    WINDOW_MSG_MIDDLE_BUTTON_UP,
    WINDOW_MSG_SCROLL_UP,
    WINDOW_MSG_SCROLL_DOWN,
    WINDOW_MSG_KEY_DOWN,
    WINDOW_MSG_KEY_UP,
    WINDOW_MSG_CHAR_INPUT,
    WINDOW_MSG_RESIZE,
    WINDOW_MSG_MOVE,
    WINDOW_MSG_FOCUS_GAINED,
    WINDOW_MSG_FOCUS_LOST,
    WINDOW_MSG_CLOSE_REQUEST,
    WINDOW_MSG_PAINT,
    WINDOW_MSG_TIMER,
    WINDOW_MSG_CUSTOM
} window_message_type_t;

typedef struct point {
    int x, y;
} point_t;

typedef struct rect {
    int x, y, width, height;
} rect_t;

typedef struct color {
    uint8_t r, g, b, a;
} color_t;

typedef struct stardustui_theme {
    color_t background;
    color_t foreground;
    color_t accent;
    color_t text_primary;
    color_t text_secondary;
    color_t border;
    color_t shadow;
    color_t highlight;
    color_t disabled;
    color_t error;
    color_t success;
    color_t warning;
    int corner_radius;
    int shadow_offset_x;
    int shadow_offset_y;
    int border_width;
    int padding;
    int margin;
    char font_name[64];
    int font_size;
} stardustui_theme_t;

typedef struct component_base {
    uint32_t id;
    rect_t bounds;
    bool visible;
    bool enabled;
    bool focused;
    bool hovered;
    bool needs_redraw;
    void (*update)(struct component_base* comp);
    bool (*handle_pointer_move)(struct component_base* comp, int x, int y);
    bool (*handle_left_button)(struct component_base* comp, bool down, int x, int y);
    bool (*handle_right_button)(struct component_base* comp, bool down, int x, int y);
    bool (*handle_scroll)(struct component_base* comp, int delta);
    bool (*handle_key)(struct component_base* comp, int key, bool down);
    void (*draw)(struct component_base* comp, uint32_t* buffer, int stride);
    void (*on_parent_resize)(struct component_base* comp, int old_w, int old_h, int new_w, int new_h);
    void* user_data;
    char name[64];
    struct window_context* parent_window;
} component_base_t;

typedef struct button_component {
    component_base_t base;
    const char* text;
    color_t bg_color;
    color_t fg_color;
    color_t hover_color;
    color_t pressed_color;
    color_t disabled_color;
    bool is_pressed;
    bool is_hovered;
    int icon_id;
    void (*on_click)(struct button_component* btn);
} button_component_t;

typedef struct label_component {
    component_base_t base;
    char* text;
    color_t text_color;
    int alignment;
    bool word_wrap;
    int max_lines;
} label_component_t;

typedef struct textbox_component {
    component_base_t base;
    char* text;
    size_t text_capacity;
    size_t cursor_pos;
    size_t selection_start;
    size_t selection_end;
    bool read_only;
    bool password_mode;
    char password_char;
    int max_chars;
    void (*on_text_changed)(struct textbox_component* tb);
    void (*on_enter_pressed)(struct textbox_component* tb);
} textbox_component_t;

typedef struct progressbar_component {
    component_base_t base;
    int min_value;
    int max_value;
    int current_value;
    color_t bar_color;
    color_t background_color;
    bool show_percentage;
    bool animated;
    float animation_progress;
} progressbar_component_t;

typedef struct slider_component {
    component_base_t base;
    int min_value;
    int max_value;
    int current_value;
    color_t track_color;
    color_t thumb_color;
    int thumb_size;
    bool vertical;
    void (*on_value_changed)(struct slider_component* slider, int value);
} slider_component_t;

typedef struct checkbox_component {
    component_base_t base;
    bool checked;
    color_t check_color;
    color_t box_color;
    const char* label;
    void (*on_state_changed)(struct checkbox_component* cb, bool checked);
} checkbox_component_t;

typedef struct listbox_item {
    const char* text;
    void* data;
    bool selected;
    bool enabled;
} listbox_item_t;

typedef struct listbox_component {
    component_base_t base;
    listbox_item_t* items;
    int item_count;
    int capacity;
    int selected_index;
    int scroll_offset;
    int visible_items;
    color_t item_bg_color;
    color_t item_selected_color;
    color_t item_text_color;
    color_t item_selected_text_color;
    bool multi_select;
    void (*on_selection_changed)(struct listbox_component* lb, int index);
    void (*on_item_double_click)(struct listbox_component* lb, int index);
} listbox_component_t;

typedef struct panel_component {
    component_base_t base;
    component_base_t** children;
    int child_count;
    int capacity;
    color_t background_color;
    int border_width;
    color_t border_color;
    int corner_radius;
    bool scrollable;
    int scroll_x;
    int scroll_y;
} panel_component_t;

typedef struct image_component {
    component_base_t base;
    uint32_t* pixel_data;
    int image_width;
    int image_height;
    int source_x;
    int source_y;
    bool stretch_to_fit;
    bool maintain_aspect_ratio;
    color_t tint_color;
    float alpha;
} image_component_t;

typedef struct menu_item {
    const char* text;
    int icon_id;
    bool enabled;
    bool checked;
    bool separator;
    struct menu_item* submenu;
    int submenu_item_count;
    void (*on_selected)(struct menu_item* item);
} menu_item_t;

typedef struct menubar_component {
    component_base_t base;
    menu_item_t** menus;
    int menu_count;
    int open_menu_index;
    int highlighted_item_index;
    color_t bg_color;
    color_t text_color;
    color_t highlight_color;
    color_t border_color;
} menubar_component_t;

typedef struct statusbar_component {
    component_base_t base;
    char** sections;
    int section_count;
    color_t bg_color;
    color_t text_color;
    color_t border_color;
} statusbar_component_t;

typedef struct toolbar_button {
    const char* tooltip;
    int icon_id;
    bool enabled;
    bool pressed;
    bool toggleable;
    bool toggled;
    void (*on_click)(struct toolbar_button* btn);
} toolbar_button_t;

typedef struct toolbar_component {
    component_base_t base;
    toolbar_button_t* buttons;
    int button_count;
    bool vertical;
    color_t bg_color;
    color_t button_color;
    color_t hover_color;
    color_t pressed_color;
    int button_size;
    int spacing;
} toolbar_component_t;

typedef struct tab_item {
    const char* title;
    int icon_id;
    component_base_t* content;
    bool closable;
    bool visible;
} tab_item_t;

typedef struct tabcontrol_component {
    component_base_t base;
    tab_item_t* tabs;
    int tab_count;
    int active_tab_index;
    color_t tab_bg_color;
    color_t tab_active_color;
    color_t tab_text_color;
    color_t tab_active_text_color;
    color_t border_color;
    void (*on_tab_changed)(struct tabcontrol_component* tc, int index);
} tabcontrol_component_t;

typedef struct treeview_node {
    const char* text;
    int icon_id;
    bool expanded;
    bool selected;
    bool has_children;
    void* data;
    struct treeview_node* parent;
    struct treeview_node** children;
    int child_count;
} treeview_node_t;

typedef struct treeview_component {
    component_base_t base;
    treeview_node_t* root_nodes;
    int root_node_count;
    treeview_node_t* selected_node;
    int indent_size;
    color_t node_text_color;
    color_t node_selected_color;
    color_t line_color;
    void (*on_node_selected)(struct treeview_component* tv, treeview_node_t* node);
    void (*node_expanded)(struct treeview_component* tv, treeview_node_t* node);
} treeview_component_t;

typedef struct table_column {
    const char* title;
    int width;
    bool resizable;
    bool sortable;
    sort_direction_t sort_dir;
} table_column_t;

typedef struct table_component {
    component_base_t base;
    table_column_t* columns;
    int column_count;
    void*** cell_data;
    int row_count;
    int col_count;
    int selected_row;
    int header_height;
    int row_height;
    color_t header_bg_color;
    color_t header_text_color;
    color_t row_alt_color;
    color_t selected_row_color;
    color_t grid_color;
    bool show_grid;
    bool selectable_rows;
    void (*on_row_selected)(struct table_component* tbl, int row);
    void (*on_cell_clicked)(struct table_component* tbl, int row, int col);
} table_component_t;

typedef struct splitcontainer_component {
    component_base_t base;
    component_base_t* pane1;
    component_base_t* pane2;
    bool horizontal;
    int splitter_position;
    int splitter_size;
    color_t splitter_color;
    int min_pane1_size;
    int min_pane2_size;
    void (*on_splitter_moved)(struct splitcontainer_component* sc, int position);
} splitcontainer_component_t;

typedef struct window_context {
    uint32_t id;
    char title[256];
    rect_t client_rect;
    bool visible;
    bool focused;
    bool minimized;
    bool maximized;
    bool resizable;
    bool decorated;
    bool modal;
    bool topmost;
    bool fullscreen;
    component_base_t** components;
    int component_count;
    int component_capacity;
    menubar_component_t* menubar;
    statusbar_component_t* statusbar;
    toolbar_component_t* toolbar;
    void (*message_handler)(window_context_t* win, window_message_type_t msg,
                            uint64_t param1, uint64_t param2);
    void* user_data;
    int display_layer_id;
    color_t title_bar_color;
    color_t client_area_color;
    int title_bar_height;
    int border_width;
    spinlock_t lock;
} window_context_t;

static window_context_t windows[STARDUSTUI_MAX_WINDOWS];
static component_base_t* all_components[STARDUSTUI_MAX_COMPONENTS];
static stardustui_theme_t current_theme;
static stardustui_theme_t theme_cache[STARDUSTUI_THEME_CACHE_SIZE];
static int theme_cache_count = 0;
static uint32_t next_component_id = 1;
static uint32_t next_window_id = 1;
static spinlock_t ui_lock;
static int initialized = 0;

void stardustui_init(void)
{
    if (initialized) return;

    spin_init(&ui_lock);

    memset(windows, 0, sizeof(windows));
    memset(all_components, 0, sizeof(all_components));
    memset(&current_theme, 0, sizeof(stardustui_theme_t));

    current_theme.background = (color_t){240, 240, 245, 255};
    current_theme.foreground = (color_t){30, 30, 30, 255};
    current_theme.accent = (color_t){66, 133, 244, 255};
    current_theme.text_primary = (color_t){30, 30, 30, 255};
    current_theme.text_secondary = (color_t){100, 100, 100, 255};
    current_theme.border = (color_t){200, 200, 200, 255};
    current_theme.shadow = (color_t){0, 0, 0, 50};
    current_theme.highlight = (color_t){232, 240, 254, 255};
    current_theme.disabled = (color_t}{180, 180, 180, 255};
    current_theme.error = (color_t){234, 67, 53, 255};
    current_theme.success = (color_t){52, 168, 83, 255};
    current_theme.warning = (color_t){251, 188, 4, 255};

    current_theme.corner_radius = 8;
    current_theme.shadow_offset_x = 2;
    current_theme.shadow_offset_y = 2;
    current_theme.border_width = 1;
    current_theme.padding = 8;
    current_theme.margin = 8;
    strncpy(current_theme.font_name, "Segoe UI", 63);
    current_theme.font_size = 14;

    initialized = 1;
}

const stardustui_theme_t* stardustui_get_theme(void)
{
    return &current_theme;
}

void stardustui_set_theme(const stardustui_theme_t* theme)
{
    if (!theme || !initialized) return;

    spin_lock(&ui_lock);
    memcpy(&current_theme, theme, sizeof(stardustui_theme_t));

    if (theme_cache_count < STARDUSTUI_THEME_CACHE_SIZE) {
        memcpy(&theme_cache[theme_cache_count++], theme, sizeof(stardustui_theme_t));
    }
    spin_unlock(&ui_lock);
}

window_context_t* stardustui_create_window(const char* title, int width, int height,
                                           bool resizable, bool decorated)
{
    if (!initialized || !title || width <= 0 || height <= 0) return NULL;

    spin_lock(&ui_lock);

    int win_idx = -1;
    for (int i = 0; i < STARDUSTUI_MAX_WINDOWS; i++) {
        if (windows[i].id == 0) {
            win_idx = i;
            break;
        }
    }

    if (win_idx < 0) {
        spin_unlock(&ui_lock);
        return NULL;
    }

    window_context_t* win = &windows[win_idx];
    memset(win, 0, sizeof(window_context_t));

    win->id = next_window_id++;
    strncpy(win->title, title, 255);
    win->client_rect.x = 100 + (win_idx * 30) % 400;
    win->client_rect.y = 100 + (win_idx * 30) % 300;
    win->client_rect.width = width;
    win->client_rect.height = height;
    win->visible = true;
    win->focused = false;
    win->resizable = resizable;
    win->decorated = decorated;
    win->component_capacity = 32;
    win->components = kzalloc(sizeof(component_base_t*) * win->component_capacity);
    win->display_layer_id = display_create_layer(width, height, title);
    spin_init(&win->lock);

    win->title_bar_color = current_theme.accent;
    win->client_area_color = current_theme.background;
    win->title_bar_height = decorated ? 32 : 0;
    win->border_width = decorated ? 3 : 0;

    spin_unlock(&ui_lock);
    return win;
}

void stardustui_destroy_window(window_context_t* win)
{
    if (!win || !initialized || win->id == 0) return;

    spin_lock(&ui_lock);

    for (int i = 0; i < win->component_count; i++) {
        if (win->components[i]) {
            stardustui_destroy_component(win->components[i]);
            win->components[i] = NULL;
        }
    }

    kfree(win->components);
    display_destroy_layer(win->display_layer_id);

    memset(win, 0, sizeof(window_context_t));

    spin_unlock(&ui_lock);
}

void stardustui_show_window(window_context_t* win)
{
    if (!win || !initialized) return;

    spin_lock(&win->lock);
    win->visible = true;
    display_set_layer_enabled(win->display_layer_id, true);
    spin_unlock(&win->lock);
}

void stardustui_hide_window(window_context_t* win)
{
    if (!win || !initialized) return;

    spin_lock(&win->lock);
    win->visible = false;
    display_set_layer_enabled(win->display_layer_id, false);
    spin_unlock(&win->lock);
}

void stardustui_focus_window(window_context_t* win)
{
    if (!win || !initialized) return;

    spin_lock(&ui_lock);

    for (int i = 0; i < STARDUSTUI_MAX_WINDOWS; i++) {
        if (windows[i].id != 0 && &windows[i] != win) {
            windows[i].focused = false;
        }
    }

    win->focused = true;
    display_focus_window(win->display_layer_id);

    spin_unlock(&ui_lock);
}

bool stardustui_is_window_visible(window_context_t* win)
{
    return win ? win->visible : false;
}

bool stardustui_is_window_focused(window_context_t* win)
{
    return win ? win->focused : false;
}

component_base_t* stardustui_add_component(window_context_t* win, component_base_t* comp)
{
    if (!win || !comp || !initialized) return NULL;

    spin_lock(&win->lock);

    if (win->component_count >= win->component_capacity) {
        int new_cap = win->component_capacity * 2;
        component_base_t** new_comps = krealloc(win->components,
                                                sizeof(component_base_t*) * new_cap);
        if (!new_comps) {
            spin_unlock(&win->lock);
            return NULL;
        }
        win->components = new_comps;
        win->component_capacity = new_cap;
    }

    comp->id = next_component_id++;
    comp->parent_window = win;
    win->components[win->component_count++] = comp;

    spin_unlock(&win->lock);
    return comp;
}

void stardustui_remove_component(window_context_t* win, component_base_t* comp)
{
    if (!win || !comp || !initialized) return;

    spin_lock(&win->lock);

    for (int i = 0; i < win->component_count; i++) {
        if (win->components[i] == comp) {
            stardustui_destroy_component(comp);
            win->components[i] = NULL;

            for (int j = i; j < win->component_count - 1; j++) {
                win->components[j] = win->components[j + 1];
            }
            win->component_count--;
            break;
        }
    }

    spin_unlock(&win->lock);
}

button_component_t* stardustui_create_button(const char* text, rect_t bounds,
                                             void (*on_click)(button_component_t*))
{
    if (!text || !initialized) return NULL;

    button_component_t* btn = kzalloc(sizeof(button_component_t));
    if (!btn) return NULL;

    btn->base.bounds = bounds;
    btn->base.visible = true;
    btn->base.enabled = true;
    btn->base.needs_redraw = true;
    strncpy(btn->base.name, "Button", 63);

    btn->text = text;
    btn->bg_color = current_theme.accent;
    btn->fg_color = (color_t){255, 255, 255, 255};
    btn->hover_color = (color_t){(uint8_t)(current_theme.accent.r * 0.9),
                                 (uint8_t)(current_theme.accent.g * 0.9),
                                 (uint8_t)(current_theme.accent.b * 0.9), 255};
    btn->pressed_color = (color_t){(uint8_t)(current_theme.accent.r * 0.7),
                                   (uint8_t)(current_theme.accent.g * 0.7),
                                   (uint8_t)(current_theme.accent.b * 0.7), 255};
    btn->disabled_color = current_theme.disabled;
    btn->is_pressed = false;
    btn->is_hovered = false;
    btn->icon_id = -1;
    btn->on_click = on_click;

    btn->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        button_component_t* btn = (button_component_t*)comp;
        rect_t b = comp->bounds;

        color_t fill_color = btn->disabled_color;
        if (comp->enabled) {
            if (btn->is_pressed) fill_color = btn->pressed_color;
            else if (btn->is_hovered) fill_color = btn->hover_color;
            else fill_color = btn->bg_color;
        }

        draw_rounded_rect(buffer, stride, b.x, b.y, b.width, b.height,
                         current_theme.corner_radius, color_to_uint32(fill_color));

        if (btn->icon_id >= 0) {
            draw_icon(buffer, stride, b.x + 8, b.y + b.height / 2 - 8,
                     btn->icon_id, 16, color_to_uint32(btn->fg_color));
        }

        if (btn->text) {
            int text_x = b.x + (b.width - strlen(btn->text) * 8) / 2;
            int text_y = b.y + (b.height - 16) / 2;
            draw_text(buffer, stride, text_x, text_y, btn->text,
                     color_to_uint32(btn->fg_color), 0, 1);
        }
    };

    btn->base.handle_left_button = [](component_base_t* comp, bool down, int x, int y) -> bool {
        button_component_t* btn = (button_component_t*)comp;
        if (point_in_rect(x, y, comp->bounds)) {
            btn->is_pressed = down;
            if (!down && btn->on_click && comp->enabled) {
                btn->on_click(btn);
            }
            comp->needs_redraw = true;
            return true;
        }
        return false;
    };

    btn->base.handle_pointer_move = [](component_base_t* comp, int x, int y) -> bool {
        button_component_t* btn = (button_component_t*)comp;
        bool was_hovered = btn->is_hovered;
        btn->is_hovered = point_in_rect(x, y, comp->bounds);
        if (was_hovered != btn->is_hovered) {
            comp->needs_redraw = true;
        }
        return btn->is_hovered;
    };

    return btn;
}

label_component_t* stardustui_create_label(const char* text, rect_t bounds, int alignment)
{
    if (!text || !initialized) return NULL;

    label_component_t* lbl = kzalloc(sizeof(label_component_t));
    if (!lbl) return NULL;

    lbl->base.bounds = bounds;
    lbl->base.visible = true;
    lbl->base.enabled = true;
    lbl->base.needs_redraw = true;
    strncpy(lbl->base.name, "Label", 63);

    lbl->text = kstrdup(text);
    lbl->text_color = current_theme.text_primary;
    lbl->alignment = alignment;
    lbl->word_wrap = false;
    lbl->max_lines = 0;

    lbl->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        label_component_t* lbl = (label_component_t*)comp;
        rect_t b = comp->bounds;

        if (!lbl->text) return;

        int text_len = strlen(lbl->text);
        int text_x = b.x;
        int text_y = b.y;

        switch (lbl->alignment) {
            case 1: text_x += (b.width - text_len * 8) / 2; break;
            case 2: text_x += b.width - text_len * 8; break;
            default: break;
        }

        draw_text(buffer, stride, text_x, text_y, lbl->text,
                 color_to_uint32(lbl->text_color), 0, 1);
    };

    return lbl;
}

textbox_component_t* stardustui_create_textbox(rect_t bounds, int max_chars,
                                               void (*on_text_changed)(textbox_component_t*))
{
    if (!initialized) return NULL;

    textbox_component_t* tb = kzalloc(sizeof(textbox_component_t));
    if (!tb) return NULL;

    tb->base.bounds = bounds;
    tb->base.visible = true;
    tb->base.enabled = true;
    tb->base.focused = false;
    tb->base.needs_redraw = true;
    strncpy(tb->base.name, "TextBox", 63);

    tb->text_capacity = max_chars > 0 ? max_chars : 256;
    tb->text = kzalloc(tb->text_capacity);
    tb->cursor_pos = 0;
    tb->selection_start = 0;
    tb->selection_end = 0;
    tb->read_only = false;
    tb->password_mode = false;
    tb->password_char = '*';
    tb->max_chars = max_chars;
    tb->on_text_changed = on_text_changed;

    tb->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        textbox_component_t* tb = (textbox_component_t*)comp;
        rect_t b = comp->bounds;

        draw_rounded_rect(buffer, stride, b.x, b.y, b.width, b.height,
                         4, color_to_uint32((color_t){255, 255, 255, 255}));

        draw_rect(buffer, stride, b.x, b.y, b.width, b.height,
                 color_to_uint32(current_theme.border));

        const char* display_text = tb->password_mode ? "" : tb->text;
        if (tb->password_mode && tb->text) {
            static char masked[256];
            int len = strlen(tb->text);
            for (int i = 0; i < len && i < 255; i++) masked[i] = tb->password_char;
            masked[len] = '\0';
            display_text = masked;
        }

        if (display_text) {
            draw_text(buffer, stride, b.x + 4, b.y + (b.height - 16) / 2,
                     display_text, color_to_uint32(current_theme.text_primary), 0, 1);
        }

        if (comp->focused) {
            int cursor_x = b.x + 4 + tb->cursor_pos * 8;
            draw_line(buffer, stride, cursor_x, b.y + 4, cursor_x, b.y + b.height - 4,
                     color_to_uint32(current_theme.foreground));
        }
    };

    tb->base.handle_left_button = [](component_base_t* comp, bool down, int x, int y) -> bool {
        textbox_component_t* tb = (textbox_component_t*)comp;
        if (point_in_rect(x, y, comp->bounds)) {
            if (down) {
                comp->focused = true;
                int rel_x = x - comp->bounds.x - 4;
                tb->cursor_pos = rel_x / 8;
                if (tb->cursor_pos > strlen(tb->text)) {
                    tb->cursor_pos = strlen(tb->text);
                }
                tb->selection_start = tb->cursor_pos;
                tb->selection_end = tb->cursor_pos;
            }
            comp->needs_redraw = true;
            return true;
        } else if (down) {
            comp->focused = false;
            comp->needs_redraw = true;
        }
        return false;
    };

    tb->base.handle_key = [](component_base_t* comp, int key, bool down) -> bool {
        textbox_component_t* tb = (textbox_component_t*)comp;
        if (!comp->focused || !down || tb->read_only) return false;

        if (key >= 32 && key < 127) {
            if (strlen(tb->text) < tb->max_chars) {
                size_t len = strlen(tb->text);
                memmove(&tb->text[tb->cursor_pos + 1], &tb->text[tb->cursor_pos],
                       len - tb->cursor_pos + 1);
                tb->text[tb->cursor_pos] = (char)key;
                tb->cursor_pos++;
                if (tb->on_text_changed) tb->on_text_changed(tb);
                comp->needs_redraw = true;
            }
            return true;
        } else if (key == 0x08 && tb->cursor_pos > 0) {
            size_t len = strlen(tb->text);
            memmove(&tb->text[tb->cursor_pos - 1], &tb->text[tb->cursor_pos],
                   len - tb->cursor_pos + 1);
            tb->cursor_pos--;
            if (tb->on_text_changed) tb->on_text_changed(tb);
            comp->needs_redraw = true;
            return true;
        } else if (key == 0x0D) {
            if (tb->on_enter_pressed) tb->on_enter_pressed(tb);
            return true;
        }
        return false;
    };

    return tb;
}

progressbar_component_t* stardustui_create_progressbar(rect_t bounds, int min_val, int max_val)
{
    if (!initialized) return NULL;

    progressbar_component_t* pb = kzalloc(sizeof(progressbar_component_t));
    if (!pb) return NULL;

    pb->base.bounds = bounds;
    pb->base.visible = true;
    pb->base.enabled = true;
    pb->base.needs_redraw = true;
    strncpy(pb->base.name, "ProgressBar", 63);

    pb->min_value = min_val;
    pb->max_value = max_val;
    pb->current_value = min_val;
    pb->bar_color = current_theme.accent;
    pb->background_color = (color_t){230, 230, 230, 255};
    pb->show_percentage = true;
    pb->animated = true;
    pb->animation_progress = 0.0f;

    pb->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        progressbar_component_t* pb = (progressbar_component_t*)comp;
        rect_t b = comp->bounds;

        draw_rounded_rect(buffer, stride, b.x, b.y, b.width, b.height,
                         4, color_to_uint32(pb->background_color));

        float percent = (float)(pb->current_value - pb->min_value) /
                       (float)(pb->max_value - pb->min_value);
        if (pb->max_value == pb->min_value) percent = 0.0f;

        int fill_width = (int)((b.width - 4) * percent);
        if (fill_width > 0) {
            draw_rounded_rect(buffer, stride, b.x + 2, b.y + 2,
                             fill_width, b.height - 4,
                             3, color_to_uint32(pb->bar_color));
        }

        if (pb->show_percentage) {
            char pct_str[16];
            snprintf(pct_str, sizeof(pct_str), "%d%%", (int)(percent * 100));
            int text_w = strlen(pct_str) * 8;
            draw_text(buffer, stride, b.x + (b.width - text_w) / 2,
                     b.y + (b.height - 16) / 2, pct_str,
                     color_to_uint32(current_theme.text_primary), 0, 1);
        }
    };

    return pb;
}

slider_component_t* stardustui_create_slider(rect_t bounds, int min_val, int max_val,
                                            void (*on_value_changed)(slider_component_t*, int))
{
    if (!initialized) return NULL;

    slider_component_t* sl = kzalloc(sizeof(slider_component_t));
    if (!sl) return NULL;

    sl->base.bounds = bounds;
    sl->base.visible = true;
    sl->base.enabled = true;
    sl->base.needs_redraw = true;
    strncpy(sl->base.name, "Slider", 63);

    sl->min_value = min_val;
    sl->max_value = max_val;
    sl->current_value = min_val;
    sl->track_color = (color_t){200, 200, 200, 255};
    sl->thumb_color = current_theme.accent;
    sl->thumb_size = 16;
    sl->vertical = false;
    sl->on_value_changed = on_value_changed;

    sl->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        slider_component_t* sl = (slider_component_t*)comp;
        rect_t b = comp->bounds;

        int track_x, track_y, track_w, track_h;
        int thumb_x, thumb_y;

        if (sl->vertical) {
            track_x = b.x + b.width / 2 - 3;
            track_y = b.y + sl->thumb_size / 2;
            track_w = 6;
            track_h = b.height - sl->thumb_size;
            thumb_x = b.x + (b.width - sl->thumb_size) / 2;
            thumb_y = track_y + (int)((float)track_h *
                      (float)(sl->current_value - sl->min_value) /
                      (float)(sl->max_value - sl->min_value));
        } else {
            track_x = b.x + sl->thumb_size / 2;
            track_y = b.y + b.height / 2 - 3;
            track_w = b.width - sl->thumb_size;
            track_h = 6;
            thumb_x = track_x + (int)((float)track_w *
                     (float)(sl->current_value - sl->min_value) /
                     (float)(sl->max_value - sl->min_value));
            thumb_y = b.y + (b.height - sl->thumb_size) / 2;
        }

        draw_rounded_rect(buffer, stride, track_x, track_y, track_w, track_h,
                         3, color_to_uint32(sl->track_color));

        draw_circle(buffer, stride, thumb_x + sl->thumb_size / 2,
                   thumb_y + sl->thumb_size / 2, sl->thumb_size / 2,
                   color_to_uint32(sl->thumb_color), true);
    };

    sl->base.handle_left_button = [](component_base_t* comp, bool down, int x, int y) -> bool {
        slider_component_t* sl = (slider_component_t*)comp;
        if (point_in_rect(x, y, comp->bounds) && down) {
            if (sl->vertical) {
                int rel_y = y - comp->bounds.y - sl->thumb_size / 2;
                int range = comp->bounds.height - sl->thumb_size;
                sl->current_value = sl->min_value +
                    (int)((float)rel_y / (float)range * (float)(sl->max_value - sl->min_value));
            } else {
                int rel_x = x - comp->bounds.x - sl->thumb_size / 2;
                int range = comp->bounds.width - sl->thumb_size;
                sl->current_value = sl->min_value +
                    (int)((float)rel_x / (float)range * (float)(sl->max_value - sl->min_value));
            }

            if (sl->current_value < sl->min_value) sl->current_value = sl->min_value;
            if (sl->current_value > sl->max_value) sl->current_value = sl->max_value;

            if (sl->on_value_changed) sl->on_value_changed(sl, sl->current_value);
            comp->needs_redraw = true;
            return true;
        }
        return false;
    };

    return sl;
}

checkbox_component_t* stardustui_create_checkbox(const char* label, rect_t bounds,
                                               void (*on_state_changed)(checkbox_component_t*, bool))
{
    if (!initialized) return NULL;

    checkbox_component_t* cb = kzalloc(sizeof(checkbox_component_t));
    if (!cb) return NULL;

    cb->base.bounds = bounds;
    cb->base.visible = true;
    cb->base.enabled = true;
    cb->base.needs_redraw = true;
    strncpy(cb->base.name, "CheckBox", 63);

    cb->checked = false;
    cb->check_color = current_theme.accent;
    cb->box_color = (color_t){200, 200, 200, 255};
    cb->label = label;
    cb->on_state_changed = on_state_changed;

    cb->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        checkbox_component_t* cb = (checkbox_component_t*)comp;
        rect_t b = comp->bounds;

        int box_size = 18;
        int box_x = b.x;
        int box_y = b.y + (b.height - box_size) / 2;

        draw_rounded_rect(buffer, stride, box_x, box_y, box_size, box_size,
                         3, color_to_uint32(cb->box_color));

        if (cb->checked) {
            draw_checkmark(buffer, stride, box_x + 3, box_y + 3, box_size - 6,
                          color_to_uint32(cb->check_color));
        }

        if (cb->label) {
            draw_text(buffer, stride, box_x + box_size + 6, b.y + (b.height - 16) / 2,
                     cb->label, color_to_uint32(current_theme.text_primary), 0, 1);
        }
    };

    cb->base.handle_left_button = [](component_base_t* comp, bool down, int x, int y) -> bool {
        checkbox_component_t* cb = (checkbox_component_t*)comp;
        if (point_in_rect(x, y, comp->bounds) && down && comp->enabled) {
            cb->checked = !cb->checked;
            if (cb->on_state_changed) cb->on_state_changed(cb, cb->checked);
            comp->needs_redraw = true;
            return true;
        }
        return false;
    };

    return cb;
}

listbox_component_t* stardustui_create_listbox(rect_t bounds, int visible_items,
                                              void (*on_sel_changed)(listbox_component_t*, int))
{
    if (!initialized || visible_items <= 0) return NULL;

    listbox_component_t* lb = kzalloc(sizeof(listbox_component_t));
    if (!lb) return NULL;

    lb->base.bounds = bounds;
    lb->base.visible = true;
    lb->base.enabled = true;
    lb->base.needs_redraw = true;
    strncpy(lb->base.name, "ListBox", 63);

    lb->capacity = visible_items + 10;
    lb->items = kzalloc(sizeof(listbox_item_t) * lb->capacity);
    lb->item_count = 0;
    lb->selected_index = -1;
    lb->scroll_offset = 0;
    lb->visible_items = visible_items;
    lb->item_bg_color = (color_t){255, 255, 255, 255};
    lb->item_selected_color = current_theme.highlight;
    lb->item_text_color = current_theme.text_primary;
    lb->item_selected_text_color = current_theme.text_primary;
    lb->multi_select = false;
    lb->on_selection_changed = on_sel_changed;

    lb->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        listbox_component_t* lb = (listbox_component_t*)comp;
        rect_t b = comp->bounds;

        draw_rounded_rect(buffer, stride, b.x, b.y, b.width, b.height,
                         4, color_to_uint32((color_t){255, 255, 255, 255}));
        draw_rect(buffer, stride, b.x, b.y, b.width, b.height,
                 color_to_uint32(current_theme.border));

        int item_height = 24;
        int start_idx = lb->scroll_offset;
        int end_idx = start_idx + lb->visible_items;
        if (end_idx > lb->item_count) end_idx = lb->item_count;

        for (int i = start_idx; i < end_idx; i++) {
            int item_y = b.y + (i - start_idx) * item_height;
            color_t bg = (i == lb->selected_index) ? lb->item_selected_color :
                                                     lb->item_bg_color;
            color_t fg = (i == lb->selected_index) ? lb->item_selected_text_color :
                                                     lb->item_text_color;

            draw_rect(buffer, stride, b.x + 1, item_y, b.width - 2, item_height,
                     color_to_uint32(bg));

            if (lb->items[i].text) {
                draw_text(buffer, stride, b.x + 4, item_y + (item_height - 16) / 2,
                         lb->items[i].text, color_to_uint32(fg), 0, 1);
            }
        }
    };

    lb->base.handle_left_button = [](component_base_t* comp, bool down, int x, int y) -> bool {
        listbox_component_t* lb = (listbox_component_t*)comp;
        if (point_in_rect(x, y, comp->bounds) && down) {
            int rel_y = y - comp->bounds.y;
            int item_idx = lb->scroll_offset + rel_y / 24;

            if (item_idx >= 0 && item_idx < lb->item_count) {
                lb->selected_index = item_idx;
                if (lb->on_selection_changed) lb->on_selection_changed(lb, item_idx);
                comp->needs_redraw = true;
            }
            return true;
        }
        return false;
    };

    lb->base.handle_scroll = [](component_base_t* comp, int delta) -> bool {
        listbox_component_t* lb = (listbox_component_t*)comp;
        int new_offset = lb->scroll_offset - delta;
        int max_offset = lb->item_count - lb->visible_items;
        if (max_offset < 0) max_offset = 0;

        if (new_offset < 0) new_offset = 0;
        if (new_offset > max_offset) new_offset = max_offset;

        if (new_offset != lb->scroll_offset) {
            lb->scroll_offset = new_offset;
            comp->needs_redraw = true;
            return true;
        }
        return false;
    };

    return lb;
}

void stardustui_listbox_add_item(listbox_component_t* lb, const char* text, void* data)
{
    if (!lb || !text || lb->item_count >= lb->capacity) return;

    listbox_item_t* item = &lb->items[lb->item_count++];
    item->text = text;
    item->data = data;
    item->selected = false;
    item->enabled = true;
    lb->base.needs_redraw = true;
}

panel_component_t* stardustui_create_panel(rect_t bounds, color_t bg_color)
{
    if (!initialized) return NULL;

    panel_component_t* pnl = kzalloc(sizeof(panel_component_t));
    if (!pnl) return NULL;

    pnl->base.bounds = bounds;
    pnl->base.visible = true;
    pnl->base.enabled = true;
    pnl->base.needs_redraw = true;
    strncpy(pnl->base.name, "Panel", 63);

    pnl->children = NULL;
    pnl->child_count = 0;
    pnl->capacity = 0;
    pnl->background_color = bg_color;
    pnl->border_width = 0;
    pnl->corner_radius = 0;
    pnl->scrollable = false;
    pnl->scroll_x = 0;
    pnl->scroll_y = 0;

    pnl->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        panel_component_t* pnl = (panel_component_t*)comp;
        rect_t b = comp->bounds;

        draw_rounded_rect(buffer, stride, b.x, b.y, b.width, b.height,
                         pnl->corner_radius, color_to_uint32(pnl->background_color));

        if (pnl->border_width > 0) {
            draw_rect(buffer, stride, b.x, b.y, b.width, b.height,
                     color_to_uint32(current_theme.border));
        }
    };

    return pnl;
}

image_component_t* stardustui_create_image(rect_t bounds, uint32_t* pixels,
                                          int width, int height)
{
    if (!initialized || !pixels || width <= 0 || height <= 0) return NULL;

    image_component_t* img = kzalloc(sizeof(image_component_t));
    if (!img) return NULL;

    img->base.bounds = bounds;
    img->base.visible = true;
    img->base.enabled = true;
    img->base.needs_redraw = true;
    strncpy(img->base.name, "Image", 63);

    img->pixel_data = pixels;
    img->image_width = width;
    img->image_height = height;
    img->source_x = 0;
    img->source_y = 0;
    img->stretch_to_fit = true;
    img->maintain_aspect_ratio = false;
    img->tint_color = (color_t){255, 255, 255, 255};
    img->alpha = 1.0f;

    img->base.draw = [](component_base_t* comp, uint32_t* buffer, int stride) {
        image_component_t* img = (image_component_t*)comp;
        rect_t b = comp->bounds;

        int src_w = img->image_width;
        int src_h = img->image_height;
        int dst_w = b.width;
        int dst_h = b.height;

        if (img->stretch_to_fit) {
            if (img->maintain_aspect_ratio) {
                float aspect = (float)src_w / (float)src_h;
                float target_aspect = (float)dst_w / (float)dst_h;
                if (aspect > target_aspect) {
                    dst_h = (int)((float)dst_w / aspect);
                } else {
                    dst_w = (int)((float)dst_h * aspect);
                }
            }
        } else {
            dst_w = src_w;
            dst_h = src_h;
        }

        int offset_x = b.x + (b.width - dst_w) / 2;
        int offset_y = b.y + (b.height - dst_h) / 2;

        for (int y = 0; y < dst_h && y + offset_y < b.y + b.height; y++) {
            for (int x = 0; x < dst_w && x + offset_x < b.x + b.width; x++) {
                int sx = img->source_x + (x * src_w / dst_w);
                int sy = img->source_y + (y * src_h / dst_h);

                if (sx < img->image_width && sy < img->image_height) {
                    uint32_t pixel = img->pixel_data[sy * img->image_width + sx];

                    uint8_t a = (pixel >> 24) & 0xFF;
                    uint8_t r = (pixel >> 16) & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t b_c = pixel & 0xFF;

                    r = (uint8_t)(r * img->tint_color.r / 255.0f);
                    g = (uint8_t)(g * img->tint_color.g / 255.0f);
                    b_c = (uint8_t)(b_c * img->tint_color.b / 255.0f);
                    a = (uint8_t)(a * img->alpha);

                    buffer[(offset_y + y) * stride + (offset_x + x)] =
                        (a << 24) | (r << 16) | (g << 8) | b_c;
                }
            }
        }
    };

    return img;
}

void stardustui_destroy_component(component_base_t* comp)
{
    if (!comp || !initialized) return;

    switch (comp->type) {
        case COMPONENT_LABEL: {
            label_component_t* lbl = (label_component_t*)comp;
            if (lbl->text) kfree(lbl->text);
            break;
        }
        case COMPONENT_TEXTBOX: {
            textbox_component_t* tb = (textbox_component_t*)comp;
            if (tb->text) kfree(tb->text);
            break;
        }
        case COMPONENT_LISTBOX: {
            listbox_component_t* lb = (listbox_component_t*)comp;
            if (lb->items) kfree(lb->items);
            break;
        }
        case COMPONENT_PANEL: {
            panel_component_t* pnl = (panel_component_t*)comp;
            if (pnl->children) kfree(pnl->children);
            break;
        }
        default:
            break;
    }

    kfree(comp);
}

void stardustui_dispatch_message(window_context_t* win, window_message_type_t msg_type,
                                uint64_t param1, uint64_t param2)
{
    if (!win || !initialized) return;

    if (win->message_handler) {
        win->message_handler(win, msg_type, param1, param2);
    }

    spin_lock(&win->lock);

    for (int i = 0; i < win->component_count; i++) {
        component_base_t* comp = win->components[i];
        if (!comp || !comp->visible || !comp->enabled) continue;

        bool handled = false;
        int x = (int)(param1 & 0xFFFFFFFF);
        int y = (int)((param1 >> 32) & 0xFFFFFFFF);

        switch (msg_type) {
            case WINDOW_MSG_MOUSE_MOVE:
                if (comp->handle_pointer_move) {
                    handled = comp->handle_pointer_move(comp, x, y);
                }
                break;
            case WINDOW_MSG_LEFT_BUTTON_DOWN:
            case WINDOW_MSG_LEFT_BUTTON_UP:
                if (comp->handle_left_button) {
                    handled = comp->handle_left_button(comp,
                        msg_type == WINDOW_MSG_LEFT_BUTTON_DOWN, x, y);
                }
                break;
            case WINDOW_MSG_RIGHT_BUTTON_DOWN:
            case WINDOW_MSG_RIGHT_BUTTON_UP:
                if (comp->handle_right_button) {
                    handled = comp->handle_right_button(comp,
                        msg_type == WINDOW_MSG_RIGHT_BUTTON_DOWN, x, y);
                }
                break;
            case WINDOW_MSG_SCROLL_UP:
            case WINDOW_MSG_SCROLL_DOWN:
                if (comp->handle_scroll) {
                    handled = comp->handle_scroll(comp,
                        msg_type == WINDOW_MSG_SCROLL_UP ? 1 : -1);
                }
                break;
            case WINDOW_MSG_KEY_DOWN:
            case WINDOW_MSG_KEY_UP:
                if (comp->handle_key) {
                    handled = comp->handle_key(comp, (int)param1,
                        msg_type == WINDOW_MSG_KEY_DOWN);
                }
                break;
            default:
                break;
        }

        if (handled) break;
    }

    spin_unlock(&win->lock);
}

void stardustui_render_window(window_context_t* win)
{
    if (!win || !win->visible || !initialized) return;

    display_layer_t* layer = display_get_layer(win->display_layer_id);
    if (!layer || !layer->buffer) return;

    uint32_t* buffer = layer->buffer;
    int stride = layer->width;

    draw_rounded_rect(buffer, stride, 0, 0, win->client_rect.width,
                     win->client_rect.height, current_theme.corner_radius,
                     color_to_uint32(win->client_area_color));

    if (win->decorated && win->title_bar_height > 0) {
        draw_rect(buffer, stride, 0, 0, win->client_rect.width, win->title_bar_height,
                 color_to_uint32(win->title_bar_color));

        draw_text(buffer, stride, 12, (win->title_bar_height - 16) / 2,
                 win->title, color_to_uint32((color_t){255, 255, 255, 255}), 0, 1);
    }

    spin_lock(&win->lock);

    for (int i = 0; i < win->component_count; i++) {
        component_base_t* comp = win->components[i];
        if (comp && comp->visible && comp->draw) {
            comp->draw(comp, buffer, stride);
            comp->needs_redraw = false;
        }
    }

    spin_unlock(&win->lock);
}

void stardustui_run_main_loop(void)
{
    if (!initialized) return;

    while (true) {
        bool any_needs_update = false;

        for (int i = 0; i < STARDUSTUI_MAX_WINDOWS; i++) {
            window_context_t* win = &windows[i];
            if (win->id == 0 || !win->visible) continue;

            spin_lock(&win->lock);

            for (int j = 0; j < win->component_count; j++) {
                component_base_t* comp = win->components[j];
                if (comp && comp->visible && comp->enabled && comp->update) {
                    comp->update(comp);
                    if (comp->needs_redraw) {
                        any_needs_update = true;
                    }
                }
            }

            spin_unlock(&win->lock);

            if (any_needs_update || true) {
                stardustui_render_window(win);
            }
        }

        display_composite_layers();
        display_flip_buffers();

        sleep_ms(16);
    }
}