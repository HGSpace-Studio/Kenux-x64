#include "ttf.h"
#include <string.h>
#include <math.h>

static uint16_t ttf_read_u16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static int16_t  ttf_read_i16(const uint8_t* p) { return (int16_t)((p[0] << 8) | p[1]); }
static uint32_t ttf_read_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static const uint8_t* ttf_find_table(const uint8_t* data, uint32_t num_tables, uint32_t tag)
{
    for (uint32_t i = 0; i < num_tables; i++) {
        const uint8_t* entry = data + 12 + i * 16;
        uint32_t entry_tag = ttf_read_u32(entry);
        if (entry_tag == tag) {
            uint32_t offset = ttf_read_u32(entry + 8);
            return data + offset;
        }
    }
    return NULL;
}

static int ttf_parse_head(ttf_font_t* font, const uint8_t* data)
{
    const uint8_t* table = ttf_find_table(data, font->num_tables, TTF_HEAD_TAG);
    if (!table) return -1;
    font->head.major_version = ttf_read_u16(table);
    font->head.minor_version = ttf_read_u16(table + 2);
    font->head.units_per_em = ttf_read_u16(table + 18);
    font->head.x_min = ttf_read_i16(table + 36);
    font->head.y_min = ttf_read_i16(table + 38);
    font->head.x_max = ttf_read_i16(table + 40);
    font->head.y_max = ttf_read_i16(table + 42);
    font->head.index_to_loc_format = ttf_read_i16(table + 50);
    font->units_per_em = font->head.units_per_em;
    return 0;
}

static int ttf_parse_hhea(ttf_font_t* font, const uint8_t* data)
{
    const uint8_t* table = ttf_find_table(data, font->num_tables, TTF_HHEA_TAG);
    if (!table) return -1;
    font->hhea.ascender = ttf_read_i16(table + 4);
    font->hhea.descender = ttf_read_i16(table + 6);
    font->hhea.line_gap = ttf_read_i16(table + 8);
    font->hhea.advance_width_max = ttf_read_u16(table + 10);
    font->hhea.num_of_long_hor_metrics = ttf_read_u16(table + 34);
    font->ascender = font->hhea.ascender;
    font->descender = font->hhea.descender;
    font->line_gap = font->hhea.line_gap;
    return 0;
}

static int ttf_parse_maxp(ttf_font_t* font, const uint8_t* data)
{
    const uint8_t* table = ttf_find_table(data, font->num_tables, TTF_MAXP_TAG);
    if (!table) return -1;
    font->maxp.version = ttf_read_u16(table);
    font->maxp.num_glyphs = ttf_read_u16(table + 4);
    font->maxp.max_points = ttf_read_u16(table + 6);
    font->maxp.max_contours = ttf_read_u16(table + 8);
    return 0;
}

static int ttf_parse_cmap(ttf_font_t* font, const uint8_t* data)
{
    const uint8_t* table = ttf_find_table(data, font->num_tables, TTF_CMAP_TAG);
    if (!table) return -1;

    uint16_t num_subtables = ttf_read_u16(table + 2);
    const uint8_t* best_subtable = NULL;
    uint16_t best_format = 0;
    uint16_t best_platform = 0;
    uint16_t best_encoding = 0;

    for (uint16_t i = 0; i < num_subtables; i++) {
        const uint8_t* entry = table + 4 + i * 8;
        uint16_t platform = ttf_read_u16(entry);
        uint16_t encoding = ttf_read_u16(entry + 2);
        uint32_t offset = ttf_read_u32(entry + 4);
        const uint8_t* subtable = table + offset;
        uint16_t format = ttf_read_u16(subtable);

        int priority = 0;
        if (platform == 3 && encoding == 10 && format == 12) priority = 4;
        else if (platform == 3 && encoding == 1 && format == 4) priority = 3;
        else if (platform == 0 && format == 4) priority = 2;
        else if (platform == 3 && encoding == 1 && format == 0) priority = 1;

        if (priority > 0 && (best_subtable == NULL || priority >= 3)) {
            best_subtable = subtable;
            best_format = format;
            best_platform = platform;
            best_encoding = encoding;
        }
    }

    if (!best_subtable) return -2;

    font->cmap_format = best_format;
    font->cmap_platform = best_platform;
    font->cmap_encoding = best_encoding;

    if (best_format == 4) {
        uint16_t seg_count = ttf_read_u16(best_subtable + 6) / 2;
        font->cmap_size = 14 + seg_count * 8;
        font->cmap = (uint16_t*)best_subtable;
    } else if (best_format == 12) {
        uint32_t num_groups = ttf_read_u32(best_subtable + 12);
        font->cmap_size = 16 + num_groups * 12;
        font->cmap = (uint16_t*)best_subtable;
    } else {
        font->cmap = (uint16_t*)best_subtable;
        font->cmap_size = 262;
    }

    return 0;
}

