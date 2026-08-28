#include "kapi_render_ext.h"
#include "kapi.h"
#include <string.h>

static struct {
    float global_dpi;
    uint32_t aa_mode;
    uint32_t composite_mode;
    struct {
        int window_id;
        float dpi;
    } window_dpi[64];
    int window_dpi_count;
} render_state;

int kapi_render_set_dpi(float dpi)
{
    if (dpi < 0.5f) dpi = 0.5f;
    render_state.global_dpi = dpi;
    return 0;
}

float kapi_render_get_dpi(void) { return render_state.global_dpi; }

int kapi_render_set_dpi_for_window(kapi_window_t* window, float dpi)
{
    if (!window) return -1;
    if (dpi < 0.5f) dpi = 0.5f;
    for (int i = 0; i < render_state.window_dpi_count; i++) {
        if (render_state.window_dpi[i].window_id == (int)(intptr_t)window) {
            render_state.window_dpi[i].dpi = dpi;
            return 0;
        }
    }
    if (render_state.window_dpi_count >= 64) return -1;
    int idx = render_state.window_dpi_count++;
    render_state.window_dpi[idx].window_id = (int)(intptr_t)window;
    render_state.window_dpi[idx].dpi = dpi;
    return 0;
}

float kapi_render_get_dpi_for_window(kapi_window_t* window)
{
    if (!window) return render_state.global_dpi;
    for (int i = 0; i < render_state.window_dpi_count; i++) {
        if (render_state.window_dpi[i].window_id == (int)(intptr_t)window)
            return render_state.window_dpi[i].dpi;
    }
    return render_state.global_dpi;
}

int kapi_render_get_dpi_info(kapi_dpi_info_t* info)
{
    if (!info) return -1;
    info->dpi = render_state.global_dpi;
    info->scale_x = render_state.global_dpi / 96.0f;
    info->scale_y = render_state.global_dpi / 96.0f;
    return 0;
}

int32_t kapi_render_scale_value(int32_t value, float dpi)
{
    return (int32_t)(value * dpi / 96.0f);
}

int32_t kapi_render_unscale_value(int32_t value, float dpi)
{
    if (dpi < 0.01f) return value;
    return (int32_t)(value * 96.0f / dpi);
}

kapi_rect_t kapi_render_scale_rect(kapi_rect_t rect, float dpi)
{
    float s = dpi / 96.0f;
    kapi_rect_t r;
    r.x = (int32_t)(rect.x * s);
    r.y = (int32_t)(rect.y * s);
    r.width = (int32_t)(rect.width * s);
    r.height = (int32_t)(rect.height * s);
    return r;
}

kapi_rect_t kapi_render_unscale_rect(kapi_rect_t rect, float dpi)
{
    if (dpi < 0.01f) return rect;
    float s = 96.0f / dpi;
    kapi_rect_t r;
    r.x = (int32_t)(rect.x * s);
    r.y = (int32_t)(rect.y * s);
    r.width = (int32_t)(rect.width * s);
    r.height = (int32_t)(rect.height * s);
    return r;
}

int kapi_render_set_aa_mode(kapi_surface_t* surface, uint32_t mode)
{
    (void)surface;
    if (mode > KAPI_AA_SUBPIXEL) return -1;
    render_state.aa_mode = mode;
    return 0;
}

uint32_t kapi_render_get_aa_mode(kapi_surface_t* surface)
{
    (void)surface;
    return render_state.aa_mode;
}

int kapi_render_set_composite_mode(kapi_surface_t* surface, uint32_t mode)
{
    (void)surface;
    if (mode > KAPI_COMPOSITE_ADD) return -1;
    render_state.composite_mode = mode;
    return 0;
}

uint32_t kapi_render_get_composite_mode(kapi_surface_t* surface)
{
    (void)surface;
    return render_state.composite_mode;
}

