#ifndef KAPI_GRAPHICS2D_H
#define KAPI_GRAPHICS2D_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Surface types */
#define KAPI_SURFACE_TYPE_UNKNOWN   0
#define KAPI_SURFACE_TYPE_WINDOW     1
#define KAPI_SURFACE_TYPE_BITMAP    2
#define KAPI_SURFACE_TYPE_PRIMITIVE 3
#define KAPI_SURFACE_TYPE_OFFSCREEN  4

/* Surface formats */
#define KAPI_SURFACE_FORMAT_UNKNOWN  0
#define KAPI_SURFACE_FORMAT_ARGB8888 1
#define KAPI_SURFACE_FORMAT_RGB888  2
#define KAPI_SURFACE_FORMAT_RGB565  3
#define KAPI_SURFACE_FORMAT_ARGB1555 4
#define KAPI_SURFACE_FORMAT_ARGB4444 5
#define KAPI_SURFACE_FORMAT_GRAY8   6
#define KAPI_SURFACE_FORMAT_MONO1   7

/* Drawing primitives */
#define KAPI_PRIMITIVE_TYPE_NONE     0
#define KAPI_PRIMITIVE_TYPE_POINT   1
#define KAPI_PRIMITIVE_TYPE_LINE    2
#define KAPI_PRIMITIVE_TYPE_RECT    3
#define KAPI_PRIMITIVE_TYPE_ROUND_RECT 4
#define KAPI_PRIMITIVE_TYPE_ELLIPSE 5
#define KAPI_PRIMITIVE_TYPE_ARC     6
#define KAPI_PRIMITIVE_TYPE_POLYGON 7
#define KAPI_PRIMITIVE_TYPE_BEZIER 8
#define KAPI_PRIMITIVE_TYPE_PATH    9

/* Fill modes */
#define KAPI_FILL_MODE_NONE         0
#define KAPI_FILL_MODE_SOLID       1
#define KAPI_FILL_MODE_STROKE      2
#define KAPI_FILL_MODE_GRADIENT    3

/* Line styles */
#define KAPI_LINE_STYLE_SOLID      0
#define KAPI_LINE_STYLE_DASHED     1
#define KAPI_LINE_STYLE_DOTTED     2
#define KAPI_LINE_STYLE_DASH_DOT   3

/* Blend modes */
#define KAPI_BLEND_MODE_NONE       0
#define KAPI_BLEND_MODE_NORMAL     1
#define KAPI_BLEND_MODE_MULTIPLY   2
#define KAPI_BLEND_MODE_SCREEN     3
#define KAPI_BLEND_MODE_OVERLAY    4
#define KAPI_BLEND_MODE_ADD        5
#define KAPI_BLEND_MODE_SUBTRACT  6

/* Text alignment */
#define KAPI_TEXT_ALIGN_LEFT       0
#define KAPI_TEXT_ALIGN_CENTER     1
#define KAPI_TEXT_ALIGN_RIGHT      2
#define KAPI_TEXT_ALIGN_TOP        0
#define KAPI_TEXT_ALIGN_MIDDLE     1
#define KAPI_TEXT_ALIGN_BOTTOM     2

/* Font styles */
#define KAPI_FONT_STYLE_NORMAL     0
#define KAPI_FONT_STYLE_BOLD       1
#define KAPI_FONT_STYLE_ITALIC     2
#define KAPI_FONT_STYLE_UNDERLINE  4
#define KAPI_FONT_STYLE_STRIKETHROUGH 8

/* Gradient types */
#define KAPI_GRADIENT_TYPE_NONE    0
#define KAPI_GRADIENT_TYPE_LINEAR  1
#define KAPI_GRADIENT_TYPE_RADIAL  2
#define KAPI_GRADIENT_TYPE_CONIC   3

/* Surface structure */
typedef struct kapi_surface kapi_surface_t;

/* Graphics context structure */
typedef struct kapi_graphics_context kapi_graphics_context_t;

/* Font structure */
typedef struct kapi_font kapi_font_t;

/* Path structure */
typedef struct kapi_path kapi_path_t;

/* Gradient structure */
typedef struct kapi_gradient kapi_gradient_t;

/* Image structure */
typedef struct kapi_image kapi_image_t;

/* Drawing primitive structure */
typedef struct kapi_primitive kapi_primitive_t;

/* Matrix structure */
typedef struct {
    float m[3][3];
} kapi_matrix_t;

/* Point array */
typedef struct {
    kapi_point_t* points;
    uint32_t count;
    uint32_t capacity;
} kapi_point_array_t;

/* Rect array */
typedef struct {
    kapi_rect_t* rects;
    uint32_t count;
    uint32_t capacity;
} kapi_rect_array_t;

