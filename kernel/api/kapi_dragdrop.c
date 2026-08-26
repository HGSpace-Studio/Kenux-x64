#include "kapi_dragdrop.h"
#include "kapi.h"
#include <string.h>

static kapi_drop_source_t sources[KAPI_DROP_MAX_TRACKERS];
static kapi_drop_target_t targets[KAPI_DROP_MAX_TRACKERS];
static int source_count = 0;
static int target_count = 0;

static struct {
    int active;
    kapi_window_t* source;
    kapi_window_t* target;
    kapi_drop_data_t data[KAPI_DROP_MAX_FORMATS];
    int data_count;
    int32_t start_x, start_y;
    int32_t cur_x, cur_y;
    uint32_t actions;
    kapi_drop_handler_fn handler;
    void* handler_data;
} drag_state;

int kapi_dragdrop_init(void)
{
    memset(sources, 0, sizeof(sources));
    memset(targets, 0, sizeof(targets));
    memset(&drag_state, 0, sizeof(drag_state));
    source_count = 0;
    target_count = 0;
    return 0;
}

void kapi_dragdrop_cleanup(void)
{
    for (int i = 0; i < drag_state.data_count; i++) {
        if (drag_state.data[i].data) kapi_kfree(drag_state.data[i].data);
    }
    memset(&drag_state, 0, sizeof(drag_state));
    source_count = 0;
    target_count = 0;
}

int kapi_dragdrop_register_source(kapi_window_t* window, uint32_t supported_actions)
{
    if (!window || source_count >= KAPI_DROP_MAX_TRACKERS) return -1;
    kapi_drop_source_t* s = &sources[source_count++];
    memset(s, 0, sizeof(kapi_drop_source_t));
    s->window = window;
    s->supported_actions = supported_actions;
    s->threshold = 4;
    return 0;
}

int kapi_dragdrop_unregister_source(kapi_window_t* window)
{
    for (int i = 0; i < source_count; i++) {
        if (sources[i].window == window) {
            sources[i] = sources[--source_count];
            return 0;
        }
    }
    return -1;
}

int kapi_dragdrop_register_target(kapi_window_t* window, uint32_t accepted_formats, uint32_t preferred_action)
{
    if (!window || target_count >= KAPI_DROP_MAX_TRACKERS) return -1;
    kapi_drop_target_t* t = &targets[target_count++];
    memset(t, 0, sizeof(kapi_drop_target_t));
    t->window = window;
    t->accepted_formats = accepted_formats;
    t->preferred_action = preferred_action;
    t->registered = 1;
    return 0;
}

int kapi_dragdrop_unregister_target(kapi_window_t* window)
{
    for (int i = 0; i < target_count; i++) {
        if (targets[i].window == window) {
            targets[i] = targets[--target_count];
            return 0;
        }
    }
    return -1;
}

int kapi_dragdrop_begin(kapi_window_t* source, int32_t x, int32_t y)
{
    if (!source || drag_state.active) return -1;
    drag_state.active = 1;
    drag_state.source = source;
    drag_state.target = NULL;
    drag_state.data_count = 0;
    drag_state.start_x = x;
    drag_state.start_y = y;
    drag_state.cur_x = x;
    drag_state.cur_y = y;
    drag_state.actions = KAPI_DROP_ACTION_COPY;
    return 0;
}

int kapi_dragdrop_set_data(uint32_t format, const uint8_t* data, size_t size)
{
    if (!drag_state.active || drag_state.data_count >= KAPI_DROP_MAX_FORMATS) return -1;
    if (!data) return -1;
    kapi_drop_data_t* d = &drag_state.data[drag_state.data_count++];
    d->format = format;
    d->data = (uint8_t*)kapi_kmalloc(size);
    if (!d->data) return -1;
    memcpy(d->data, data, size);
    d->size = size;
    return 0;
}

int kapi_dragdrop_add_text(const char* text, size_t length)
{
    return kapi_dragdrop_set_data(KAPI_DROP_FORMAT_TEXT, (const uint8_t*)text, length);
}

int kapi_dragdrop_add_uri_list(const char** uris, int count)
{
    if (!uris || count <= 0) return -1;
    size_t total = 0;
    for (int i = 0; i < count; i++) total += kapi_strlen(uris[i]) + 1;
    uint8_t* buf = (uint8_t*)kapi_kmalloc(total);
    if (!buf) return -1;
    size_t off = 0;
    for (int i = 0; i < count; i++) {
        size_t len = kapi_strlen(uris[i]);
        memcpy(buf + off, uris[i], len);
        buf[off + len] = '\n';
        off += len + 1;
    }
    int ret = kapi_dragdrop_set_data(KAPI_DROP_FORMAT_URI_LIST, buf, total);
    kapi_kfree(buf);
    return ret;
}

