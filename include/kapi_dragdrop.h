#ifndef KAPI_DRAGDROP_H
#define KAPI_DRAGDROP_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_window.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_DROP_FORMAT_TEXT       1
#define KAPI_DROP_FORMAT_HTML       2
#define KAPI_DROP_FORMAT_URI_LIST   3
#define KAPI_DROP_FORMAT_IMAGE      4
#define KAPI_DROP_FORMAT_CUSTOM    16

#define KAPI_DROP_MAX_FORMATS       8
#define KAPI_DROP_MAX_DATA       (64 * 1024)
#define KAPI_DROP_MAX_TRACKERS    32

typedef enum {
    KAPI_DROP_ACTION_NONE    = 0,
    KAPI_DROP_ACTION_COPY    = 1,
    KAPI_DROP_ACTION_MOVE    = 2,
    KAPI_DROP_ACTION_LINK    = 4,
    KAPI_DROP_ACTION_ASK     = 8
} kapi_drop_action_t;

typedef struct kapi_drop_data {
    uint32_t format;
    uint8_t* data;
    size_t size;
} kapi_drop_data_t;

typedef struct kapi_drop_event {
    kapi_window_t* source;
    kapi_window_t* target;
    int32_t x;
    int32_t y;
    int32_t root_x;
    int32_t root_y;
    uint32_t available_actions;
    uint32_t selected_action;
    kapi_drop_data_t* data_items;
    int data_count;
} kapi_drop_event_t;

typedef struct kapi_drop_source {
    kapi_window_t* window;
    kapi_drop_data_t data[KAPI_DROP_MAX_FORMATS];
    int data_count;
    uint32_t supported_actions;
    int active;
    int32_t start_x;
    int32_t start_y;
    int32_t threshold;
} kapi_drop_source_t;

typedef struct kapi_drop_target {
    kapi_window_t* window;
    uint32_t accepted_formats;
    uint32_t preferred_action;
    int registered;
} kapi_drop_target_t;

typedef enum {
    KAPI_DROP_ENTER  = 1,
    KAPI_DROP_LEAVE  = 2,
    KAPI_DROP_MOVE   = 3,
    KAPI_DROP_DROP   = 4,
    KAPI_DROP_ACCEPT = 5,
    KAPI_DROP_REJECT = 6
} kapi_drop_notify_type_t;

typedef void (*kapi_drop_handler_fn)(kapi_drop_notify_type_t type,
                                     const kapi_drop_event_t* event,
                                     void* user_data);

int kapi_dragdrop_init(void);
void kapi_dragdrop_cleanup(void);

int kapi_dragdrop_register_source(kapi_window_t* window, uint32_t supported_actions);
int kapi_dragdrop_unregister_source(kapi_window_t* window);

int kapi_dragdrop_register_target(kapi_window_t* window, uint32_t accepted_formats,
                                  uint32_t preferred_action);
int kapi_dragdrop_unregister_target(kapi_window_t* window);

int kapi_dragdrop_begin(kapi_window_t* source, int32_t x, int32_t y);
int kapi_dragdrop_set_data(uint32_t format, const uint8_t* data, size_t size);
int kapi_dragdrop_add_text(const char* text, size_t length);
int kapi_dragdrop_add_uri_list(const char** uris, int count);
int kapi_dragdrop_add_image(const uint8_t* pixels, uint32_t w, uint32_t h, uint32_t fmt);

int kapi_dragdrop_update(int32_t x, int32_t y);
int kapi_dragdrop_finish(int32_t x, int32_t y, uint32_t action);
int kapi_dragdrop_cancel(void);

int kapi_dragdrop_is_active(void);
kapi_window_t* kapi_dragdrop_get_source(void);
kapi_window_t* kapi_dragdrop_get_target(void);

int kapi_dragdrop_set_handler(kapi_window_t* window, kapi_drop_handler_fn handler, void* user_data);
int kapi_dragdrop_set_cursor_for_action(uint32_t action, void* cursor);

int kapi_dragdrop_query_target(kapi_window_t* window, int32_t x, int32_t y,
                               uint32_t* formats, uint32_t* actions);

#ifdef __cplusplus
}
#endif

#endif