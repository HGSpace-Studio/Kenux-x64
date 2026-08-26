#ifndef KAPI_WINDOW_H
#define KAPI_WINDOW_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Window types */
#define KAPI_WINDOW_TYPE_UNKNOWN     0
#define KAPI_WINDOW_TYPE_NORMAL      1
#define KAPI_WINDOW_TYPE_DIALOG      2
#define KAPI_WINDOW_TYPE_TOOLTIP     3
#define KAPI_WINDOW_TYPE_MENU        4
#define KAPI_WINDOW_TYPE_DESKTOP     5

/* Window flags */
#define KAPI_WINDOW_FLAG_VISIBLE     (1 << 0)
#define KAPI_WINDOW_FLAG_BORDER      (1 << 1)
#define KAPI_WINDOW_FLAG_RESIZABLE   (1 << 2)
#define KAPI_WINDOW_FLAG_MINIMIZABLE  (1 << 3)
#define KAPI_WINDOW_FLAG_MAXIMIZABLE  (1 << 4)
#define KAPI_WINDOW_FLAG_CLOSEABLE   (1 << 5)
#define KAPI_WINDOW_FLAG_MODAL       (1 << 6)
#define KAPI_WINDOW_FLAG_FOCUSED     (1 << 7)
#define KAPI_WINDOW_FLAG_TOPMOST     (1 << 8)
#define KAPI_WINDOW_FLAG_FULLSCREEN   (1 << 9)
#define KAPI_WINDOW_FLAG_TRANSPARENT (1 << 10)

/* Window states */
#define KAPI_WINDOW_STATE_UNKNOWN    0
#define KAPI_WINDOW_STATE_NORMAL     1
#define KAPI_WINDOW_STATE_MINIMIZED  2
#define KAPI_WINDOW_STATE_MAXIMIZED  3
#define KAPI_WINDOW_STATE_FULLSCREEN  4
#define KAPI_WINDOW_STATE_HIDDEN     5

/* Window event types */
#define KAPI_WINDOW_EVENT_NONE       0
#define KAPI_WINDOW_EVENT_MOVE       1
#define KAPI_WINDOW_EVENT_RESIZE     2
#define KAPI_WINDOW_EVENT_CLOSE      3
#define KAPI_WINDOW_EVENT_FOCUS      4
#define KAPI_WINDOW_EVENT_BLUR       5
#define KAPI_WINDOW_EVENT_SHOW       6
#define KAPI_WINDOW_EVENT_HIDE       7
#define KAPI_WINDOW_EVENT_MINIMIZE   8
#define KAPI_WINDOW_EVENT_MAXIMIZE   9
#define KAPI_WINDOW_EVENT_RESTORE    10
#define KAPI_WINDOW_EVENT_KEY        11
#define KAPI_WINDOW_EVENT_MOUSE      12
#define KAPI_WINDOW_EVENT_PAINT      13
#define KAPI_WINDOW_EVENT_DESTROY    14

/* Mouse button types */
#define KAPI_MOUSE_BUTTON_NONE      0
#define KAPI_MOUSE_BUTTON_LEFT      1
#define KAPI_MOUSE_BUTTON_RIGHT     2
#define KAPI_MOUSE_BUTTON_MIDDLE    3
#define KAPI_MOUSE_BUTTON_X1         4
#define KAPI_MOUSE_BUTTON_X2         5

/* Mouse event types */
#define KAPI_MOUSE_EVENT_NONE       0
#define KAPI_MOUSE_EVENT_PRESS       1
#define KAPI_MOUSE_EVENT_RELEASE     2
#define KAPI_MOUSE_EVENT_DOUBLE_CLICK 3
#define KAPI_MOUSE_EVENT_MOVE       4
#define KAPI_MOUSE_EVENT_WHEEL       5
#define KAPI_MOUSE_EVENT_ENTER      6
#define KAPI_MOUSE_EVENT_LEAVE      7

/* Keyboard event types */
#define KAPI_KEY_EVENT_NONE         0
#define KAPI_KEY_EVENT_PRESS        1
#define KAPI_KEY_EVENT_RELEASE      2
#define KAPI_KEY_EVENT_REPEAT       3

/* Coordinate structures */
typedef struct {
    int32_t x;
    int32_t y;
} kapi_point_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} kapi_rect_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} kapi_vector3d_t;

/* Color structures */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} kapi_color_t;

/* Window structure */
typedef struct kapi_window kapi_window_t;

/* Display structure */
typedef struct kapi_display kapi_display_t;

/* Surface structure (for drawing) */
typedef struct kapi_surface kapi_surface_t;

/* Window context structure */
typedef struct kapi_window_context kapi_window_context_t;

/* Event structures */
typedef struct {
    uint32_t type;
    uint32_t timestamp;
    kapi_window_t* window;
    union {
        struct {
            int32_t x;
            int32_t y;
        } move;
        struct {
            int32_t width;
            int32_t height;
        } resize;
        struct {
            uint32_t key;
            uint32_t scan_code;
            uint32_t modifiers;
        } key;
        struct {
            uint32_t button;
            int32_t x;
            int32_t y;
            int32_t dx;
            int32_t dy;
        } mouse;
        struct {
            int32_t x;
            int32_t y;
            int32_t width;
            int32_t height;
        } paint;
    } data;
} kapi_window_event_t;

/* Window properties structure */
typedef struct {
    char title[256];
    uint32_t type;
    uint32_t flags;
    kapi_rect_t bounds;
    kapi_rect_t client_rect;
    uint32_t state;
    kapi_color_t background_color;
    kapi_color_t border_color;
    uint32_t border_width;
    uint32_t title_bar_height;
    uint32_t menu_bar_height;
    kapi_window_t* parent;
    kapi_window_t* next_sibling;
    kapi_window_t* first_child;
    kapi_window_t* last_child;
    kapi_surface_t* surface;
} kapi_window_properties_t;

