#include "kanvas_animator.h"
#include <string.h>

static kanvas_animator_t g_animator;

float kanvas_ease_out(float t)
{
    return 1.0f - (1.0f - t) * (1.0f - t);
}

float kanvas_ease_in(float t)
{
    return t * t;
}

float kanvas_ease_inout(float t)
{
    if (t < 0.5f) return 2.0f * t * t;
    return 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
}

float kanvas_ease_linear(float t)
{
    return t;
}

float kanvas_ease_spring(float t)
{
    float c = 2.5f;
    return 1.0f - __builtin_cosf(c * t * 3.14159265f) * (1.0f - t);
}

float kanvas_ease_bounce(float t)
{
    if (t < 1.0f / 2.75f) return 7.5625f * t * t;
    else if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    else if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    else { t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f; }
}

static float apply_easing(int easing, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    switch (easing) {
    case KANVAS_ANIM_EASE_OUT:   return kanvas_ease_out(t);
    case KANVAS_ANIM_EASE_IN:    return kanvas_ease_in(t);
    case KANVAS_ANIM_EASE_INOUT: return kanvas_ease_inout(t);
    case KANVAS_ANIM_SPRING:     return kanvas_ease_spring(t);
    case KANVAS_ANIM_BOUNCE:     return kanvas_ease_bounce(t);
    default:                     return kanvas_ease_linear(t);
    }
}

void kanvas_animator_init(void)
{
    memset(&g_animator, 0, sizeof(kanvas_animator_t));
    g_animator.next_id = 1;
    g_animator.initialized = true;
}

void kanvas_animator_shutdown(void)
{
    g_animator.count = 0;
    g_animator.initialized = false;
}

uint32_t kanvas_animator_start(kanvas_anim_type_t type, int easing,
                                uint32_t duration_ms, float from, float to,
                                void* target, void (*on_complete)(void*), void* user_data)
{
    if (!g_animator.initialized || g_animator.count >= KANVAS_ANIM_MAX) return 0;
    uint32_t idx = g_animator.count++;
    kanvas_anim_t* a = &g_animator.anims[idx];
    memset(a, 0, sizeof(kanvas_anim_t));
    a->id = g_animator.next_id++;
    a->type = type;
    a->easing = easing;
    a->duration_ms = duration_ms > 0 ? duration_ms : KANVAS_ANIM_DURATION_MS;
    a->from_val = from;
    a->to_val = to;
    a->current_val = from;
    a->active = true;
    a->completed = false;
    a->reverse = false;
    a->target = target;
    a->opacity = (type == KANVAS_ANIM_FADE_IN || type == KANVAS_ANIM_WINDOW_OPEN || type == KANVAS_ANIM_MENU_OPEN) ? 0.0f : 1.0f;
    a->scale = 1.0f;
    a->offset_x = 0;
    a->offset_y = 0;
    a->on_complete = on_complete;
    a->user_data = user_data;
    return a->id;
}

void kanvas_animator_cancel(uint32_t anim_id)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        if (g_animator.anims[i].id == anim_id) {
            g_animator.anims[i].active = false;
            g_animator.anims[i].completed = true;
            return;
        }
    }
}

void kanvas_animator_cancel_all(void)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        g_animator.anims[i].active = false;
        g_animator.anims[i].completed = true;
    }
    g_animator.count = 0;
}