static inline void aa_pixel(uint8_t* data, int stride, int w, int h, int x, int y, uint32_t color, float alpha)
{
    if (x < 0 || x >= w || y < 0 || y >= h || !data) return;
    if (alpha <= 0.0f) return;
    if (alpha >= 1.0f) {
        ((uint32_t*)(data + y * stride))[x] = color;
        return;
    }
    uint32_t* p = (uint32_t*)(data + y * stride);
    uint32_t dst = p[x];
    uint8_t ia = (uint8_t)(255.0f * (1.0f - alpha));
    uint8_t a = (uint8_t)(255.0f * alpha);
    uint8_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint8_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    p[x] = 0xFF000000u | (((sr * a + dr * ia) / 255) << 16) | (((sg * a + dg * ia) / 255) << 8) | ((sb * a + db * ia) / 255);
}

static inline float aa_dist(float x, float y) { return kapi_sqrtf(x * x + y * y); }

int kapi_render_draw_line_aa(kapi_surface_t* surface, const kapi_aa_line_t* line)
{
    if (!surface || !line || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    float dx = line->x1 - line->x0, dy = line->y1 - line->y0;
    float len = kapi_sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return 0;
    float nx = -dy / len, ny = dx / len;
    float half_w = line->width * 0.5f;
    int min_x = (int)(line->x0 < line->x1 ? line->x0 : line->x1) - (int)half_w - 1;
    int max_x = (int)(line->x0 > line->x1 ? line->x0 : line->x1) + (int)half_w + 1;
    int min_y = (int)(line->y0 < line->y1 ? line->y0 : line->y1) - (int)half_w - 1;
    int max_y = (int)(line->y0 > line->y1 ? line->y0 : line->y1) + (int)half_w + 1;
    if (min_x < 0) min_x = 0; if (max_x >= w) max_x = w - 1;
    if (min_y < 0) min_y = 0; if (max_y >= h) max_y = h - 1;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
            float px = fx - line->x0, py = fy - line->y0;
            float proj = (px * dx + py * dy) / (len * len);
            if (proj < 0.0f || proj > 1.0f) continue;
            float dist = px * nx + py * ny;
            if (dist < 0.0f) dist = -dist;
            if (dist > half_w + 1.0f) continue;
            float alpha = 1.0f;
            if (dist > half_w - 0.5f) alpha = half_w + 0.5f - dist;
            if (alpha > 0.0f) aa_pixel(surface->data, surface->stride, w, h, x, y, line->color, alpha);
        }
    }
    return 0;
}

int kapi_render_draw_circle_aa(kapi_surface_t* surface, const kapi_aa_circle_t* circle)
{
    if (!surface || !circle || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int r = (int)circle->r + 2;
    int min_x = (int)circle->cx - r; if (min_x < 0) min_x = 0;
    int max_x = (int)circle->cx + r; if (max_x >= w) max_x = w - 1;
    int min_y = (int)circle->cy - r; if (min_y < 0) min_y = 0;
    int max_y = (int)circle->cy + r; if (max_y >= h) max_y = h - 1;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
            float dist = aa_dist(fx - circle->cx, fy - circle->cy);
            if (circle->filled) {
                if (dist <= circle->r - 0.5f) {
                    aa_pixel(surface->data, surface->stride, w, h, x, y, circle->color, 1.0f);
                } else if (dist <= circle->r + 0.5f) {
                    float alpha = circle->r + 0.5f - dist;
                    aa_pixel(surface->data, surface->stride, w, h, x, y, circle->color, alpha);
                }
            } else {
                float ring = dist > circle->r ? dist - circle->r : circle->r - dist;
                if (ring < 1.0f) {
                    float alpha = 1.0f - ring;
                    aa_pixel(surface->data, surface->stride, w, h, x, y, circle->color, alpha);
                }
            }
        }
    }
    return 0;
}

