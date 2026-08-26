#ifndef KAPI_IME_H
#define KAPI_IME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_IME_MAX_CANDIDATES    32
#define KAPI_IME_MAX_PREEDIT      256
#define KAPI_IME_MAX_COMMIT       512
#define KAPI_IME_MAX_INPUT_CTX    16

typedef enum {
    KAPI_IME_STATE_INACTIVE = 0,
    KAPI_IME_STATE_ACTIVE   = 1,
    KAPI_IME_STATE_COMPOSING = 2,
    KAPI_IME_STATE_CANDIDATE = 3
} kapi_ime_state_t;

typedef enum {
    KAPI_IME_LANG_NONE    = 0,
    KAPI_IME_LANG_ZH_CN   = 1,
    KAPI_IME_LANG_ZH_TW   = 2,
    KAPI_IME_LANG_JA      = 3,
    KAPI_IME_LANG_KO      = 4,
    KAPI_IME_LANG_EN      = 5
} kapi_ime_lang_t;

typedef enum {
    KAPI_IME_MODE_NONE      = 0,
    KAPI_IME_MODE_PINYIN    = 1,
    KAPI_IME_MODE_WUBI      = 2,
    KAPI_IME_MODE_CANGJIE   = 3,
    KAPI_IME_MODE_HIRAGANA  = 4,
    KAPI_IME_MODE_KATAKANA  = 5,
    KAPI_IME_MODE_HANGUL    = 6,
    KAPI_IME_MODE_DIRECT    = 7
} kapi_ime_mode_t;

typedef enum {
    KAPI_IME_EVENT_COMMIT    = 1,
    KAPI_IME_EVENT_PREEDIT   = 2,
    KAPI_IME_EVENT_CANDIDATE = 3,
    KAPI_IME_EVENT_MODE_CHANGE = 4,
    KAPI_IME_EVENT_LANG_CHANGE = 5,
    KAPI_IME_EVENT_FOCUS     = 6,
    KAPI_IME_EVENT_BLUR      = 7
} kapi_ime_event_type_t;

typedef struct kapi_ime_candidate {
    char text[64];
    char comment[64];
    int id;
} kapi_ime_candidate_t;

typedef struct kapi_ime_preedit {
    char text[KAPI_IME_MAX_PREEDIT];
    int cursor_pos;
    int selection_start;
    int selection_end;
    struct {
        int start;
        int length;
        uint32_t color;
    } attributes[8];
    int attribute_count;
} kapi_ime_preedit_t;

typedef struct kapi_ime_event {
    kapi_ime_event_type_t type;
    union {
        struct {
            char text[KAPI_IME_MAX_COMMIT];
            size_t length;
        } commit;
        kapi_ime_preedit_t preedit;
        struct {
            kapi_ime_candidate_t items[KAPI_IME_MAX_CANDIDATES];
            int count;
            int page_index;
            int page_count;
            int selected_index;
        } candidates;
        struct {
            kapi_ime_mode_t old_mode;
            kapi_ime_mode_t new_mode;
        } mode_change;
        struct {
            kapi_ime_lang_t old_lang;
            kapi_ime_lang_t new_lang;
        } lang_change;
    } data;
} kapi_ime_event_t;

typedef struct kapi_ime_rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} kapi_ime_rect_t;

typedef void (*kapi_ime_callback_fn)(const kapi_ime_event_t* event, void* user_data);

typedef struct kapi_ime_context {
    int id;
    int window_id;
    kapi_ime_state_t state;
    kapi_ime_lang_t language;
    kapi_ime_mode_t mode;
    kapi_ime_preedit_t preedit;
    kapi_ime_callback_fn callback;
    void* user_data;
    kapi_ime_rect_t candidate_rect;
    bool active;
} kapi_ime_context_t;

int kapi_ime_init(void);
void kapi_ime_cleanup(void);

int kapi_ime_create_context(int window_id, kapi_ime_callback_fn callback, void* user_data);
int kapi_ime_destroy_context(int ctx_id);
int kapi_ime_focus_context(int ctx_id);
int kapi_ime_blur_context(int ctx_id);

int kapi_ime_enable(int ctx_id);
int kapi_ime_disable(int ctx_id);
bool kapi_ime_is_enabled(int ctx_id);

int kapi_ime_set_language(int ctx_id, kapi_ime_lang_t lang);
kapi_ime_lang_t kapi_ime_get_language(int ctx_id);
int kapi_ime_set_mode(int ctx_id, kapi_ime_mode_t mode);
kapi_ime_mode_t kapi_ime_get_mode(int ctx_id);

int kapi_ime_set_candidate_rect(int ctx_id, int32_t x, int32_t y, int32_t w, int32_t h);

int kapi_ime_process_key(int ctx_id, uint32_t key, uint32_t modifiers, bool down);
int kapi_ime_process_char(int ctx_id, uint32_t ch);

int kapi_ime_select_candidate(int ctx_id, int index);
int kapi_ime_page_up(int ctx_id);
int kapi_ime_page_down(int ctx_id);

int kapi_ime_reset(int ctx_id);
int kapi_ime_commit_preedit(int ctx_id);

kapi_ime_state_t kapi_ime_get_state(int ctx_id);
const kapi_ime_preedit_t* kapi_ime_get_preedit(int ctx_id);

int kapi_ime_toggle(int ctx_id);

int kapi_ime_register_engine(const char* name, kapi_ime_lang_t lang, void* engine_data);
int kapi_ime_unregister_engine(const char* name);

#ifdef __cplusplus
}
#endif

#endif