/* Window operations structure */
typedef struct {
    int (*create)(kapi_window_t* window, const kapi_window_properties_t* props);
    int (*destroy)(kapi_window_t* window);
    int (*show)(kapi_window_t* window);
    int (*hide)(kapi_window_t* window);
    int (*move)(kapi_window_t* window, int32_t x, int32_t y);
    int (*resize)(kapi_window_t* window, int32_t width, int32_t height);
    int (*minimize)(kapi_window_t* window);
    int (*maximize)(kapi_window_t* window);
    int (*restore)(kapi_window_t* window);
    int (*set_focus)(kapi_window_t* window);
    int (*remove_focus)(kapi_window_t* window);
    int (*bring_to_front)(kapi_window_t* window);
    int (*send_to_back)(kapi_window_t* window);
    int (*set_always_on_top)(kapi_window_t* window, int topmost);
    int (*set_transparency)(kapi_window_t* window, uint8_t alpha);
    kapi_surface_t* (*get_surface)(kapi_window_t* window);
    int (*set_title)(kapi_window_t* window, const char* title);
    int (*set_icon)(kapi_window_t* window, const char* icon_path);
    int (*set_background_color)(kapi_window_t* window, const kapi_color_t* color);
} kapi_window_ops_t;

/* Window structure */
struct kapi_window {
    kapi_window_t* next;
    kapi_window_t* prev;
    kapi_window_t* parent;
    kapi_window_t* first_child;
    kapi_window_t* last_child;
    kapi_window_properties_t properties;
    kapi_window_ops_t* ops;
    void* user_data;
    uint32_t id;
    uint32_t ref_count;
    uint64_t creation_time;
};

/* Display structure */
struct kapi_display {
    uint32_t id;
    char name[64];
    kapi_rect_t bounds;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t refresh_rate;
    uint32_t color_depth;
    kapi_window_t* desktop_window;
    kapi_window_t* focused_window;
    kapi_window_t* top_window;
    uint32_t window_count;
    kapi_display_t* next;
};

/* Window manager API */
int kapi_window_manager_init(void);
void kapi_window_manager_cleanup(void);

int kapi_window_manager_update(void);

/* Window operations */
kapi_window_t* kapi_window_create(const char* title, int x, int y, int width, int height,
                                  uint32_t type, uint32_t flags);
int kapi_window_destroy(kapi_window_t* window);

kapi_window_t* kapi_window_find(uint32_t window_id);
kapi_window_t* kapi_window_find_by_title(const char* title);

int kapi_window_show(kapi_window_t* window);
int kapi_window_hide(kapi_window_t* window);
int kapi_window_move(kapi_window_t* window, int32_t x, int32_t y);
int kapi_window_resize(kapi_window_t* window, int32_t width, int32_t height);
int kapi_window_minimize(kapi_window_t* window);
int kapi_window_maximize(kapi_window_t* window);
int kapi_window_restore(kapi_window_t* window);
int kapi_window_set_focus(kapi_window_t* window);
int kapi_window_remove_focus(kapi_window_t* window);

int kapi_window_bring_to_front(kapi_window_t* window);
int kapi_window_send_to_back(kapi_window_t* window);
int kapi_window_set_always_on_top(kapi_window_t* window, int topmost);
int kapi_window_set_transparency(kapi_window_t* window, uint8_t alpha);

/* Window properties */
int kapi_window_set_title(kapi_window_t* window, const char* title);
int kapi_window_set_icon(kapi_window_t* window, const char* icon_path);
int kapi_window_set_background_color(kapi_window_t* window, const kapi_color_t* color);
int kapi_window_set_border_color(kapi_window_t* window, const kapi_color_t* color);

/* Window hierarchy */
int kapi_window_set_parent(kapi_window_t* window, kapi_window_t* parent);
int kapi_window_add_child(kapi_window_t* parent, kapi_window_t* child);
int kapi_window_remove_child(kapi_window_t* parent, kapi_window_t* child);

/* Window event handling */
int kapi_window_process_events(void);
int kapi_window_handle_event(kapi_window_t* window, const kapi_window_event_t* event);
int kapi_window_send_event(kapi_window_t* window, const kapi_window_event_t* event);

/* Window state queries */
kapi_window_properties_t* kapi_window_get_properties(kapi_window_t* window);
kapi_rect_t kapi_window_get_bounds(kapi_window_t* window);
kapi_rect_t kapi_window_get_client_rect(kapi_window_t* window);
uint32_t kapi_window_get_state(kapi_window_t* window);
kapi_surface_t* kapi_window_get_surface(kapi_window_t* window);

/* Display operations */
kapi_display_t* kapi_display_get_primary(void);
kapi_display_t* kapi_display_get(uint32_t display_id);
int kapi_display_get_count(uint32_t* count);
int kapi_display_update_all(void);

/* Window enumeration */
int kapi_window_list_all(kapi_window_t*** windows, uint32_t* count);
int kapi_window_list_visible(kapi_window_t*** windows, uint32_t* count);
int kapi_window_list_focused(kapi_window_t*** windows, uint32_t* count);

/* Window debugging */
int kapi_window_dump_hierarchy(kapi_window_t* window);
int kapi_window_dump_all(void);

/* Z-order management */
int kapi_window_get_z_order(kapi_window_t* window);
int kapi_window_set_z_order(kapi_window_t* window, int z_order);

#ifdef __cplusplus
}
#endif

#endif