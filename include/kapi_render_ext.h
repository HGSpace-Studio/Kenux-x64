#ifndef KAPI_RENDER_EXT_H
#define KAPI_RENDER_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kapi_graphics2d.h"
#include "kapi_window.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_AA_NONE      0
#define KAPI_AA_GRAY      1
#define KAPI_AA_SUBPIXEL  2

#define KAPI_COMPOSITE_SRC_OVER  0
#define KAPI_COMPOSITE_SRC_COPY  1
#define KAPI_COMPOSITE_MULTIPLY  2
#define KAPI_COMPOSITE_SCREEN    3
#define KAPI_COMPOSITE_OVERLAY   4
#define KAPI_COMPOSITE_DARKEN    5
#define KAPI_COMPOSITE_LIGHTEN   6
#define KAPI_COMPOSITE_ADD       7

#define KAPI_EASE_LINEAR       0
#define KAPI_EASE_IN_QUAD      1
#define KAPI_EASE_OUT_QUAD     2
#define KAPI_EASE_IN_OUT_QUAD  3
#define KAPI_EASE_IN_CUBIC     4
#define KAPI_EASE_OUT_CUBIC    5
#define KAPI_EASE_IN_OUT_CUBIC 6
#define KAPI_EASE_IN_QUART     7
#define KAPI_EASE_OUT_QUART    8
#define KAPI_EASE_IN_EXPO      9
#define KAPI_EASE_OUT_EXPO    10
#define KAPI_EASE_IN_OUT_EXPO 11
#define KAPI_EASE_IN_BACK     12
#define KAPI_EASE_OUT_BACK    13
#define KAPI_EASE_IN_OUT_BACK 14
#define KAPI_EASE_IN_ELASTIC  15
#define KAPI_EASE_OUT_ELASTIC 16
#define KAPI_EASE_IN_BOUNCE   17
#define KAPI_EASE_OUT_BOUNCE  18
#define KAPI_EASE_SPRING      19

typedef struct kapi_dpi_info {
    float dpi;
    float scale_x;
    float scale_y;
    int32_t scaled_width;
    int32_t scaled_height;
    int32_t physical_width;
    int32_t physical_height;
} kapi_dpi_info_t;

typedef struct kapi_aa_line {
    float x0, y0, x1, y1;
    float width;
    uint32_t color;
} kapi_aa_line_t;

typedef struct kapi_aa_circle {
    float cx, cy, r;
    uint32_t color;
    bool filled;
} kapi_aa_circle_t;

typedef struct kapi_aa_ellipse {
    float cx, cy, rx, ry;
    uint32_t color;
    bool filled;
} kapi_aa_ellipse_t;

typedef struct kapi_aa_round_rect {
    float x, y, w, h, r;
    uint32_t color;
    bool filled;
} kapi_aa_round_rect_t;

typedef struct kapi_aa_arc {
    float cx, cy, r;
    float start_angle, end_angle;
    uint32_t color;
} kapi_aa_arc_t;

typedef struct kapi_aa_bezier {
    float x0, y0;
    float cx0, cy0;
    float cx1, cy1;
    float x1, y1;
    uint32_t color;
    float width;
} kapi_aa_bezier_t;

typedef struct kapi_shadow_params {
    int32_t offset_x;
    int32_t offset_y;
    int32_t blur_radius;
    uint8_t alpha;
    uint32_t color;
    int32_t corner_radius;
} kapi_shadow_params_t;

typedef struct kapi_buffer_region {
    int32_t x, y;
    int32_t width, height;
} kapi_buffer_region_t;

int kapi_render_set_dpi(float dpi);
float kapi_render_get_dpi(void);
int kapi_render_set_dpi_for_window(kapi_window_t* window, float dpi);
float kapi_render_get_dpi_for_window(kapi_window_t* window);
int kapi_render_get_dpi_info(kapi_dpi_info_t* info);
int32_t kapi_render_scale_value(int32_t value, float dpi);
int32_t kapi_render_unscale_value(int32_t value, float dpi);
kapi_rect_t kapi_render_scale_rect(kapi_rect_t rect, float dpi);
kapi_rect_t kapi_render_unscale_rect(kapi_rect_t rect, float dpi);

