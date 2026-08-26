#include "kapi_ime.h"
#include "kapi.h"
#include <string.h>

static kapi_ime_context_t contexts[KAPI_IME_MAX_INPUT_CTX];
static int context_count = 0;
static int focused_ctx = -1;

int kapi_ime_init(void)
{
    memset(contexts, 0, sizeof(contexts));
    context_count = 0;
    focused_ctx = -1;
    return 0;
}

void kapi_ime_cleanup(void)
{
    for (int i = 0; i < context_count; i++) {
        contexts[i].active = 0;
    }
    context_count = 0;
    focused_ctx = -1;
}

int kapi_ime_create_context(int window_id, kapi_ime_callback_fn callback, void* user_data)
{
    if (context_count >= KAPI_IME_MAX_INPUT_CTX) return -1;
    int idx = context_count++;
    kapi_ime_context_t* ctx = &contexts[idx];
    memset(ctx, 0, sizeof(kapi_ime_context_t));
    ctx->id = idx;
    ctx->window_id = window_id;
    ctx->state = KAPI_IME_STATE_INACTIVE;
    ctx->language = KAPI_IME_LANG_EN;
    ctx->mode = KAPI_IME_MODE_DIRECT;
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->active = 1;
    return idx;
}

int kapi_ime_destroy_context(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    contexts[ctx_id].active = 0;
    if (focused_ctx == ctx_id) focused_ctx = -1;
    return 0;
}

int kapi_ime_focus_context(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count || !contexts[ctx_id].active) return -1;
    if (focused_ctx >= 0 && focused_ctx != ctx_id) {
        kapi_ime_context_t* old = &contexts[focused_ctx];
        if (old->callback) {
            kapi_ime_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = KAPI_IME_EVENT_BLUR;
            old->callback(&ev, old->user_data);
        }
    }
    focused_ctx = ctx_id;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    if (ctx->callback) {
        kapi_ime_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KAPI_IME_EVENT_FOCUS;
        ctx->callback(&ev, ctx->user_data);
    }
    return 0;
}

int kapi_ime_blur_context(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    if (focused_ctx == ctx_id) {
        kapi_ime_context_t* ctx = &contexts[ctx_id];
        if (ctx->callback) {
            kapi_ime_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = KAPI_IME_EVENT_BLUR;
            ctx->callback(&ev, ctx->user_data);
        }
        focused_ctx = -1;
    }
    return 0;
}

int kapi_ime_enable(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    contexts[ctx_id].state = KAPI_IME_STATE_ACTIVE;
    return 0;
}

int kapi_ime_disable(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    contexts[ctx_id].state = KAPI_IME_STATE_INACTIVE;
    memset(&contexts[ctx_id].preedit, 0, sizeof(kapi_ime_preedit_t));
    return 0;
}

bool kapi_ime_is_enabled(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return false;
    return contexts[ctx_id].state != KAPI_IME_STATE_INACTIVE;
}

int kapi_ime_set_language(int ctx_id, kapi_ime_lang_t lang)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    kapi_ime_lang_t old = ctx->language;
    ctx->language = lang;
    if (old != lang && ctx->callback) {
        kapi_ime_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KAPI_IME_EVENT_LANG_CHANGE;
        ev.data.lang_change.old_lang = old;
        ev.data.lang_change.new_lang = lang;
        ctx->callback(&ev, ctx->user_data);
    }
    return 0;
}

kapi_ime_lang_t kapi_ime_get_language(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return KAPI_IME_LANG_NONE;
    return contexts[ctx_id].language;
}

int kapi_ime_set_mode(int ctx_id, kapi_ime_mode_t mode)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    kapi_ime_mode_t old = ctx->mode;
    ctx->mode = mode;
    if (old != mode && ctx->callback) {
        kapi_ime_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KAPI_IME_EVENT_MODE_CHANGE;
        ev.data.mode_change.old_mode = old;
        ev.data.mode_change.new_mode = mode;
        ctx->callback(&ev, ctx->user_data);
    }
    return 0;
}