static int ttf_parse_loca(ttf_font_t* font, const uint8_t* data)
{
    const uint8_t* table = ttf_find_table(data, font->num_tables, TTF_LOCA_TAG);
    if (!table) return -1;

    font->loca_is_long = font->head.index_to_loc_format;
    uint16_t num_glyphs = font->maxp.num_glyphs;

    if (font->loca_is_long) {
        font->loca_long = (uint32_t*)table;
    } else {
        font->loca_short = (uint16_t*)table;
    }

    (void)num_glyphs;
    return 0;
}

static int ttf_parse_hmtx(ttf_font_t* font, const uint8_t* data)
{
    const uint8_t* table = ttf_find_table(data, font->num_tables, TTF_HMTX_TAG);
    if (!table) return -1;

    uint16_t num_long = font->hhea.num_of_long_hor_metrics;
    font->hmtx_num_long = num_long;
    font->hmtx_advance = (int16_t*)table;
    font->hmtx_lsb = (int16_t*)(table + num_long * 4);

    return 0;
}

int ttf_load(ttf_font_t* font, const uint8_t* data, uint32_t size)
{
    if (!font || !data || size < 12) return -1;

    uint32_t magic = ttf_read_u32(data);
    if (magic != 0x00010000 && magic != TTF_TAG('O','T','T','O')) return -2;

    font->data = (uint8_t*)data;
    font->size = size;
    font->num_tables = ttf_read_u16(data + 4);

    int result;
    result = ttf_parse_head(font, data);
    if (result != 0) return -3;
    result = ttf_parse_hhea(font, data);
    if (result != 0) return -4;
    result = ttf_parse_maxp(font, data);
    if (result != 0) return -5;
    result = ttf_parse_cmap(font, data);
    if (result != 0) return -6;
    result = ttf_parse_loca(font, data);
    if (result != 0) return -7;
    result = ttf_parse_hmtx(font, data);
    if (result != 0) return -8;

    const uint8_t* glyf_table = ttf_find_table(data, font->num_tables, TTF_GLYF_TAG);
    if (glyf_table) {
        font->glyf_data = (uint8_t*)glyf_table;
        const uint8_t* glyf_entry = data + 12;
        for (uint32_t i = 0; i < font->num_tables; i++) {
            if (ttf_read_u32(glyf_entry + i * 16) == TTF_GLYF_TAG) {
                font->glyf_size = ttf_read_u32(glyf_entry + i * 16 + 12);
                break;
            }
        }
    }

    return 0;
}

void ttf_free(ttf_font_t* font)
{
    if (!font) return;
    memset(font, 0, sizeof(ttf_font_t));
}

uint16_t ttf_get_glyph_index(ttf_font_t* font, uint32_t codepoint)
{
    if (!font || !font->cmap) return 0;

    const uint8_t* cmap = (const uint8_t*)font->cmap;

    if (font->cmap_format == 0) {
        if (codepoint < 256) return cmap[6 + codepoint];
        return 0;
    }

    if (font->cmap_format == 4) {
        uint16_t seg_count = ttf_read_u16(cmap + 6) / 2;
        const uint8_t* end_codes = cmap + 14;
        const uint8_t* start_codes = end_codes + seg_count * 2 + 2;
        const uint8_t* id_deltas = start_codes + seg_count * 2;
        const uint8_t* id_range_offsets = id_deltas + seg_count * 2;

        for (uint16_t i = 0; i < seg_count; i++) {
            uint16_t end_code = ttf_read_u16(end_codes + i * 2);
            uint16_t start_code = ttf_read_u16(start_codes + i * 2);

            if (codepoint > end_code) continue;
            if (codepoint < start_code) return 0;

            uint16_t id_delta = ttf_read_u16(id_deltas + i * 2);
            uint16_t id_range_offset = ttf_read_u16(id_range_offsets + i * 2);

            if (id_range_offset == 0) {
                return (uint16_t)(codepoint + id_delta);
            } else {
                uint32_t offset = id_range_offset + 2 * (codepoint - start_code);
                const uint8_t* glyph_entry = id_range_offsets + i * 2 + offset;
                uint16_t glyph_id = ttf_read_u16(glyph_entry);
                if (glyph_id != 0) glyph_id = (uint16_t)(glyph_id + id_delta);
                return glyph_id;
            }
        }
        return 0;
    }

    if (font->cmap_format == 12) {
        uint32_t num_groups = ttf_read_u32(cmap + 12);
        const uint8_t* groups = cmap + 16;

        for (uint32_t i = 0; i < num_groups; i++) {
            const uint8_t* group = groups + i * 12;
            uint32_t start_char = ttf_read_u32(group);
            uint32_t end_char = ttf_read_u32(group + 4);
            uint32_t start_glyph = ttf_read_u32(group + 8);

            if (codepoint >= start_char && codepoint <= end_char) {
                return (uint16_t)(start_glyph + (codepoint - start_char));
            }
        }
        return 0;
    }

    return 0;
}