void kanvas_animator_update(uint64_t now_ms)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        kanvas_anim_t* a = &g_animator.anims[i];
        if (!a->active) continue;

        if (a->start_ms == 0) a->start_ms = now_ms;

        uint64_t elapsed = now_ms - a->start_ms;
        float t = (float)elapsed / (float)a->duration_ms;
        if (t > 1.0f) t = 1.0f;

        float eased = apply_easing(a->easing, t);
        a->current_val = a->from_val + (a->to_val - a->from_val) * eased;

        switch (a->type) {
        case KANVAS_ANIM_FADE_IN:
            a->opacity = eased;
            break;
        case KANVAS_ANIM_FADE_OUT:
            a->opacity = 1.0f - eased;
            break;
        case KANVAS_ANIM_SLIDE_UP:
            a->offset_y = (int)((1.0f - eased) * (a->from_val - a->to_val));
            a->opacity = eased;
            break;
        case KANVAS_ANIM_SLIDE_DOWN:
            a->offset_y = (int)(eased * (a->to_val - a->from_val));
            a->opacity = eased;
            break;
        case KANVAS_ANIM_SLIDE_LEFT:
            a->offset_x = (int)((1.0f - eased) * (a->from_val - a->to_val));
            a->opacity = eased;
            break;
        case KANVAS_ANIM_SLIDE_RIGHT:
            a->offset_x = (int)(eased * (a->to_val - a->from_val));
            a->opacity = eased;
            break;
        case KANVAS_ANIM_SCALE_UP:
            a->scale = 0.8f + 0.2f * eased;
            a->opacity = eased;
            break;
        case KANVAS_ANIM_SCALE_DOWN:
            a->scale = 1.0f - 0.2f * eased;
            a->opacity = 1.0f - eased;
            break;
        case KANVAS_ANIM_POP:
            if (t < 0.5f) a->scale = 0.5f + 1.0f * (t * 2.0f);
            else a->scale = 1.5f - 0.5f * ((t - 0.5f) * 2.0f);
            a->opacity = eased;
            break;
        case KANVAS_ANIM_WINDOW_OPEN:
            a->scale = 0.92f + 0.08f * eased;
            a->opacity = eased;
            break;
        case KANVAS_ANIM_WINDOW_CLOSE:
            a->scale = 1.0f - 0.08f * eased;
            a->opacity = 1.0f - eased;
            break;
        case KANVAS_ANIM_MENU_OPEN:
            a->scale = 0.95f + 0.05f * eased;
            a->opacity = eased;
            a->offset_y = (int)((1.0f - eased) * 8);
            break;
        case KANVAS_ANIM_MENU_CLOSE:
            a->scale = 1.0f - 0.05f * eased;
            a->opacity = 1.0f - eased;
            a->offset_y = (int)(eased * 8);
            break;
        case KANVAS_ANIM_HOVER_LIFT:
            a->offset_y = (int)(-3.0f * eased);
            a->scale = 1.0f + 0.02f * eased;
            break;
        default:
            break;
        }

        if (t >= 1.0f) {
            a->active = false;
            a->completed = true;
            a->current_val = a->to_val;
            a->opacity = (a->type == KANVAS_ANIM_FADE_OUT || a->type == KANVAS_ANIM_WINDOW_CLOSE || a->type == KANVAS_ANIM_MENU_CLOSE) ? 0.0f : 1.0f;
            a->scale = 1.0f;
            a->offset_x = 0;
            a->offset_y = 0;
            if (a->on_complete) a->on_complete(a->user_data);
        }
    }

    uint32_t write = 0;
    for (uint32_t i = 0; i < g_animator.count; i++) {
        if (!g_animator.anims[i].completed) {
            if (write != i) g_animator.anims[write] = g_animator.anims[i];
            write++;
        }
    }
    g_animator.count = write;
}

float kanvas_animator_get_value(uint32_t anim_id)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        if (g_animator.anims[i].id == anim_id)
            return g_animator.anims[i].current_val;
    }
    return 0.0f;
}

bool kanvas_animator_is_active(uint32_t anim_id)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        if (g_animator.anims[i].id == anim_id)
            return g_animator.anims[i].active;
    }
    return false;
}

bool kanvas_animator_is_completed(uint32_t anim_id)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        if (g_animator.anims[i].id == anim_id)
            return g_animator.anims[i].completed;
    }
    return true;
}

kanvas_anim_t* kanvas_animator_get(uint32_t anim_id)
{
    for (uint32_t i = 0; i < g_animator.count; i++) {
        if (g_animator.anims[i].id == anim_id)
            return &g_animator.anims[i];
    }
    return NULL;
}