int kapi_dragdrop_add_image(const uint8_t* pixels, uint32_t w, uint32_t h, uint32_t fmt)
{
    if (!pixels) return -1;
    size_t header = sizeof(uint32_t) * 3;
    size_t img_sz = (size_t)w * h * 4;
    size_t total = header + img_sz;
    uint8_t* buf = (uint8_t*)kapi_kmalloc(total);
    if (!buf) return -1;
    uint32_t* hdr = (uint32_t*)buf;
    hdr[0] = w; hdr[1] = h; hdr[2] = fmt;
    memcpy(buf + header, pixels, img_sz);
    int ret = kapi_dragdrop_set_data(KAPI_DROP_FORMAT_IMAGE, buf, total);
    kapi_kfree(buf);
    return ret;
}

static kapi_window_t* find_target_at(int32_t x, int32_t y)
{
    (void)x; (void)y;
    for (int i = 0; i < target_count; i++) {
        if (targets[i].registered && targets[i].window) return targets[i].window;
    }
    return NULL;
}

int kapi_dragdrop_update(int32_t x, int32_t y)
{
    if (!drag_state.active) return -1;
    drag_state.cur_x = x;
    drag_state.cur_y = y;
    kapi_window_t* new_target = find_target_at(x, y);
    if (new_target != drag_state.target) {
        if (drag_state.target && drag_state.handler) {
            kapi_drop_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.source = drag_state.source;
            ev.target = drag_state.target;
            ev.x = x; ev.y = y;
            drag_state.handler(KAPI_DROP_LEAVE, &ev, drag_state.handler_data);
        }
        drag_state.target = new_target;
        if (new_target && drag_state.handler) {
            kapi_drop_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.source = drag_state.source;
            ev.target = new_target;
            ev.x = x; ev.y = y;
            ev.available_actions = drag_state.actions;
            drag_state.handler(KAPI_DROP_ENTER, &ev, drag_state.handler_data);
        }
    } else if (drag_state.target && drag_state.handler) {
        kapi_drop_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.source = drag_state.source;
        ev.target = drag_state.target;
        ev.x = x; ev.y = y;
        ev.available_actions = drag_state.actions;
        drag_state.handler(KAPI_DROP_MOVE, &ev, drag_state.handler_data);
    }
    return 0;
}

int kapi_dragdrop_finish(int32_t x, int32_t y, uint32_t action)
{
    if (!drag_state.active) return -1;
    drag_state.cur_x = x;
    drag_state.cur_y = y;
    if (drag_state.target && drag_state.handler) {
        kapi_drop_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.source = drag_state.source;
        ev.target = drag_state.target;
        ev.x = x; ev.y = y;
        ev.selected_action = action;
        ev.available_actions = drag_state.actions;
        ev.data_items = drag_state.data;
        ev.data_count = drag_state.data_count;
        drag_state.handler(KAPI_DROP_DROP, &ev, drag_state.handler_data);
    }
    for (int i = 0; i < drag_state.data_count; i++) {
        if (drag_state.data[i].data) kapi_kfree(drag_state.data[i].data);
    }
    drag_state.active = 0;
    drag_state.data_count = 0;
    return 0;
}

int kapi_dragdrop_cancel(void)
{
    if (!drag_state.active) return -1;
    if (drag_state.target && drag_state.handler) {
        kapi_drop_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.source = drag_state.source;
        ev.target = drag_state.target;
        drag_state.handler(KAPI_DROP_LEAVE, &ev, drag_state.handler_data);
    }
    for (int i = 0; i < drag_state.data_count; i++) {
        if (drag_state.data[i].data) kapi_kfree(drag_state.data[i].data);
    }
    drag_state.active = 0;
    drag_state.data_count = 0;
    return 0;
}

int kapi_dragdrop_is_active(void) { return drag_state.active; }
kapi_window_t* kapi_dragdrop_get_source(void) { return drag_state.source; }
kapi_window_t* kapi_dragdrop_get_target(void) { return drag_state.target; }

int kapi_dragdrop_set_handler(kapi_window_t* window, kapi_drop_handler_fn handler, void* user_data)
{
    (void)window;
    drag_state.handler = handler;
    drag_state.handler_data = user_data;
    return 0;
}

int kapi_dragdrop_set_cursor_for_action(uint32_t action, void* cursor)
{
    (void)action; (void)cursor;
    return 0;
}

int kapi_dragdrop_query_target(kapi_window_t* window, int32_t x, int32_t y,
                               uint32_t* formats, uint32_t* actions)
{
    (void)x; (void)y;
    for (int i = 0; i < target_count; i++) {
        if (targets[i].window == window && targets[i].registered) {
            if (formats) *formats = targets[i].accepted_formats;
            if (actions) *actions = targets[i].preferred_action;
            return 0;
        }
    }
    return -1;
}