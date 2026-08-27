#ifndef KANVAS_KEX_RUNNER_H
#define KANVAS_KEX_RUNNER_H

#include "kapi_kanvasui.h"
#include <stdint.h>
#include <stdbool.h>

#define KEX_RUNNER_MAX_PATH      256
#define KEX_RUNNER_MAX_ARGS      16
#define KEX_RUNNER_MAX_OUTPUT    4096
#define KEX_RUNNER_MAX_HISTORY   32

typedef enum {
    KEX_FORMAT_UNKNOWN = 0,
    KEX_FORMAT_KEX,
    KEX_FORMAT_KXP,
    KEX_FORMAT_ELF
} kex_format_t;

typedef enum {
    KEX_STATE_IDLE = 0,
    KEX_STATE_VALIDATING,
    KEX_STATE_INSTALLING,
    KEX_STATE_RUNNING,
    KEX_STATE_COMPLETED,
    KEX_STATE_ERROR
} kex_runner_state_t;

typedef struct {
    char path[KEX_RUNNER_MAX_PATH];
    char name[64];
    kex_format_t format;
    uint32_t size;
    uint32_t version;
    bool executable;
    bool installed;
} kex_package_info_t;

typedef struct {
    char path[KEX_RUNNER_MAX_PATH];
    char args[KEX_RUNNER_MAX_ARGS][64];
    int arg_count;
    kex_runner_state_t state;
    kex_format_t format;
    int exit_code;
    uint32_t pid;
    uint64_t start_time;
    uint64_t end_time;
    char output[KEX_RUNNER_MAX_OUTPUT];
    int output_len;
    float progress;
    bool use_gfx;
    int gfx_w, gfx_h;
} kex_execution_t;

typedef struct {
    kex_execution_t current;
    kex_execution_t history[KEX_RUNNER_MAX_HISTORY];
    int history_count;
    bool window_visible;
    int window_x, window_y, window_w, window_h;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t accent_color;
    kui_color_t success_color;
    kui_color_t error_color;
    uint32_t anim_id;
} kanvas_kex_runner_t;

kanvas_kex_runner_t* kanvas_kex_runner_create(void);
void kanvas_kex_runner_destroy(kanvas_kex_runner_t* runner);

void kanvas_kex_runner_paint(kanvas_kex_runner_t* runner, uint32_t* fb, int stride, int fw, int fh);
void kanvas_kex_runner_handle_mouse(kanvas_kex_runner_t* runner, int mx, int my, bool left_down, bool left_up);
void kanvas_kex_runner_update(kanvas_kex_runner_t* runner, uint64_t now_ms);

kex_format_t kanvas_kex_detect_format(const char* path);
bool kanvas_kex_validate_package(const char* path, kex_package_info_t* info);

int kanvas_kex_install(kanvas_kex_runner_t* runner, const char* path);
int kanvas_kex_run(kanvas_kex_runner_t* runner, const char* path, bool use_gfx, int gfx_w, int gfx_h);
int kanvas_kex_run_with_args(kanvas_kex_runner_t* runner, const char* path, const char** args, int argc);
void kanvas_kex_stop(kanvas_kex_runner_t* runner);
void kanvas_kex_uninstall(kanvas_kex_runner_t* runner, const char* name);

void kanvas_kex_show_window(kanvas_kex_runner_t* runner);
void kanvas_kex_hide_window(kanvas_kex_runner_t* runner);

const char* kanvas_kex_format_name(kex_format_t format);
const char* kanvas_kex_state_name(kex_runner_state_t state);

#endif