void kanvas_animator_apply_to_window(uint32_t anim_id, int* x, int* y, int* w, int* h, float* opacity)
{
    kanvas_anim_t* a = kanvas_animator_get(anim_id);
    if (!a || !a->active) return;
    if (x) *x += a->offset_x;
    if (y) *y += a->offset_y;
    if (opacity) *opacity = a->opacity;
    (void)w; (void)h;
}

void kanvas_animator_apply_to_menu(uint32_t anim_id, int* x, int* y, float* opacity, float* scale)
{
    kanvas_anim_t* a = kanvas_animator_get(anim_id);
    if (!a || !a->active) return;
    if (x) *x += a->offset_x;
    if (y) *y += a->offset_y;
    if (opacity) *opacity = a->opacity;
    if (scale) *scale = a->scale;
}

uint32_t kanvas_animator_fade_in(void* target, uint32_t duration_ms)
{
    return kanvas_animator_start(KANVAS_ANIM_FADE_IN, KANVAS_ANIM_EASE_OUT, duration_ms, 0.0f, 1.0f, target, NULL, NULL);
}

uint32_t kanvas_animator_fade_out(void* target, uint32_t duration_ms)
{
    return kanvas_animator_start(KANVAS_ANIM_FADE_OUT, KANVAS_ANIM_EASE_IN, duration_ms, 1.0f, 0.0f, target, NULL, NULL);
}

uint32_t kanvas_animator_slide_up(void* target, int from_y, int to_y, uint32_t duration_ms)
{
    return kanvas_animator_start(KANVAS_ANIM_SLIDE_UP, KANVAS_ANIM_EASE_OUT, duration_ms, (float)from_y, (float)to_y, target, NULL, NULL);
}

uint32_t kanvas_animator_slide_down(void* target, int from_y, int to_y, uint32_t duration_ms)
{
    return kanvas_animator_start(KANVAS_ANIM_SLIDE_DOWN, KANVAS_ANIM_EASE_OUT, duration_ms, (float)from_y, (float)to_y, target, NULL, NULL);
}

uint32_t kanvas_animator_window_open(void* target, int x, int y, int w, int h)
{
    uint32_t id = kanvas_animator_start(KANVAS_ANIM_WINDOW_OPEN, KANVAS_ANIM_EASE_OUT, 200, 0.0f, 1.0f, target, NULL, NULL);
    kanvas_anim_t* a = kanvas_animator_get(id);
    if (a) { a->target_x = x; a->target_y = y; a->target_w = w; a->target_h = h; }
    return id;
}

uint32_t kanvas_animator_window_close(void* target)
{
    return kanvas_animator_start(KANVAS_ANIM_WINDOW_CLOSE, KANVAS_ANIM_EASE_IN, 150, 1.0f, 0.0f, target, NULL, NULL);
}

uint32_t kanvas_animator_menu_open(void* target, int x, int y)
{
    uint32_t id = kanvas_animator_start(KANVAS_ANIM_MENU_OPEN, KANVAS_ANIM_EASE_OUT, 180, 0.0f, 1.0f, target, NULL, NULL);
    kanvas_anim_t* a = kanvas_animator_get(id);
    if (a) { a->target_x = x; a->target_y = y; }
    return id;
}

uint32_t kanvas_animator_menu_close(void* target)
{
    return kanvas_animator_start(KANVAS_ANIM_MENU_CLOSE, KANVAS_ANIM_EASE_IN, 120, 1.0f, 0.0f, target, NULL, NULL);
}

uint32_t kanvas_animator_pop_in(void* target)
{
    return kanvas_animator_start(KANVAS_ANIM_POP, KANVAS_ANIM_SPRING, 250, 0.0f, 1.0f, target, NULL, NULL);
}

uint32_t kanvas_animator_hover_lift(void* target)
{
    return kanvas_animator_start(KANVAS_ANIM_HOVER_LIFT, KANVAS_ANIM_EASE_OUT, 100, 0.0f, 1.0f, target, NULL, NULL);
}