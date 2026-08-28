#ifndef KERNEL_FONT_TTF_H
#define KERNEL_FONT_TTF_H

#include <arch/types.h>

#define TTF_TAG(c1,c2,c3,c4) ((uint32_t)(c1)<<24|(uint32_t)(c2)<<16|(uint32_t)(c3)<<8|(c4))

#define TTF_HEAD_TAG   TTF_TAG('h','e','a','d')
#define TTF_HHEA_TAG   TTF_TAG('h','h','e','a')
#define TTF_MAXP_TAG   TTF_TAG('m','a','x','p')
#define TTF_CMAP_TAG   TTF_TAG('c','m','a','p')
#define TTF_LOCA_TAG   TTF_TAG('l','o','c','a')
#define TTF_GLYF_TAG   TTF_TAG('g','l','y','f')
#define TTF_NAME_TAG   TTF_TAG('n','a','m','e')
#define TTF_POST_TAG   TTF_TAG('p','o','s','t')
#define TTF_OSF2_TAG   TTF_TAG('O','S','/','2')
#define TTF_HMTX_TAG   TTF_TAG('h','m','t','x')
#define TTF_KERN_TAG   TTF_TAG('k','e','r','n')

#define TTF_CMAP_FORMAT0   0
#define TTF_CMAP_FORMAT4   4
#define TTF_CMAP_FORMAT6   6
#define TTF_CMAP_FORMAT12  12

#define TTF_GLYF_SIMPLE    1
#define TTF_GLYF_COMPOUND  2

#define TTF_POINT_ON_CURVE   0x01
#define TTF_X_SHORT_VECTOR   0x02
#define TTF_Y_SHORT_VECTOR   0x04
#define TTF_REPEAT_FLAG      0x08
#define TTF_X_IS_SAME        0x10
#define TTF_Y_IS_SAME        0x20

typedef struct {
    uint16_t major_version;
    uint16_t minor_version;
    int16_t  font_revision_maj;
    int16_t  font_revision_min;
    uint32_t checksum_adjustment;
    uint32_t magic_number;
    uint16_t flags;
    uint16_t units_per_em;
    int16_t  created_hi;
    int16_t  created_lo;
    int16_t  modified_hi;
    int16_t  modified_lo;
    int16_t  x_min;
    int16_t  y_min;
    int16_t  x_max;
    int16_t  y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_ppem;
    int16_t  font_direction_hint;
    int16_t  index_to_loc_format;
    int16_t  glyph_data_format;
} ttf_head_t;

typedef struct {
    int16_t  ascender;
    int16_t  descender;
    int16_t  line_gap;
    uint16_t advance_width_max;
    int16_t  min_left_side_bearing;
    int16_t  min_right_side_bearing;
    int16_t  x_max_extent;
    int16_t  caret_slope_rise;
    int16_t  caret_slope_run;
    int16_t  caret_offset;
    int16_t  reserved[4];
    int16_t  metric_data_format;
    uint16_t num_of_long_hor_metrics;
} ttf_hhea_t;

typedef struct {
    uint16_t version;
    uint16_t num_glyphs;
    uint16_t max_points;
    uint16_t max_contours;
    uint16_t max_composite_points;
    uint16_t max_composite_contours;
    uint16_t max_zones;
    uint16_t max_twilight_points;
    uint16_t max_storage;
    uint16_t max_function_defs;
    uint16_t max_instruction_defs;
    uint16_t max_stack_elements;
    uint16_t max_size_of_instructions;
    uint16_t max_component_elements;
    uint16_t max_component_depth;
} ttf_maxp_t;

typedef struct {
    int16_t  x;
    int16_t  y;
    uint8_t  on_curve;
} ttf_point_t;

typedef struct {
    ttf_point_t* points;
    uint16_t     num_points;
    uint16_t*    end_points;
    uint16_t     num_contours;
    int16_t      x_min;
    int16_t      y_min;
    int16_t      x_max;
    int16_t      y_max;
} ttf_glyph_t;

typedef struct {
    uint8_t*  data;
    uint32_t  size;
    uint32_t  num_tables;

    ttf_head_t  head;
    ttf_hhea_t  hhea;
    ttf_maxp_t  maxp;

    uint16_t*  cmap;
    uint32_t   cmap_size;
    uint16_t   cmap_format;
    uint16_t   cmap_platform;
    uint16_t   cmap_encoding;

    uint32_t*  loca_long;
    uint16_t*  loca_short;
    int        loca_is_long;

    uint8_t*   glyf_data;
    uint32_t   glyf_size;

    int16_t*   hmtx_advance;
    int16_t*   hmtx_lsb;
    uint16_t   hmtx_num_long;

    int16_t    ascender;
    int16_t    descender;
    int16_t    line_gap;
    uint16_t   units_per_em;
} ttf_font_t;

typedef struct {
    uint8_t* pixels;
    int      width;
    int      height;
    int      stride;
} ttf_bitmap_t;

int      ttf_load(ttf_font_t* font, const uint8_t* data, uint32_t size);
void     ttf_free(ttf_font_t* font);
uint16_t ttf_get_glyph_index(ttf_font_t* font, uint32_t codepoint);
int      ttf_get_glyph_outline(ttf_font_t* font, uint16_t glyph_index, ttf_glyph_t* glyph);
void     ttf_free_glyph_outline(ttf_glyph_t* glyph);
int16_t  ttf_get_advance_width(ttf_font_t* font, uint16_t glyph_index);
int      ttf_render_glyph(ttf_font_t* font, uint16_t glyph_index, int pixel_size,
                           ttf_bitmap_t* bitmap);
int      ttf_render_codepoint(ttf_font_t* font, uint32_t codepoint, int pixel_size,
                               ttf_bitmap_t* bitmap);
int      ttf_measure_text(ttf_font_t* font, const uint32_t* codepoints, int count,
                           int pixel_size, int* out_width, int* out_height);

#endif