/* Color stop structure (for gradients) */
typedef struct {
    kapi_color_t color;
    float position;  /* 0.0 to 1.0 */
} kapi_color_stop_t;

/* Gradient structure */
struct kapi_gradient {
    uint32_t type;
    uint32_t stop_count;
    kapi_color_stop_t* stops;
    kapi_point_t start_point;
    kapi_point_t end_point;
    kapi_vector3d_t center;
    kapi_vector3d_t radius;
    float angle;
    kapi_matrix_t transform;
};

/* Font structure */
struct kapi_font {
    char name[256];
    float size;
    uint32_t style;
    uint32_t weight;
    float line_height;
    float ascent;
    float descent;
    kapi_font_t* next;
    void* native_font;  /* Platform-specific font handle */
};

/* Surface structure */
struct kapi_surface {
    uint32_t id;
    uint32_t type;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t* data;
    kapi_surface_t* next;
    kapi_window_t* window;  /* For window surfaces */
    kapi_surface_t* parent; /* For offscreen surfaces */
    kapi_rect_t bounds;
    uint32_t ref_count;
    kapi_graphics_context_t* context;
    void* user_data;
};

/* Graphics context structure */
struct kapi_graphics_context {
    kapi_surface_t* surface;
    kapi_color_t stroke_color;
    kapi_color_t fill_color;
    kapi_color_t background_color;
    uint32_t stroke_width;
    uint32_t line_style;
    uint32_t fill_mode;
    uint32_t blend_mode;
    kapi_font_t* current_font;
    kapi_gradient_t* current_gradient;
    kapi_matrix_t transform;
    kapi_matrix_t view_transform;
    kapi_matrix_t projection_transform;
    uint32_t clip_enabled;
    kapi_rect_t clip_rect;
    kapi_point_t origin;
    kapi_surface_t* target_surface;
};

/* Path structure */
struct kapi_path {
    kapi_point_array_t points;
    kapi_point_array_t control_points;
    uint32_t commands;
    uint32_t subpath_count;
    kapi_rect_t bounds;
    kapi_surface_t* surface;
};

/* Image structure */
struct kapi_image {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint8_t* data;
    size_t data_size;
    kapi_surface_t* surface;
    kapi_image_t* next;
    char filename[512];
};

/* Drawing primitive structure */
struct kapi_primitive {
    uint32_t type;
    kapi_point_t* points;
    uint32_t point_count;
    kapi_color_t stroke_color;
    kapi_color_t fill_color;
    uint32_t stroke_width;
    uint32_t line_style;
    uint32_t fill_mode;
    kapi_gradient_t* fill_gradient;
    kapi_rect_t bounds;
    kapi_surface_t* surface;
};

/* Surface API */
int kapi_surface_create(kapi_surface_t** surface, uint32_t width, uint32_t height, 
                       uint32_t format, kapi_surface_t* parent);
int kapi_surface_destroy(kapi_surface_t* surface);

kapi_surface_t* kapi_surface_from_window(kapi_window_t* window);
kapi_surface_t* kapi_surface_create_offscreen(uint32_t width, uint32_t height, uint32_t format);

int kapi_surface_get_info(kapi_surface_t* surface, 
                         uint32_t* width, uint32_t* height, uint32_t* format, uint8_t** data);
int kapi_surface_set_data(kapi_surface_t* surface, const uint8_t* data, size_t size);
int kapi_surface_update(kapi_surface_t* surface, const kapi_rect_t* rect);

int kapi_surface_copy(kapi_surface_t* dest, kapi_surface_t* src, 
                     const kapi_rect_t* src_rect, const kapi_rect_t* dest_rect);
int kapi_surface_blend(kapi_surface_t* dest, kapi_surface_t* src, 
                      const kapi_rect_t* src_rect, const kapi_rect_t* dest_rect,
                      uint32_t blend_mode, uint8_t alpha);

/* Graphics context API */
int kapi_graphics_context_create(kapi_graphics_context_t** context, kapi_surface_t* surface);
int kapi_graphics_context_destroy(kapi_graphics_context_t* context);

int kapi_graphics_context_set_surface(kapi_graphics_context_t* context, kapi_surface_t* surface);
int kapi_graphics_context_get_surface(kapi_graphics_context_t* context, kapi_surface_t** surface);

int kapi_graphics_context_clear(kapi_graphics_context_t* context, const kapi_color_t* color);
int kapi_graphics_context_present(kapi_graphics_context_t* context);

/* Drawing color API */
int kapi_graphics_context_set_stroke_color(kapi_graphics_context_t* context, const kapi_color_t* color);
int kapi_graphics_context_set_fill_color(kapi_graphics_context_t* context, const kapi_color_t* color);
int kapi_graphics_context_set_background_color(kapi_graphics_context_t* context, const kapi_color_t* color);
int kapi_graphics_context_get_stroke_color(kapi_graphics_context_t* context, kapi_color_t* color);
int kapi_graphics_context_get_fill_color(kapi_graphics_context_t* context, kapi_color_t* color);