int kapi_render_draw_ellipse_aa(kapi_surface_t* surface, const kapi_aa_ellipse_t* ellipse)
{
    if (!surface || !ellipse || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int rx = (int)ellipse->rx + 2, ry = (int)ellipse->ry + 2;
    int min_x = (int)ellipse->cx - rx; if (min_x < 0) min_x = 0;
    int max_x = (int)ellipse->cx + rx; if (max_x >= w) max_x = w - 1;
    int min_y = (int)ellipse->cy - ry; if (min_y < 0) min_y = 0;
    int max_y = (int)ellipse->cy + ry; if (max_y >= h) max_y = h - 1;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
            float dx = (fx - ellipse->cx) / ellipse->rx;
            float dy = (fy - ellipse->cy) / ellipse->ry;
            float d = dx * dx + dy * dy;
            if (ellipse->filled) {
                if (d <= 1.0f) {
                    float edge = 1.0f - d;
                    float alpha = edge > 1.0f ? 1.0f : edge;
                    aa_pixel(surface->data, surface->stride, w, h, x, y, ellipse->color, alpha);
                }
            } else {
                float ring = d > 1.0f ? d - 1.0f : 1.0f - d;
                if (ring < 0.1f) {
                    float alpha = 1.0f - ring * 10.0f;
                    aa_pixel(surface->data, surface->stride, w, h, x, y, ellipse->color, alpha);
                }
            }
        }
    }
    return 0;
}

int kapi_render_draw_round_rect_aa(kapi_surface_t* surface, const kapi_aa_round_rect_t* rr)
{
    if (!surface || !rr || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    for (int y = 0; y < (int)rr->h; y++) {
        for (int x = 0; x < (int)rr->w; x++) {
            float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
            float dist = 0.0f;
            float r = rr->r;
            if (fx < r && fy < r) dist = aa_dist(fx - r, fy - r) - r;
            else if (fx > rr->w - r && fy < r) dist = aa_dist(fx - (rr->w - r), fy - r) - r;
            else if (fx < r && fy > rr->h - r) dist = aa_dist(fx - r, fy - (rr->h - r)) - r;
            else if (fx > rr->w - r && fy > rr->h - r) dist = aa_dist(fx - (rr->w - r), fy - (rr->h - r)) - r;
            else dist = -1.0f;
            float alpha = 0.0f;
            if (rr->filled) {
                if (dist <= -0.5f) alpha = 1.0f;
                else if (dist <= 0.5f) alpha = 0.5f - dist;
            } else {
                float ring = dist < 0.0f ? -dist : dist;
                if (ring < 1.0f) alpha = 1.0f - ring;
            }
            if (alpha > 0.0f) {
                int px = (int)(rr->x + x), py = (int)(rr->y + y);
                aa_pixel(surface->data, surface->stride, w, h, px, py, rr->color, alpha);
            }
        }
    }
    return 0;
}

int kapi_render_draw_arc_aa(kapi_surface_t* surface, const kapi_aa_arc_t* arc)
{
    if (!surface || !arc || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int r = (int)arc->r + 2;
    int min_x = (int)arc->cx - r; if (min_x < 0) min_x = 0;
    int max_x = (int)arc->cx + r; if (max_x >= w) max_x = w - 1;
    int min_y = (int)arc->cy - r; if (min_y < 0) min_y = 0;
    int max_y = (int)arc->cy + r; if (max_y >= h) max_y = h - 1;
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
            float dist = aa_dist(fx - arc->cx, fy - arc->cy);
            float ring = dist > arc->r ? dist - arc->r : arc->r - dist;
            if (ring < 1.0f) {
                float angle = kapi_atan2f(fy - arc->cy, fx - arc->cx);
                if (angle >= arc->start_angle && angle <= arc->end_angle) {
                    float alpha = 1.0f - ring;
                    aa_pixel(surface->data, surface->stride, w, h, x, y, arc->color, alpha);
                }
            }
        }
    }
    return 0;
}

