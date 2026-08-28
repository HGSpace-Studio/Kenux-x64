#ifndef KAPI_CLIPBOARD_H
#define KAPI_CLIPBOARD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_CLIPBOARD_FORMAT_TEXT       1
#define KAPI_CLIPBOARD_FORMAT_HTML       2
#define KAPI_CLIPBOARD_FORMAT_RTF        3
#define KAPI_CLIPBOARD_FORMAT_IMAGE      4
#define KAPI_CLIPBOARD_FORMAT_FILE_LIST  5
#define KAPI_CLIPBOARD_FORMAT_CUSTOM    16

#define KAPI_CLIPBOARD_MAX_DATA       (64 * 1024)
#define KAPI_CLIPBOARD_MAX_FORMATS     8
#define KAPI_CLIPBOARD_MAX_LISTENERS  16

typedef struct kapi_clipboard_data {
    uint32_t format;
    uint8_t* data;
    size_t size;
    size_t capacity;
} kapi_clipboard_data_t;

typedef struct kapi_clipboard {
    kapi_clipboard_data_t entries[KAPI_CLIPBOARD_MAX_FORMATS];
    int entry_count;
    uint32_t sequence_number;
    uint64_t timestamp;
    int owner_window_id;
} kapi_clipboard_t;

typedef void (*kapi_clipboard_notify_fn)(uint32_t format, uint32_t sequence, void* user_data);

typedef struct kapi_clipboard_listener {
    kapi_clipboard_notify_fn callback;
    void* user_data;
    uint32_t format_mask;
    int active;
} kapi_clipboard_listener_t;

int kapi_clipboard_init(void);
void kapi_clipboard_cleanup(void);

int kapi_clipboard_set_text(const char* text, size_t length);
int kapi_clipboard_get_text(char* buffer, size_t buffer_size, size_t* out_length);

int kapi_clipboard_set_html(const char* html, size_t length);
int kapi_clipboard_get_html(char* buffer, size_t buffer_size, size_t* out_length);

int kapi_clipboard_set_image(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t format);
int kapi_clipboard_get_image(uint8_t** pixels, uint32_t* width, uint32_t* height, uint32_t* format);

int kapi_clipboard_set_file_list(const char** paths, int count);
int kapi_clipboard_get_file_list(char*** paths, int* count);

int kapi_clipboard_set_data(uint32_t format, const uint8_t* data, size_t size);
int kapi_clipboard_get_data(uint32_t format, uint8_t* buffer, size_t buffer_size, size_t* out_size);

int kapi_clipboard_has_format(uint32_t format);
int kapi_clipboard_get_available_formats(uint32_t* formats, int max_count, int* out_count);

void kapi_clipboard_clear(void);
int kapi_clipboard_is_empty(void);

uint32_t kapi_clipboard_get_sequence(void);

int kapi_clipboard_register_listener(kapi_clipboard_notify_fn callback, void* user_data, uint32_t format_mask);
int kapi_clipboard_unregister_listener(kapi_clipboard_notify_fn callback);

int kapi_clipboard_set_owner(int window_id);
int kapi_clipboard_get_owner(void);

int kapi_clipboard_begin_batch(void);
int kapi_clipboard_end_batch(void);

#ifdef __cplusplus
}
#endif

#endif