int16_t ttf_get_advance_width(ttf_font_t* font, uint16_t glyph_index)
{
    if (!font || !font->hmtx_advance) return 0;
    if (glyph_index < font->hmtx_num_long) {
        return ttf_read_i16((const uint8_t*)font->hmtx_advance + glyph_index * 4);
    }
    return ttf_read_i16((const uint8_t*)font->hmtx_advance + (font->hmtx_num_long - 1) * 4);
}

static uint32_t ttf_get_glyph_offset(ttf_font_t* font, uint16_t glyph_index)
{
    if (!font) return 0;
    if (font->loca_is_long) {
        return ttf_read_u32((const uint8_t*)font->loca_long + glyph_index * 4);
    } else {
        return (uint32_t)ttf_read_u16((const uint8_t*)font->loca_short + glyph_index * 2) * 2;
    }
}

int ttf_get_glyph_outline(ttf_font_t* font, uint16_t glyph_index, ttf_glyph_t* glyph)
{
    if (!font || !font->glyf_data || !glyph) return -1;

    uint32_t offset = ttf_get_glyph_offset(font, glyph_index);
    uint32_t next_offset = ttf_get_glyph_offset(font, glyph_index + 1);

    if (offset == next_offset) {
        memset(glyph, 0, sizeof(ttf_glyph_t));
        return 0;
    }

    const uint8_t* glyf = font->glyf_data + offset;
    int16_t num_contours = ttf_read_i16(glyf + 0);

    glyph->x_min = ttf_read_i16(glyf + 2);
    glyph->y_min = ttf_read_i16(glyf + 4);
    glyph->x_max = ttf_read_i16(glyf + 6);
    glyph->y_max = ttf_read_i16(glyf + 8);

    if (num_contours >= 0) {
        glyph->num_contours = (uint16_t)num_contours;
        if (glyph->num_contours == 0) {
            glyph->points = NULL;
            glyph->num_points = 0;
            glyph->end_points = NULL;
            return 0;
        }

        glyph->end_points = (uint16_t*)(glyf + 10);
        glyph->num_points = ttf_read_u16(glyf + 10 + (glyph->num_contours - 1) * 2) + 1;

        const uint8_t* ptr = glyf + 10 + glyph->num_contours * 2;
        uint16_t instruction_length = ttf_read_u16(ptr);
        ptr += 2 + instruction_length;

        glyph->points = (ttf_point_t*)memory_alloc(sizeof(ttf_point_t) * glyph->num_points);
        if (!glyph->points) return -2;

        uint8_t* flags = (uint8_t*)memory_alloc(glyph->num_points);
        if (!flags) { memory_free(glyph->points); return -3; }

        for (uint16_t i = 0; i < glyph->num_points; ) {
            uint8_t flag = *ptr++;
            flags[i] = flag;
            i++;
            if (flag & TTF_REPEAT_FLAG) {
                uint8_t repeat = *ptr++;
                for (uint8_t r = 0; r < repeat && i < glyph->num_points; r++, i++) {
                    flags[i] = flag;
                }
            }
        }

        int16_t x = 0;
        for (uint16_t i = 0; i < glyph->num_points; i++) {
            if (flags[i] & TTF_X_SHORT_VECTOR) {
                int16_t dx = *ptr++;
                x += (flags[i] & TTF_X_IS_SAME) ? dx : -dx;
            } else if (!(flags[i] & TTF_X_IS_SAME)) {
                x += ttf_read_i16(ptr);
                ptr += 2;
            }
            glyph->points[i].x = x;
            glyph->points[i].on_curve = flags[i] & TTF_POINT_ON_CURVE;
        }

        int16_t y = 0;
        for (uint16_t i = 0; i < glyph->num_points; i++) {
            if (flags[i] & TTF_Y_SHORT_VECTOR) {
                int16_t dy = *ptr++;
                y += (flags[i] & TTF_Y_IS_SAME) ? dy : -dy;
            } else if (!(flags[i] & TTF_Y_IS_SAME)) {
                y += ttf_read_i16(ptr);
                ptr += 2;
            }
            glyph->points[i].y = y;
        }

        memory_free(flags);
    } else {
        glyph->num_contours = 0;
        glyph->num_points = 0;
        glyph->points = NULL;
        glyph->end_points = NULL;
    }

    return 0;
}

