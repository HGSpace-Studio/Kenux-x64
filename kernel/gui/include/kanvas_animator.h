#ifndef KANVAS_ANIMATOR_H
#define KANVAS_ANIMATOR_H

#include "kapi_kanvasui.h"
#include <stdint.h>
#include <stdbool.h>

#define KANVAS_ANIM_MAX         256
#define KANVAS_ANIM_DURATION_MS 300
#define KANVAS_ANIM_EASE_OUT    0
#define KANVAS_ANIM_EASE_IN     1
#define KANVAS_ANIM_EASE_INOUT  2
#define KANVAS_ANIM_LINEAR      3
#define KANVAS_ANIM_SPRING      4
#define KANVAS_ANIM_BOUNCE      5

typedef enum {
    KANVAS_ANIM_NONE = 0,
    KANVAS_ANIM_FADE_IN,
    KANVAS_ANIM_FADE_OUT,
    KANVAS_ANIM_SLIDE_UP,
    KANVAS_ANIM_SLIDE_DOWN,
    KANVAS_ANIM_SLIDE_LEFT,
    KANVAS_ANIM_SLIDE_RIGHT,
    KANVAS_ANIM_SCALE_UP,
    KANVAS_ANIM_SCALE_DOWN,
    KANVAS_ANIM_POP,
    KANVAS_ANIM_FLY_IN_TOP,
    KANVAS_ANIM_FLY_IN_BOTTOM,
    KANVAS_ANIM_BLUR_IN,
    KANVAS_ANIM_BLUR_OUT,
    KANVAS_ANIM_WINDOW_OPEN,
    KANVAS_ANIM_WINDOW_CLOSE,
    KANVAS_ANIM_MENU_OPEN,
    KANVAS_ANIM_MENU_CLOSE,
    KANVAS_ANIM_HOVER_LIFT,
    KANVAS_ANIM_COUNT
} kanvas_anim_type_t;

typedef struct {
    uint32_t id;
    kanvas_anim_type_t type;
    int easing;
    uint64_t start_ms;
    uint32_t duration_ms;
    float from_val;
    float to_val;
    float current_val;
    bool active;
    bool completed;
    bool reverse;
    void* target;
    int target_x, target_y;
    int target_w, target_h;
    float opacity;
    float scale;
    int offset_x, offset_y;
    void (*on_complete)(void* user_data);
    void* user_data;
} kanvas_anim_t;

typedef struct {
    kanvas_anim_t anims[KANVAS_ANIM_MAX];
    uint32_t count;
    uint32_t next_id;
    bool initialized;
} kanvas_animator_t;

void kanvas_animator_init(void);
void kanvas_animator_shutdown(void);

uint32_t kanvas_animator_start(kanvas_anim_type_t type, int easing,
                                uint32_t duration_ms, float from, float to,
                                void* target, void (*on_complete)(void*), void* user_data);

void kanvas_animator_cancel(uint32_t anim_id);
void kanvas_animator_cancel_all(void);

void kanvas_animator_update(uint64_t now_ms);

float kanvas_animator_get_value(uint32_t anim_id);
bool kanvas_animator_is_active(uint32_t anim_id);
bool kanvas_animator_is_completed(uint32_t anim_id);

kanvas_anim_t* kanvas_animator_get(uint32_t anim_id);

float kanvas_ease_out(float t);
float kanvas_ease_in(float t);
float kanvas_ease_inout(float t);
float kanvas_ease_linear(float t);
float kanvas_ease_spring(float t);
float kanvas_ease_bounce(float t);

void kanvas_animator_apply_to_window(uint32_t anim_id, int* x, int* y, int* w, int* h, float* opacity);
void kanvas_animator_apply_to_menu(uint32_t anim_id, int* x, int* y, float* opacity, float* scale);

uint32_t kanvas_animator_fade_in(void* target, uint32_t duration_ms);
uint32_t kanvas_animator_fade_out(void* target, uint32_t duration_ms);
uint32_t kanvas_animator_slide_up(void* target, int from_y, int to_y, uint32_t duration_ms);
uint32_t kanvas_animator_slide_down(void* target, int from_y, int to_y, uint32_t duration_ms);
uint32_t kanvas_animator_window_open(void* target, int x, int y, int w, int h);
uint32_t kanvas_animator_window_close(void* target);
uint32_t kanvas_animator_menu_open(void* target, int x, int y);
uint32_t kanvas_animator_menu_close(void* target);
uint32_t kanvas_animator_pop_in(void* target);
uint32_t kanvas_animator_hover_lift(void* target);

#endif