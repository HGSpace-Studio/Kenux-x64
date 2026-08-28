#include "kapi_clipboard.h"
#include "kapi.h"
#include <string.h>

static kapi_clipboard_t clipboard;
static kapi_clipboard_listener_t listeners[KAPI_CLIPBOARD_MAX_LISTENERS];
static int listener_count = 0;
static int batch_mode = 0;

int kapi_clipboard_init(void)
{
    memset(&clipboard, 0, sizeof(kapi_clipboard_t));
    memset(listeners, 0, sizeof(listeners));
    listener_count = 0;
    batch_mode = 0;
    return 0;
}

void kapi_clipboard_cleanup(void)
{
    for (int i = 0; i < clipboard.entry_count; i++) {
        if (clipboard.entries[i].data) {
            kapi_kfree(clipboard.entries[i].data);
            clipboard.entries[i].data = NULL;
        }
    }
    clipboard.entry_count = 0;
    listener_count = 0;
}

static int find_entry(uint32_t format)
{
    for (int i = 0; i < clipboard.entry_count; i++) {
        if (clipboard.entries[i].format == format) return i;
    }
    return -1;
}

static int alloc_entry(uint32_t format)
{
    int idx = find_entry(format);
    if (idx >= 0) {
        if (clipboard.entries[idx].data) kapi_kfree(clipboard.entries[idx].data);
        memset(&clipboard.entries[idx], 0, sizeof(kapi_clipboard_data_t));
        clipboard.entries[idx].format = format;
        return idx;
    }
    if (clipboard.entry_count >= KAPI_CLIPBOARD_MAX_FORMATS) return -1;
    idx = clipboard.entry_count++;
    memset(&clipboard.entries[idx], 0, sizeof(kapi_clipboard_data_t));
    clipboard.entries[idx].format = format;
    return idx;
}

static void notify_listeners(uint32_t format)
{
    for (int i = 0; i < listener_count; i++) {
        if (listeners[i].active && listeners[i].callback) {
            if (listeners[i].format_mask == 0 || (listeners[i].format_mask & format)) {
                listeners[i].callback(format, clipboard.sequence_number, listeners[i].user_data);
            }
        }
    }
}

static int set_data_internal(uint32_t format, const uint8_t* data, size_t size)
{
    int idx = alloc_entry(format);
    if (idx < 0) return -1;
    clipboard.entries[idx].data = (uint8_t*)kapi_kmalloc(size);
    if (!clipboard.entries[idx].data) return -1;
    memcpy(clipboard.entries[idx].data, data, size);
    clipboard.entries[idx].size = size;
    clipboard.entries[idx].capacity = size;
    clipboard.sequence_number++;
    clipboard.timestamp = kapi_get_tick_count();
    if (!batch_mode) notify_listeners(format);
    return 0;
}

int kapi_clipboard_set_text(const char* text, size_t length)
{
    if (!text) return -1;
    return set_data_internal(KAPI_CLIPBOARD_FORMAT_TEXT, (const uint8_t*)text, length);
}

int kapi_clipboard_get_text(char* buffer, size_t buffer_size, size_t* out_length)
{
    int idx = find_entry(KAPI_CLIPBOARD_FORMAT_TEXT);
    if (idx < 0) { if (out_length) *out_length = 0; return -1; }
    size_t len = clipboard.entries[idx].size;
    if (len >= buffer_size) len = buffer_size - 1;
    memcpy(buffer, clipboard.entries[idx].data, len);
    buffer[len] = '\0';
    if (out_length) *out_length = len;
    return 0;
}

int kapi_clipboard_set_html(const char* html, size_t length)
{
    if (!html) return -1;
    return set_data_internal(KAPI_CLIPBOARD_FORMAT_HTML, (const uint8_t*)html, length);
}

int kapi_clipboard_get_html(char* buffer, size_t buffer_size, size_t* out_length)
{
    int idx = find_entry(KAPI_CLIPBOARD_FORMAT_HTML);
    if (idx < 0) { if (out_length) *out_length = 0; return -1; }
    size_t len = clipboard.entries[idx].size;
    if (len >= buffer_size) len = buffer_size - 1;
    memcpy(buffer, clipboard.entries[idx].data, len);
    buffer[len] = '\0';
    if (out_length) *out_length = len;
    return 0;
}

int kapi_clipboard_set_image(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t format)
{
    if (!pixels) return -1;
    size_t header = sizeof(uint32_t) * 3;
    size_t img_sz = (size_t)width * height * 4;
    size_t total = header + img_sz;
    uint8_t* buf = (uint8_t*)kapi_kmalloc(total);
    if (!buf) return -1;
    uint32_t* hdr = (uint32_t*)buf;
    hdr[0] = width; hdr[1] = height; hdr[2] = format;
    memcpy(buf + header, pixels, img_sz);
    int ret = set_data_internal(KAPI_CLIPBOARD_FORMAT_IMAGE, buf, total);
    kapi_kfree(buf);
    return ret;
}