int kapi_render_draw_bezier_aa(kapi_surface_t* surface, const kapi_aa_bezier_t* bezier)
{
    if (!surface || !bezier || !surface->data) return -1;
    int steps = 64;
    float prev_x = bezier->x0, prev_y = bezier->y0;
    for (int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        float u = 1.0f - t;
        float x = u*u*u*bezier->x0 + 3*u*u*t*bezier->cx0 + 3*u*t*t*bezier->cx1 + t*t*t*bezier->x1;
        float y = u*u*u*bezier->y0 + 3*u*u*t*bezier->cy0 + 3*u*t*t*bezier->cy1 + t*t*t*bezier->y1;
        kapi_aa_line_t seg;
        seg.x0 = prev_x; seg.y0 = prev_y; seg.x1 = x; seg.y1 = y;
        seg.width = bezier->width; seg.color = bezier->color;
        kapi_render_draw_line_aa(surface, &seg);
        prev_x = x; prev_y = y;
    }
    return 0;
}

int kapi_render_draw_polygon_aa(kapi_surface_t* surface, const kapi_point_t* points,
                                uint32_t count, uint32_t color, bool filled)
{
    if (!surface || !points || count < 3) return -1;
    if (filled) {
        int w = (int)surface->width, h = (int)surface->height;
        int min_y = points[0].y, max_y = points[0].y;
        for (uint32_t i = 1; i < count; i++) {
            if (points[i].y < min_y) min_y = points[i].y;
            if (points[i].y > max_y) max_y = points[i].y;
        }
        if (min_y < 0) min_y = 0;
        if (max_y >= h) max_y = h - 1;
        int intersections[64];
        for (int y = min_y; y <= max_y; y++) {
            int ic = 0;
            for (uint32_t i = 0; i < count && ic < 64; i++) {
                uint32_t j = (i + 1) % count;
                int y0 = points[i].y, y1 = points[j].y;
                if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                    float t = (float)(y - y0) / (float)(y1 - y0);
                    intersections[ic++] = (int)(points[i].x + t * (points[j].x - points[i].x));
                }
            }
            for (int k = 0; k < ic - 1; k += 2) {
                int x0 = intersections[k], x1 = intersections[k + 1];
                if (x0 > x1) { int tmp = x0; x0 = x1; x1 = tmp; }
                for (int x = x0; x <= x1; x++) {
                    if (x >= 0 && x < w) aa_pixel(surface->data, surface->stride, w, h, x, y, color, 1.0f);
                }
            }
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            uint32_t j = (i + 1) % count;
            kapi_aa_line_t seg;
            seg.x0 = (float)points[i].x; seg.y0 = (float)points[i].y;
            seg.x1 = (float)points[j].x; seg.y1 = (float)points[j].y;
            seg.width = 1.0f; seg.color = color;
            kapi_render_draw_line_aa(surface, &seg);
        }
    }
    return 0;
}

int kapi_render_draw_shadow(kapi_surface_t* surface, int32_t x, int32_t y,
                            int32_t w, int32_t h, const kapi_shadow_params_t* params)
{
    if (!surface || !params || !surface->data) return -1;
    int sw = (int)surface->width, sh = (int)surface->height;
    int blur = params->blur_radius;
    int sx = x + params->offset_x - blur;
    int sy = y + params->offset_y - blur;
    int ex = x + w + params->offset_x + blur;
    int ey = y + h + params->offset_y + blur;
    if (sx < 0) sx = 0; if (ex >= sw) ex = sw - 1;
    if (sy < 0) sy = 0; if (ey >= sh) ey = sh - 1;
    for (int py = sy; py <= ey; py++) {
        for (int px = sx; px <= ex; px++) {
            int dx = 0, dy = 0;
            int cx = px - params->offset_x, cy = py - params->offset_y;
            if (cx < x) dx = x - cx; else if (cx > x + w) dx = cx - x - w;
            if (cy < y) dy = y - cy; else if (cy > y + h) dy = cy - y - h;
            float dist = aa_dist((float)dx, (float)dy);
            float alpha = 0.0f;
            if (blur > 0) {
                if (dist < (float)blur) alpha = 1.0f - dist / (float)blur;
            } else {
                if (dist < 1.0f) alpha = 1.0f;
            }
            alpha *= (float)params->alpha / 255.0f;
            if (alpha > 0.001f) aa_pixel(surface->data, surface->stride, sw, sh, px, py, params->color, alpha);
        }
    }
    return 0;
}