int kapi_render_set_aa_mode(kapi_surface_t* surface, uint32_t mode);
uint32_t kapi_render_get_aa_mode(kapi_surface_t* surface);

int kapi_render_set_composite_mode(kapi_surface_t* surface, uint32_t mode);
uint32_t kapi_render_get_composite_mode(kapi_surface_t* surface);

int kapi_render_draw_line_aa(kapi_surface_t* surface, const kapi_aa_line_t* line);
int kapi_render_draw_circle_aa(kapi_surface_t* surface, const kapi_aa_circle_t* circle);
int kapi_render_draw_ellipse_aa(kapi_surface_t* surface, const kapi_aa_ellipse_t* ellipse);
int kapi_render_draw_round_rect_aa(kapi_surface_t* surface, const kapi_aa_round_rect_t* rr);
int kapi_render_draw_arc_aa(kapi_surface_t* surface, const kapi_aa_arc_t* arc);
int kapi_render_draw_bezier_aa(kapi_surface_t* surface, const kapi_aa_bezier_t* bezier);
int kapi_render_draw_polygon_aa(kapi_surface_t* surface, const kapi_point_t* points,
                                uint32_t count, uint32_t color, bool filled);

int kapi_render_draw_shadow(kapi_surface_t* surface, int32_t x, int32_t y,
                            int32_t w, int32_t h, const kapi_shadow_params_t* params);

int kapi_render_draw_thick_line(kapi_surface_t* surface, int32_t x1, int32_t y1,
                                int32_t x2, int32_t y2, uint32_t color, uint32_t thickness);
int kapi_render_draw_dashed_line(kapi_surface_t* surface, int32_t x1, int32_t y1,
                                 int32_t x2, int32_t y2, uint32_t color,
                                 uint32_t dash_len, uint32_t gap_len);
int kapi_render_draw_dashed_rect(kapi_surface_t* surface, int32_t x, int32_t y,
                                 int32_t w, int32_t h, uint32_t color,
                                 uint32_t dash_len, uint32_t gap_len);

int kapi_render_read_buffer(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                            uint32_t* buffer, size_t buffer_size);
int kapi_render_write_buffer(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                             const uint32_t* buffer, size_t buffer_size);
int kapi_render_read_buffer_rgba(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                                 uint8_t* buffer, size_t buffer_size);
int kapi_render_write_buffer_rgba(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                                  const uint8_t* buffer, size_t buffer_size);

int kapi_render_scroll_region(kapi_surface_t* surface, int32_t dx, int32_t dy,
                              const kapi_buffer_region_t* clip);

float kapi_ease_eval(uint32_t easing, float t);
const char* kapi_ease_name(uint32_t easing);

float kapi_ease_linear(float t);
float kapi_ease_in_quad(float t);
float kapi_ease_out_quad(float t);
float kapi_ease_in_out_quad(float t);
float kapi_ease_in_cubic(float t);
float kapi_ease_out_cubic(float t);
float kapi_ease_in_out_cubic(float t);
float kapi_ease_in_quart(float t);
float kapi_ease_out_quart(float t);
float kapi_ease_in_expo(float t);
float kapi_ease_out_expo(float t);
float kapi_ease_in_out_expo(float t);
float kapi_ease_in_back(float t);
float kapi_ease_out_back(float t);
float kapi_ease_in_out_back(float t);
float kapi_ease_in_elastic(float t);
float kapi_ease_out_elastic(float t);
float kapi_ease_in_bounce(float t);
float kapi_ease_out_bounce(float t);
float kapi_ease_spring(float t);

#ifdef __cplusplus
}
#endif

#endif