/* Drawing style API */
int kapi_graphics_context_set_stroke_width(kapi_graphics_context_t* context, uint32_t width);
int kapi_graphics_context_set_line_style(kapi_graphics_context_t* context, uint32_t style);
int kapi_graphics_context_set_fill_mode(kapi_graphics_context_t* context, uint32_t mode);
int kapi_graphics_context_set_blend_mode(kapi_graphics_context_t* context, uint32_t mode);

int kapi_graphics_context_get_stroke_width(kapi_graphics_context_t* context, uint32_t* width);
int kapi_graphics_context_get_line_style(kapi_graphics_context_t* context, uint32_t* style);
int kapi_graphics_context_get_fill_mode(kapi_graphics_context_t* context, uint32_t* mode);
int kapi_graphics_context_get_blend_mode(kapi_graphics_context_t* context, uint32_t* mode);

/* Transformation API */
int kapi_graphics_context_set_transform(kapi_graphics_context_t* context, const kapi_matrix_t* matrix);
int kapi_graphics_context_get_transform(kapi_graphics_context_t* context, kapi_matrix_t* matrix);
int kapi_graphics_context_translate(kapi_graphics_context_t* context, float tx, float ty);
int kapi_graphics_context_scale(kapi_graphics_context_t* context, float sx, float sy);
int kapi_graphics_context_rotate(kapi_graphics_context_t* context, float angle);
int kapi_graphics_context_skew(kapi_graphics_context_t* context, float skx, float sky);
int kapi_graphics_context_reset_transform(kapi_graphics_context_t* context);

/* Clipping API */
int kapi_graphics_context_enable_clip(kapi_graphics_context_t* context, const kapi_rect_t* rect);
int kapi_graphics_context_disable_clip(kapi_graphics_context_t* context);
int kapi_graphics_context_get_clip(kapi_graphics_context_t* context, kapi_rect_t* rect);

/* Primitive drawing API */
int kapi_graphics_context_draw_point(kapi_graphics_context_t* context, float x, float y);
int kapi_graphics_context_draw_line(kapi_graphics_context_t* context, float x1, float y1, float x2, float y2);
int kapi_graphics_context_draw_rect(kapi_graphics_context_t* context, float x, float y, float width, float height);
int kapi_graphics_context_draw_round_rect(kapi_graphics_context_t* context, float x, float y, float width, float height, float radius);
int kapi_graphics_context_draw_ellipse(kapi_graphics_context_t* context, float cx, float cy, float rx, float ry);
int kapi_graphics_context_draw_arc(kapi_graphics_context_t* context, float cx, float cy, float radius, float start_angle, float end_angle);

int kapi_graphics_context_fill_rect(kapi_graphics_context_t* context, float x, float y, float width, float height);
int kapi_graphics_context_fill_round_rect(kapi_graphics_context_t* context, float x, float y, float width, float height, float radius);
int kapi_graphics_context_fill_ellipse(kapi_graphics_context_t* context, float cx, float cy, float rx, float ry);
int kapi_graphics_context_fill_polygon(kapi_graphics_context_t* context, const kapi_point_t* points, uint32_t count);

/* Path API */
int kapi_path_create(kapi_path_t** path, kapi_surface_t* surface);
int kapi_path_destroy(kapi_path_t* path);

int kapi_path_move_to(kapi_path_t* path, float x, float y);
int kapi_path_line_to(kapi_path_t* path, float x, float y);
int kapi_path_quad_to(kapi_path_t* path, float cx, float cy, float x, float y);
int kapi_path_cubic_to(kapi_path_t* path, float cx1, float cy1, float cx2, float cy2, float x, float y);
int kapi_path_close(kapi_path_t* path);

int kapi_path_draw(kapi_graphics_context_t* context, kapi_path_t* path);
int kapi_path_fill(kapi_graphics_context_t* context, kapi_path_t* path);
int kapi_path_stroke(kapi_graphics_context_t* context, kapi_path_t* path);

/* Font API */
int kapi_font_create(kapi_font_t** font, const char* name, float size);
int kapi_font_destroy(kapi_font_t* font);

int kapi_font_set_style(kapi_font_t* font, uint32_t style, uint32_t weight);
int kapi_font_set_size(kapi_font_t* font, float size);

kapi_font_t* kapi_font_get_system_font(const char* name);
kapi_font_t* kapi_font_load_from_file(const char* filename, float size);

/* Text API */
int kapi_graphics_context_set_font(kapi_graphics_context_t* context, kapi_font_t* font);
int kapi_graphics_context_get_font(kapi_graphics_context_t* context, kapi_font_t** font);