int kapi_render_draw_thick_line(kapi_surface_t* surface, int32_t x1, int32_t y1,
                                int32_t x2, int32_t y2, uint32_t color, uint32_t thickness)
{
    kapi_aa_line_t line;
    line.x0 = (float)x1; line.y0 = (float)y1;
    line.x1 = (float)x2; line.y1 = (float)y2;
    line.width = (float)thickness; line.color = color;
    return kapi_render_draw_line_aa(surface, &line);
}

int kapi_render_draw_dashed_line(kapi_surface_t* surface, int32_t x1, int32_t y1,
                                 int32_t x2, int32_t y2, uint32_t color,
                                 uint32_t dash_len, uint32_t gap_len)
{
    if (!surface || !surface->data) return -1;
    int dx = x2 - x1, dy = y2 - y1;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int len = adx > ady ? adx : ady;
    if (len == 0) return 0;
    uint32_t cycle = dash_len + gap_len;
    if (cycle == 0) cycle = 1;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int err = adx - ady, cx = x1, cy = y1;
    int step = 0;
    for (int i = 0; i <= len; i++) {
        uint32_t pos = (uint32_t)step % cycle;
        if (pos < dash_len) {
            if (cx >= 0 && cx < (int)surface->width && cy >= 0 && cy < (int)surface->height) {
                ((uint32_t*)(surface->data + cy * surface->stride))[cx] = color;
            }
        }
        int e2 = 2 * err;
        if (e2 > -ady) { err -= ady; cx += sx; }
        if (e2 < adx) { err += adx; cy += sy; }
        step++;
    }
    return 0;
}

int kapi_render_draw_dashed_rect(kapi_surface_t* surface, int32_t x, int32_t y,
                                 int32_t w, int32_t h, uint32_t color,
                                 uint32_t dash_len, uint32_t gap_len)
{
    kapi_render_draw_dashed_line(surface, x, y, x + w - 1, y, color, dash_len, gap_len);
    kapi_render_draw_dashed_line(surface, x + w - 1, y, x + w - 1, y + h - 1, color, dash_len, gap_len);
    kapi_render_draw_dashed_line(surface, x + w - 1, y + h - 1, x, y + h - 1, color, dash_len, gap_len);
    kapi_render_draw_dashed_line(surface, x, y + h - 1, x, y, color, dash_len, gap_len);
    return 0;
}

int kapi_render_read_buffer(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                            uint32_t* buffer, size_t buffer_size)
{
    if (!surface || !region || !buffer || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int rx = region->x, ry = region->y, rw = region->width, rh = region->height;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > w) rw = w - rx;
    if (ry + rh > h) rh = h - ry;
    if (rw <= 0 || rh <= 0) return -1;
    size_t needed = (size_t)rw * rh * 4;
    if (buffer_size < needed) return -1;
    for (int y = 0; y < rh; y++) {
        const uint32_t* src = (const uint32_t*)(surface->data + (ry + y) * surface->stride);
        memcpy(buffer + (size_t)y * rw, src + rx, (size_t)rw * 4);
    }
    return 0;
}