int kapi_clipboard_get_image(uint8_t** pixels, uint32_t* width, uint32_t* height, uint32_t* format)
{
    int idx = find_entry(KAPI_CLIPBOARD_FORMAT_IMAGE);
    if (idx < 0) return -1;
    kapi_clipboard_data_t* e = &clipboard.entries[idx];
    if (e->size < sizeof(uint32_t) * 3) return -1;
    const uint32_t* hdr = (const uint32_t*)e->data;
    uint32_t w = hdr[0], h = hdr[1], f = hdr[2];
    size_t img_sz = (size_t)w * h * 4;
    size_t header = sizeof(uint32_t) * 3;
    if (e->size < header + img_sz) return -1;
    *pixels = (uint8_t*)kapi_kmalloc(img_sz);
    if (!*pixels) return -1;
    memcpy(*pixels, e->data + header, img_sz);
    *width = w; *height = h; *format = f;
    return 0;
}

int kapi_clipboard_set_file_list(const char** paths, int count)
{
    if (!paths || count <= 0) return -1;
    size_t total = sizeof(int);
    for (int i = 0; i < count; i++) total += kapi_strlen(paths[i]) + 1;
    uint8_t* buf = (uint8_t*)kapi_kmalloc(total);
    if (!buf) return -1;
    *(int*)buf = count;
    size_t off = sizeof(int);
    for (int i = 0; i < count; i++) {
        size_t len = kapi_strlen(paths[i]) + 1;
        memcpy(buf + off, paths[i], len);
        off += len;
    }
    int ret = set_data_internal(KAPI_CLIPBOARD_FORMAT_FILE_LIST, buf, total);
    kapi_kfree(buf);
    return ret;
}

int kapi_clipboard_get_file_list(char*** paths, int* count)
{
    int idx = find_entry(KAPI_CLIPBOARD_FORMAT_FILE_LIST);
    if (idx < 0) return -1;
    kapi_clipboard_data_t* e = &clipboard.entries[idx];
    int n = *(int*)e->data;
    *count = n;
    *paths = (char**)kapi_kmalloc(sizeof(char*) * n);
    if (!*paths) return -1;
    size_t off = sizeof(int);
    for (int i = 0; i < n; i++) {
        size_t len = kapi_strlen((const char*)(e->data + off)) + 1;
        (*paths)[i] = (char*)kapi_kmalloc(len);
        memcpy((*paths)[i], e->data + off, len);
        off += len;
    }
    return 0;
}

int kapi_clipboard_set_data(uint32_t format, const uint8_t* data, size_t size)
{
    if (!data) return -1;
    return set_data_internal(format, data, size);
}

int kapi_clipboard_get_data(uint32_t format, uint8_t* buffer, size_t buffer_size, size_t* out_size)
{
    int idx = find_entry(format);
    if (idx < 0) { if (out_size) *out_size = 0; return -1; }
    size_t sz = clipboard.entries[idx].size;
    if (sz > buffer_size) sz = buffer_size;
    memcpy(buffer, clipboard.entries[idx].data, sz);
    if (out_size) *out_size = sz;
    return 0;
}

int kapi_clipboard_has_format(uint32_t format)
{
    return find_entry(format) >= 0 ? 1 : 0;
}

int kapi_clipboard_get_available_formats(uint32_t* formats, int max_count, int* out_count)
{
    int n = clipboard.entry_count < max_count ? clipboard.entry_count : max_count;
    for (int i = 0; i < n; i++) formats[i] = clipboard.entries[i].format;
    *out_count = clipboard.entry_count;
    return 0;
}

void kapi_clipboard_clear(void)
{
    for (int i = 0; i < clipboard.entry_count; i++) {
        if (clipboard.entries[i].data) kapi_kfree(clipboard.entries[i].data);
    }
    clipboard.entry_count = 0;
    clipboard.sequence_number++;
}

int kapi_clipboard_is_empty(void) { return clipboard.entry_count == 0; }
uint32_t kapi_clipboard_get_sequence(void) { return clipboard.sequence_number; }

int kapi_clipboard_register_listener(kapi_clipboard_notify_fn callback, void* user_data, uint32_t format_mask)
{
    if (!callback || listener_count >= KAPI_CLIPBOARD_MAX_LISTENERS) return -1;
    kapi_clipboard_listener_t* l = &listeners[listener_count++];
    l->callback = callback; l->user_data = user_data; l->format_mask = format_mask; l->active = 1;
    return 0;
}

int kapi_clipboard_unregister_listener(kapi_clipboard_notify_fn callback)
{
    for (int i = 0; i < listener_count; i++) {
        if (listeners[i].callback == callback) {
            listeners[i] = listeners[--listener_count];
            return 0;
        }
    }
    return -1;
}

int kapi_clipboard_set_owner(int window_id) { clipboard.owner_window_id = window_id; return 0; }
int kapi_clipboard_get_owner(void) { return clipboard.owner_window_id; }
int kapi_clipboard_begin_batch(void) { batch_mode = 1; return 0; }
int kapi_clipboard_end_batch(void)
{
    batch_mode = 0;
    for (int i = 0; i < clipboard.entry_count; i++) notify_listeners(clipboard.entries[i].format);
    return 0;
}