kapi_ime_mode_t kapi_ime_get_mode(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return KAPI_IME_MODE_NONE;
    return contexts[ctx_id].mode;
}

int kapi_ime_set_candidate_rect(int ctx_id, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    contexts[ctx_id].candidate_rect = (kapi_ime_rect_t){x, y, w, h};
    return 0;
}

int kapi_ime_process_key(int ctx_id, uint32_t key, uint32_t modifiers, bool down)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    if (ctx->state == KAPI_IME_STATE_INACTIVE) return 0;
    (void)key; (void)modifiers; (void)down;
    return 0;
}

int kapi_ime_process_char(int ctx_id, uint32_t ch)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    if (ctx->state == KAPI_IME_STATE_INACTIVE) return 0;
    if (ctx->mode == KAPI_IME_MODE_DIRECT) {
        if (ctx->callback) {
            kapi_ime_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = KAPI_IME_EVENT_COMMIT;
            ev.data.commit.text[0] = (char)ch;
            ev.data.commit.length = 1;
            ctx->callback(&ev, ctx->user_data);
        }
        return 1;
    }
    if (ctx->preedit.cursor_pos < KAPI_IME_MAX_PREEDIT - 1) {
        ctx->preedit.text[ctx->preedit.cursor_pos] = (char)ch;
        ctx->preedit.cursor_pos++;
        ctx->state = KAPI_IME_STATE_COMPOSING;
        if (ctx->callback) {
            kapi_ime_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = KAPI_IME_EVENT_PREEDIT;
            ev.data.preedit = ctx->preedit;
            ctx->callback(&ev, ctx->user_data);
        }
    }
    return 1;
}

int kapi_ime_select_candidate(int ctx_id, int index)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    if (ctx->callback) {
        kapi_ime_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KAPI_IME_EVENT_CANDIDATE;
        ev.data.candidates.selected_index = index;
        ctx->callback(&ev, ctx->user_data);
    }
    return 0;
}

int kapi_ime_page_up(int ctx_id) { (void)ctx_id; return 0; }
int kapi_ime_page_down(int ctx_id) { (void)ctx_id; return 0; }

int kapi_ime_reset(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    memset(&contexts[ctx_id].preedit, 0, sizeof(kapi_ime_preedit_t));
    contexts[ctx_id].state = KAPI_IME_STATE_ACTIVE;
    return 0;
}

int kapi_ime_commit_preedit(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    kapi_ime_context_t* ctx = &contexts[ctx_id];
    if (ctx->preedit.cursor_pos > 0 && ctx->callback) {
        kapi_ime_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = KAPI_IME_EVENT_COMMIT;
        size_t len = kapi_strlen(ctx->preedit.text);
        if (len >= KAPI_IME_MAX_COMMIT) len = KAPI_IME_MAX_COMMIT - 1;
        memcpy(ev.data.commit.text, ctx->preedit.text, len);
        ev.data.commit.length = len;
        ctx->callback(&ev, ctx->user_data);
    }
    memset(&ctx->preedit, 0, sizeof(kapi_ime_preedit_t));
    ctx->state = KAPI_IME_STATE_ACTIVE;
    return 0;
}

kapi_ime_state_t kapi_ime_get_state(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return KAPI_IME_STATE_INACTIVE;
    return contexts[ctx_id].state;
}

const kapi_ime_preedit_t* kapi_ime_get_preedit(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return NULL;
    return &contexts[ctx_id].preedit;
}

int kapi_ime_toggle(int ctx_id)
{
    if (ctx_id < 0 || ctx_id >= context_count) return -1;
    if (contexts[ctx_id].state == KAPI_IME_STATE_INACTIVE)
        return kapi_ime_enable(ctx_id);
    return kapi_ime_disable(ctx_id);
}

int kapi_ime_register_engine(const char* name, kapi_ime_lang_t lang, void* engine_data)
{
    (void)name; (void)lang; (void)engine_data;
    return 0;
}

int kapi_ime_unregister_engine(const char* name)
{
    (void)name;
    return 0;
}