int kapi_render_write_buffer(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                             const uint32_t* buffer, size_t buffer_size)
{
    if (!surface || !region || !buffer || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int rx = region->x, ry = region->y, rw = region->width, rh = region->height;
    if (rx < 0) rx = 0; if (ry < 0) ry = 0;
    if (rx + rw > w) rw = w - rx;
    if (ry + rh > h) rh = h - ry;
    if (rw <= 0 || rh <= 0) return -1;
    size_t needed = (size_t)rw * rh * 4;
    if (buffer_size < needed) return -1;
    for (int y = 0; y < rh; y++) {
        uint32_t* dst = (uint32_t*)(surface->data + (ry + y) * surface->stride);
        memcpy(dst + rx, buffer + (size_t)y * rw, (size_t)rw * 4);
    }
    return 0;
}

int kapi_render_read_buffer_rgba(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                                 uint8_t* buffer, size_t buffer_size)
{
    if (!surface || !region || !buffer || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int rx = region->x, ry = region->y, rw = region->width, rh = region->height;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > w) rw = w - rx;
    if (ry + rh > h) rh = h - ry;
    if (rw <= 0 || rh <= 0) return -1;
    size_t needed = (size_t)rw * rh * 4;
    if (buffer_size < needed) return -1;
    for (int y = 0; y < rh; y++) {
        const uint32_t* src = (const uint32_t*)(surface->data + (ry + y) * surface->stride);
        for (int x = 0; x < rw; x++) {
            uint32_t c = src[rx + x];
            size_t off = ((size_t)y * rw + x) * 4;
            buffer[off] = (c >> 16) & 0xFF;
            buffer[off + 1] = (c >> 8) & 0xFF;
            buffer[off + 2] = c & 0xFF;
            buffer[off + 3] = (c >> 24) & 0xFF;
        }
    }
    return 0;
}

int kapi_render_write_buffer_rgba(kapi_surface_t* surface, const kapi_buffer_region_t* region,
                                  const uint8_t* buffer, size_t buffer_size)
{
    if (!surface || !region || !buffer || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int rx = region->x, ry = region->y, rw = region->width, rh = region->height;
    if (rx < 0) rx = 0; if (ry < 0) ry = 0;
    if (rx + rw > w) rw = w - rx;
    if (ry + rh > h) rh = h - ry;
    if (rw <= 0 || rh <= 0) return -1;
    size_t needed = (size_t)rw * rh * 4;
    if (buffer_size < needed) return -1;
    for (int y = 0; y < rh; y++) {
        uint32_t* dst = (uint32_t*)(surface->data + (ry + y) * surface->stride);
        for (int x = 0; x < rw; x++) {
            size_t off = ((size_t)y * rw + x) * 4;
            dst[rx + x] = ((uint32_t)buffer[off + 3] << 24) | ((uint32_t)buffer[off] << 16) |
                          ((uint32_t)buffer[off + 1] << 8) | buffer[off + 2];
        }
    }
    return 0;
}

int kapi_render_scroll_region(kapi_surface_t* surface, int32_t dx, int32_t dy,
                              const kapi_buffer_region_t* clip)
{
    if (!surface || !surface->data) return -1;
    int w = (int)surface->width, h = (int)surface->height;
    int cx = clip ? clip->x : 0, cy = clip ? clip->y : 0;
    int cw = clip ? clip->width : w, ch = clip ? clip->height : h;
    if (dx == 0 && dy == 0) return 0;
    if (dy > 0) {
        for (int y = cy + ch - 1; y >= cy; y--) {
            int src_y = y - dy;
            if (src_y < cy || src_y >= cy + ch) {
                uint32_t* row = (uint32_t*)(surface->data + y * surface->stride);
                for (int x = cx; x < cx + cw; x++) row[x] = 0;
                continue;
            }
            uint32_t* dst_row = (uint32_t*)(surface->data + y * surface->stride);
            uint32_t* src_row = (uint32_t*)(surface->data + src_y * surface->stride);
            if (dx == 0) {
                memmove(dst_row + cx, src_row + cx, (size_t)cw * 4);
            } else if (dx > 0) {
                for (int x = cx + cw - 1; x >= cx; x--) {
                    int sx = x - dx;
                    dst_row[x] = (sx >= cx && sx < cx + cw) ? src_row[sx] : 0;
                }
            } else {
                for (int x = cx; x < cx + cw; x++) {
                    int sx = x - dx;
                    dst_row[x] = (sx >= cx && sx < cx + cw) ? src_row[sx] : 0;
                }
            }
        }
    } else {
        for (int y = cy; y < cy + ch; y++) {
            int src_y = y - dy;
            if (src_y < cy || src_y >= cy + ch) {
                uint32_t* row = (uint32_t*)(surface->data + y * surface->stride);
                for (int x = cx; x < cx + cw; x++) row[x] = 0;
                continue;
            }
            uint32_t* dst_row = (uint32_t*)(surface->data + y * surface->stride);
            uint32_t* src_row = (uint32_t*)(surface->data + src_y * surface->stride);
            if (dx == 0) {
                memmove(dst_row + cx, src_row + cx, (size_t)cw * 4);
            } else if (dx > 0) {
                for (int x = cx + cw - 1; x >= cx; x--) {
                    int sx = x - dx;
                    dst_row[x] = (sx >= cx && sx < cx + cw) ? src_row[sx] : 0;
                }
            } else {
                for (int x = cx; x < cx + cw; x++) {
                    int sx = x - dx;
                    dst_row[x] = (sx >= cx && sx < cx + cw) ? src_row[sx] : 0;
                }
            }
        }
    }
    return 0;
}

float kapi_ease_linear(float t) { return t; }
float kapi_ease_in_quad(float t) { return t * t; }
float kapi_ease_out_quad(float t) { return t * (2.0f - t); }
float kapi_ease_in_out_quad(float t) { return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t; }
float kapi_ease_in_cubic(float t) { return t * t * t; }
float kapi_ease_out_cubic(float t) { float u = t - 1.0f; return u * u * u + 1.0f; }
float kapi_ease_in_out_cubic(float t) { return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f; }
float kapi_ease_in_quart(float t) { return t * t * t * t; }
float kapi_ease_out_quart(float t) { float u = t - 1.0f; return 1.0f - u * u * u * u; }
float kapi_ease_in_expo(float t) { return t == 0.0f ? 0.0f : kapi_powf(2.0f, 10.0f * (t - 1.0f)); }
float kapi_ease_out_expo(float t) { return t == 1.0f ? 1.0f : 1.0f - kapi_powf(2.0f, -10.0f * t); }
float kapi_ease_in_out_expo(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f ? kapi_powf(2.0f, 20.0f * t - 10.0f) / 2.0f : (2.0f - kapi_powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}
float kapi_ease_in_back(float t) { float s = 1.70158f; return t * t * ((s + 1.0f) * t - s); }
float kapi_ease_out_back(float t) { float s = 1.70158f; float u = t - 1.0f; return u * u * ((s + 1.0f) * u + s) + 1.0f; }
float kapi_ease_in_out_back(float t) {
    float s = 1.70158f * 1.525f;
    if (t < 0.5f) { float u = 2.0f * t; return (u * u * ((s + 1.0f) * u - s)) / 2.0f; }
    float u = 2.0f * t - 2.0f; return (u * u * ((s + 1.0f) * u + s) + 2.0f) / 2.0f;
}
float kapi_ease_in_elastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return -kapi_powf(2.0f, 10.0f * t - 10.0f) * kapi_sinf((10.0f * t - 10.75f) * 2.0943951f);
}
float kapi_ease_out_elastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return kapi_powf(2.0f, -10.0f * t) * kapi_sinf((10.0f * t - 0.75f) * 2.0943951f) + 1.0f;
}
float kapi_ease_in_bounce(float t) { return 1.0f - kapi_ease_out_bounce(1.0f - t); }
float kapi_ease_out_bounce(float t) {
    float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.0f / d1) return n1 * t * t;
    if (t < 2.0f / d1) { float u = t - 1.5f / d1; return n1 * u * u + 0.75f; }
    if (t < 2.5f / d1) { float u = t - 2.25f / d1; return n1 * u * u + 0.9375f; }
    float u = t - 2.625f / d1; return n1 * u * u + 0.984375f;
}
float kapi_ease_spring(float t) {
    float w = 0.4f, d = 0.6f;
    float k = kapi_sqrtf(1.0f - w * w);
    float phase = kapi_atan2f(k, w);
    return 1.0f - kapi_powf(d, t * 10.0f) * kapi_cosf(k * t * 10.0f + phase) / kapi_cosf(phase);
}