int kapi_graphics_context_draw_text(kapi_graphics_context_t* context, const char* text, float x, float y);
int kapi_graphics_context_draw_text_ext(kapi_graphics_context_t* context, const char* text, float x, float y, 
                                       float width, uint32_t alignment);
int kapi_graphics_context_measure_text(kapi_graphics_context_t* context, const char* text, 
                                       float* width, float* height);

/* Image API */
int kapi_image_create(kapi_image_t** image, uint32_t width, uint32_t height, uint32_t format);
int kapi_image_destroy(kapi_image_t* image);

int kapi_image_load_from_file(kapi_image_t** image, const char* filename);
int kapi_image_save_to_file(kapi_image_t* image, const char* filename);

int kapi_graphics_context_draw_image(kapi_graphics_context_t* context, kapi_image_t* image, 
                                    float x, float y, float width, float height);
int kapi_graphics_context_draw_image_ext(kapi_graphics_context_t* context, kapi_image_t* image,
                                        const kapi_rect_t* src_rect, const kapi_rect_t* dest_rect);

/* Gradient API */
int kapi_gradient_create(kapi_gradient_t** gradient);
int kapi_gradient_destroy(kapi_gradient_t* gradient);

int kapi_gradient_set_type(kapi_gradient_t* gradient, uint32_t type);
int kapi_gradient_set_linear(kapi_gradient_t* gradient, float x1, float y1, float x2, float y2);
int kapi_gradient_set_radial(kapi_gradient_t* gradient, float cx, float cy, float r);
int kapi_gradient_add_color_stop(kapi_gradient_t* gradient, const kapi_color_t* color, float position);

int kapi_graphics_context_set_gradient(kapi_graphics_context_t* context, kapi_gradient_t* gradient);
int kapi_graphics_context_fill_gradient(kapi_graphics_context_t* context, const kapi_rect_t* rect);

/* Primitive API */
int kapi_primitive_create(kapi_primitive_t** primitive);
int kapi_primitive_destroy(kapi_primitive_t* primitive);

int kapi_primitive_set_type(kapi_primitive_t* primitive, uint32_t type);
int kapi_primitive_set_points(kapi_primitive_t* primitive, const kapi_point_t* points, uint32_t count);
int kapi_primitive_set_colors(kapi_primitive_t* primitive, const kapi_color_t* stroke_color, const kapi_color_t* fill_color);
int kapi_primitive_set_width(kapi_primitive_t* primitive, uint32_t stroke_width);
int kapi_primitive_set_style(kapi_primitive_t* primitive, uint32_t line_style, uint32_t fill_mode);
int kapi_primitive_set_gradient(kapi_primitive_t* primitive, kapi_gradient_t* gradient);

int kapi_graphics_context_draw_primitive(kapi_graphics_context_t* context, kapi_primitive_t* primitive);
int kapi_graphics_context_fill_primitive(kapi_graphics_context_t* context, kapi_primitive_t* primitive);

/* Advanced drawing functions */
int kapi_graphics_context_draw_grid(kapi_graphics_context_t* context, float x, float y, float width, float height, 
                                  float cell_width, float cell_height);
int kapi_graphics_context_draw_progress_bar(kapi_graphics_context_t* context, float x, float y, float width, float height, 
                                            float progress);
int kapi_graphics_context_draw_button(kapi_graphics_context_t* context, float x, float y, float width, float height,
                                     const char* text, uint32_t state);
int kapi_graphics_context_draw_scrollbar(kapi_graphics_context_t* context, float x, float y, float width, float height,
                                       float min_value, float max_value, float current_value);

/* 3D transformation functions */
int kapi_graphics_context_transform_point3d(kapi_graphics_context_t* context, const kapi_vector3d_t* point, 
                                           kapi_point_t* screen_point);
int kapi_graphics_context_untransform_point3d(kapi_graphics_context_t* context, const kapi_point_t* screen_point,
                                            kapi_vector3d_t* world_point);

/* Matrix operations */
int kapi_matrix_identity(kapi_matrix_t* matrix);
int kapi_matrix_multiply(kapi_matrix_t* result, const kapi_matrix_t* a, const kapi_matrix_t* b);
int kapi_matrix_translate(kapi_matrix_t* matrix, float tx, float ty);
int kapi_matrix_scale(kapi_matrix_t* matrix, float sx, float sy);
int kapi_matrix_rotate(kapi_matrix_t* matrix, float angle);
int kapi_matrix_invert(kapi_matrix_t* matrix);

/* Debugging API */
int kapi_graphics_context_draw_debug_info(kapi_graphics_context_t* context);
int kapi_graphics_context_dump_surface_info(kapi_surface_t* surface);

#ifdef __cplusplus
}
#endif

#endif