void ttf_free_glyph_outline(ttf_glyph_t* glyph)
{
    if (!glyph) return;
    if (glyph->points) memory_free(glyph->points);
    glyph->points = NULL;
}

int ttf_render_glyph(ttf_font_t* font, uint16_t glyph_index, int pixel_size,
                      ttf_bitmap_t* bitmap)
{
    if (!font || !bitmap) return -1;

    ttf_glyph_t glyph;
    int result = ttf_get_glyph_outline(font, glyph_index, &glyph);
    if (result != 0) return result;

    float scale = (float)pixel_size / (float)font->units_per_em;
    int width = (int)((glyph.x_max - glyph.x_min) * scale + 0.5f);
    int height = (int)((font->ascender - font->descender) * scale + 0.5f);

    if (width <= 0) width = 1;
    if (height <= 0) height = pixel_size;

    bitmap->width = width;
    bitmap->height = height;
    bitmap->stride = width;
    bitmap->pixels = (uint8_t*)memory_alloc(width * height);
    if (!bitmap->pixels) {
        ttf_free_glyph_outline(&glyph);
        return -2;
    }
    memset(bitmap->pixels, 0, width * height);

    if (glyph.num_contours > 0 && glyph.points) {
        int baseline_y = (int)(font->ascender * scale);

        for (uint16_t c = 0; c < glyph.num_contours; c++) {
            uint16_t start = (c == 0) ? 0 : glyph.end_points[c - 1] + 1;
            uint16_t end = glyph.end_points[c];

            for (uint16_t i = start; i <= end; i++) {
                int px = (int)((glyph.points[i].x - glyph.x_min) * scale);
                int py = baseline_y - (int)(glyph.points[i].y * scale);

                if (px >= 0 && px < width && py >= 0 && py < height) {
                    int dist = 0;
                    if (py > 0 && bitmap->pixels[(py - 1) * width + px] > 0) dist++;
                    if (py < height - 1 && bitmap->pixels[(py + 1) * width + px] > 0) dist++;
                    if (px > 0 && bitmap->pixels[py * width + px - 1] > 0) dist++;
                    if (px < width - 1 && bitmap->pixels[py * width + px + 1] > 0) dist++;

                    if (dist == 0) bitmap->pixels[py * width + px] = 255;
                    else if (dist < 4) bitmap->pixels[py * width + px] = 192;
                }
            }
        }

        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                if (bitmap->pixels[y * width + x] == 0) {
                    int left = 0, right = 0;
                    for (int sx = 0; sx < x; sx++) {
                        if (bitmap->pixels[y * width + sx] > 128) left = 1;
                    }
                    for (int sx = x + 1; sx < width; sx++) {
                        if (bitmap->pixels[y * width + sx] > 128) right = 1;
                    }
                    if (left && right) bitmap->pixels[y * width + x] = 200;
                }
            }
        }
    }

    ttf_free_glyph_outline(&glyph);
    return 0;
}

int ttf_render_codepoint(ttf_font_t* font, uint32_t codepoint, int pixel_size,
                          ttf_bitmap_t* bitmap)
{
    if (!font || !bitmap) return -1;
    uint16_t glyph_index = ttf_get_glyph_index(font, codepoint);
    return ttf_render_glyph(font, glyph_index, pixel_size, bitmap);
}

int ttf_measure_text(ttf_font_t* font, const uint32_t* codepoints, int count,
                      int pixel_size, int* out_width, int* out_height)
{
    if (!font || !codepoints || count <= 0) return -1;

    float scale = (float)pixel_size / (float)font->units_per_em;
    int total_width = 0;

    for (int i = 0; i < count; i++) {
        uint16_t glyph_index = ttf_get_glyph_index(font, codepoints[i]);
        int16_t advance = ttf_get_advance_width(font, glyph_index);
        total_width += (int)(advance * scale);
    }

    if (out_width) *out_width = total_width;
    if (out_height) *out_height = (int)((font->ascender - font->descender + font->line_gap) * scale);

    return 0;
}