float kapi_ease_eval(uint32_t easing, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    switch (easing) {
    case KAPI_EASE_LINEAR: return kapi_ease_linear(t);
    case KAPI_EASE_IN_QUAD: return kapi_ease_in_quad(t);
    case KAPI_EASE_OUT_QUAD: return kapi_ease_out_quad(t);
    case KAPI_EASE_IN_OUT_QUAD: return kapi_ease_in_out_quad(t);
    case KAPI_EASE_IN_CUBIC: return kapi_ease_in_cubic(t);
    case KAPI_EASE_OUT_CUBIC: return kapi_ease_out_cubic(t);
    case KAPI_EASE_IN_OUT_CUBIC: return kapi_ease_in_out_cubic(t);
    case KAPI_EASE_IN_QUART: return kapi_ease_in_quart(t);
    case KAPI_EASE_OUT_QUART: return kapi_ease_out_quart(t);
    case KAPI_EASE_IN_EXPO: return kapi_ease_in_expo(t);
    case KAPI_EASE_OUT_EXPO: return kapi_ease_out_expo(t);
    case KAPI_EASE_IN_OUT_EXPO: return kapi_ease_in_out_expo(t);
    case KAPI_EASE_IN_BACK: return kapi_ease_in_back(t);
    case KAPI_EASE_OUT_BACK: return kapi_ease_out_back(t);
    case KAPI_EASE_IN_OUT_BACK: return kapi_ease_in_out_back(t);
    case KAPI_EASE_IN_ELASTIC: return kapi_ease_in_elastic(t);
    case KAPI_EASE_OUT_ELASTIC: return kapi_ease_out_elastic(t);
    case KAPI_EASE_IN_BOUNCE: return kapi_ease_in_bounce(t);
    case KAPI_EASE_OUT_BOUNCE: return kapi_ease_out_bounce(t);
    case KAPI_EASE_SPRING: return kapi_ease_spring(t);
    default: return t;
    }
}

const char* kapi_ease_name(uint32_t easing)
{
    switch (easing) {
    case KAPI_EASE_LINEAR: return "linear";
    case KAPI_EASE_IN_QUAD: return "in_quad";
    case KAPI_EASE_OUT_QUAD: return "out_quad";
    case KAPI_EASE_IN_OUT_QUAD: return "in_out_quad";
    case KAPI_EASE_IN_CUBIC: return "in_cubic";
    case KAPI_EASE_OUT_CUBIC: return "out_cubic";
    case KAPI_EASE_IN_OUT_CUBIC: return "in_out_cubic";
    case KAPI_EASE_IN_QUART: return "in_quart";
    case KAPI_EASE_OUT_QUART: return "out_quart";
    case KAPI_EASE_IN_EXPO: return "in_expo";
    case KAPI_EASE_OUT_EXPO: return "out_expo";
    case KAPI_EASE_IN_OUT_EXPO: return "in_out_expo";
    case KAPI_EASE_IN_BACK: return "in_back";
    case KAPI_EASE_OUT_BACK: return "out_back";
    case KAPI_EASE_IN_OUT_BACK: return "in_out_back";
    case KAPI_EASE_IN_ELASTIC: return "in_elastic";
    case KAPI_EASE_OUT_ELASTIC: return "out_elastic";
    case KAPI_EASE_IN_BOUNCE: return "in_bounce";
    case KAPI_EASE_OUT_BOUNCE: return "out_bounce";
    case KAPI_EASE_SPRING: return "spring";
    default: return "unknown";
    }
}