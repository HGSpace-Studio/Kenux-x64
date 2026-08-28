#include "kanvas_window.h"
#include "kapi.h"
#include <string.h>

static uint32_t next_win_id = 1;

static uint32_t kui_col32_inline(kui_color_t c)
{
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

kanvas_window_t* kanvas_window_create(const char* title, int x, int y, int w, int h,
                                       bool resizable, bool decorated)
{
    kanvas_window_t* win = (kanvas_window_t*)kapi_malloc(sizeof(kanvas_window_t));
    if (!win) return NULL;
    memset(win, 0, sizeof(kanvas_window_t));
    win->id = next_win_id++;
    if (title) {
        size_t len = strlen(title);
        if (len >= 256) len = 255;
        memcpy(win->title, title, len);
        win->title[len] = '\0';
    }
    win->x = x; win->y = y;
    win->width = w; win->height = h;
    win->min_width = KANVAS_WIN_MIN_W;
    win->min_height = KANVAS_WIN_MIN_H;
    win->max_width = 4096;
    win->max_height = 4096;
    win->visible = true;
    win->focused = false;
    win->minimized = false;
    win->maximized = false;
    win->resizable = resizable;
    win->decorated = decorated;
    win->modal = false;
    win->topmost = false;
    win->closing = false;
    win->prev_x = x; win->prev_y = y;
    win->prev_w = w; win->prev_h = h;
    win->titlebar_bg = (kui_color_t){45, 45, 45, 255};
    win->titlebar_fg = (kui_color_t){240, 240, 240, 255};
    win->client_bg = (kui_color_t){30, 30, 30, 255};
    win->border_color = (kui_color_t){60, 60, 60, 255};
    win->hover_ctrl = KANVAS_WIN_CTRL_NONE;
    win->resize_edge = KANVAS_RESIZE_NONE;
    win->components_head = NULL;
    win->component_count = 0;
    win->on_paint = NULL;
    win->on_event = NULL;
    win->user_data = NULL;
    win->back_buffer = NULL;
    win->front_buffer = NULL;
    win->buffer_stride = w * 4;
    win->dirty_count = 0;
    win->next = NULL;
    win->prev = NULL;
    win->parent = NULL;
    size_t buf_sz = (size_t)w * h * 4;
    win->back_buffer = (uint32_t*)kapi_malloc(buf_sz);
    win->front_buffer = (uint32_t*)kapi_malloc(buf_sz);
    if (!win->back_buffer || !win->front_buffer) {
        if (win->back_buffer) kapi_free(win->back_buffer);
        if (win->front_buffer) kapi_free(win->front_buffer);
        kapi_free(win);
        return NULL;
    }
    memset(win->back_buffer, 0, buf_sz);
    memset(win->front_buffer, 0, buf_sz);
    return win;
}

void kanvas_window_destroy(kanvas_window_t* win)
{
    if (!win) return;
    if (win->back_buffer) kapi_free(win->back_buffer);
    if (win->front_buffer) kapi_free(win->front_buffer);
    kapi_free(win);
}

static void fill_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fw) w = fw - x;
    if (y + h > fh) h = fh - y;
    if (w <= 0 || h <= 0) return;
    for (int row = y; row < y + h; row++) {
        uint32_t* p = (uint32_t*)((uint8_t*)fb + row * stride);
        for (int col = x; col < x + w; col++) p[col] = color;
    }
}

static void draw_rect_outline(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t color)
{
    fill_rect(fb, stride, fw, fh, x, y, w, 1, color);
    fill_rect(fb, stride, fw, fh, x, y + h - 1, w, 1, color);
    fill_rect(fb, stride, fw, fh, x, y, 1, h, color);
    fill_rect(fb, stride, fw, fh, x + w - 1, y, 1, h, color);
}

static void fill_rounded_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, int r, uint32_t color)
{
    if (r <= 0) { fill_rect(fb, stride, fw, fh, x, y, w, h, color); return; }
    fill_rect(fb, stride, fw, fh, x + r, y, w - 2 * r, h, color);
    fill_rect(fb, stride, fw, fh, x, y + r, r, h - 2 * r, color);
    fill_rect(fb, stride, fw, fh, x + w - r, y + r, r, h - 2 * r, color);
    for (int dy = 0; dy < r; dy++) {
        int dx = (int)__builtin_sqrtf((float)(r * r - dy * dy));
        int cx1 = x + r - dx, cx2 = x + w - r + dx;
        int ry1 = y + r - dy - 1, ry2 = y - r + h + dy;
        fill_rect(fb, stride, fw, fh, cx1, ry1, cx2 - cx1, 1, color);
        fill_rect(fb, stride, fw, fh, cx1, ry2, cx2 - cx1, 1, color);
    }
}

static void blend_pixel(uint32_t* fb, int stride, int fw, int fh, int x, int y, uint32_t color, uint8_t alpha)
{
    if (x < 0 || x >= fw || y < 0 || y >= fh) return;
    uint32_t* p = (uint32_t*)((uint8_t*)fb + y * stride);
    uint32_t dst = p[x];
    uint8_t ia = 255 - alpha;
    uint8_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint8_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    p[x] = 0xFF000000u | (((sr * alpha + dr * ia) / 255) << 16) | (((sg * alpha + dg * ia) / 255) << 8) | ((sb * alpha + db * ia) / 255);
}

void kanvas_window_paint_shadow(kanvas_window_t* win, uint32_t* fb, int stride, int fw, int fh)
{
    if (!win || !fb) return;
    int blur = KANVAS_WIN_SHADOW_BLUR;
    uint8_t alpha = KANVAS_WIN_SHADOW_ALPHA;
    uint32_t sc = 0x000000;
    for (int i = blur; i >= 1; i--) {
        uint8_t a = (uint8_t)((float)alpha * (1.0f - (float)i / blur));
        int sx = win->x - i, sy = win->y - i;
        int sw = win->width + 2 * i, sh = win->height + 2 * i;
        for (int y = sy; y < sy + sh && y < fh; y++) {
            if (y < 0) continue;
            if (y >= win->y && y < win->y + (int)win->height) {
                if (sx >= 0 && sx < fw) blend_pixel(fb, stride, fw, fh, sx, y, sc, a);
                if (sx + sw - 1 >= 0 && sx + sw - 1 < fw) blend_pixel(fb, stride, fw, fh, sx + sw - 1, y, sc, a);
            } else {
                for (int x = sx; x < sx + sw && x < fw; x++) {
                    if (x >= 0) blend_pixel(fb, stride, fw, fh, x, y, sc, a);
                }
            }
        }
    }
}

void kanvas_window_paint_titlebar(kanvas_window_t* win, uint32_t* fb, int stride, int fw, int fh)
{
    if (!win || !win->decorated || !fb) return;
    uint32_t tb_col = kui_col32_inline(win->titlebar_bg);
    uint32_t fg_col = kui_col32_inline(win->titlebar_fg);
    int tb_h = KANVAS_WIN_TITLEBAR_H;
    int r = KANVAS_WIN_RADIUS;
    fill_rounded_rect(fb, stride, fw, fh, win->x, win->y, win->width, tb_h + r, r, tb_col);
    fill_rect(fb, stride, fw, fh, win->x, win->y + r, win->width, tb_h - r, tb_col);
    int text_x = win->x + 12;
    int text_y = win->y + (tb_h - 14) / 2;
    kui_draw_text(fb, stride, fw, fh, text_x, text_y, win->title, fg_col, 14, 0);
    int ctrl_x = win->x + win->width - 12;
    int ctrl_y = win->y + (tb_h - KANVAS_WIN_CTRL_SIZE) / 2;
    uint32_t close_col = win->hover_ctrl == KANVAS_WIN_CTRL_CLOSE ? 0xE04040FF : 0x808080FF;
    fill_rounded_rect(fb, stride, fw, fh, ctrl_x - 2, ctrl_y - 2, KANVAS_WIN_CTRL_SIZE + 4, KANVAS_WIN_CTRL_SIZE + 4, 4, close_col);
    ctrl_x -= KANVAS_WIN_CTRL_SIZE + KANVAS_WIN_CTRL_SPACING;
    uint32_t max_col = win->hover_ctrl == KANVAS_WIN_CTRL_MAXIMIZE ? 0x606060FF : 0x808080FF;
    fill_rounded_rect(fb, stride, fw, fh, ctrl_x, ctrl_y, KANVAS_WIN_CTRL_SIZE, KANVAS_WIN_CTRL_SIZE, 4, max_col);
    ctrl_x -= KANVAS_WIN_CTRL_SIZE + KANVAS_WIN_CTRL_SPACING;
    uint32_t min_col = win->hover_ctrl == KANVAS_WIN_CTRL_MINIMIZE ? 0x606060FF : 0x808080FF;
    fill_rounded_rect(fb, stride, fw, fh, ctrl_x, ctrl_y, KANVAS_WIN_CTRL_SIZE, KANVAS_WIN_CTRL_SIZE, 4, min_col);
}

void kanvas_window_paint_border(kanvas_window_t* win, uint32_t* fb, int stride, int fw, int fh)
{
    if (!win || !fb) return;
    uint32_t bc = kui_col32_inline(win->border_color);
    draw_rect_outline(fb, stride, fw, fh, win->x, win->y, win->width, win->height, bc);
}

void kanvas_window_paint(kanvas_window_t* win, uint32_t* fb, int stride, int screen_w, int screen_h)
{
    if (!win || !win->visible || !fb) return;
    if (win->minimized) return;
    kanvas_window_paint_shadow(win, fb, stride, screen_w, screen_h);
    uint32_t bg = kui_col32_inline(win->client_bg);
    int r = KANVAS_WIN_RADIUS;
    fill_rounded_rect(fb, stride, screen_w, screen_h, win->x, win->y, win->width, win->height, r, bg);
    if (win->decorated) {
        kanvas_window_paint_titlebar(win, fb, stride, screen_w, screen_h);
        int content_y = win->y + KANVAS_WIN_TITLEBAR_H;
        int content_h = win->height - KANVAS_WIN_TITLEBAR_H;
        if (content_h > 0)
            fill_rect(fb, stride, screen_w, screen_h, win->x, content_y, win->width, content_h, bg);
    }
    kanvas_window_paint_border(win, fb, stride, screen_w, screen_h);
    if (win->on_paint) win->on_paint(win, fb, stride, screen_w, screen_h);
}

bool kanvas_window_hit_test(kanvas_window_t* win, int px, int py)
{
    if (!win || !win->visible || win->minimized) return false;
    return px >= win->x && px < win->x + win->width && py >= win->y && py < win->y + win->height;
}

bool kanvas_window_titlebar_hit(kanvas_window_t* win, int px, int py)
{
    if (!win || !win->decorated) return false;
    return px >= win->x && px < win->x + win->width && py >= win->y && py < win->y + KANVAS_WIN_TITLEBAR_H;
}

kanvas_win_ctrl_t kanvas_window_hit_ctrl(kanvas_window_t* win, int px, int py)
{
    if (!kanvas_window_titlebar_hit(win, px, py)) return KANVAS_WIN_CTRL_NONE;
    int ctrl_x = win->x + win->width - 12;
    int ctrl_y = win->y + (KANVAS_WIN_TITLEBAR_H - KANVAS_WIN_CTRL_SIZE) / 2;
    if (px >= ctrl_x - 1 && px <= ctrl_x + KANVAS_WIN_CTRL_SIZE + 1 && py >= ctrl_y - 1 && py <= ctrl_y + KANVAS_WIN_CTRL_SIZE + 1)
        return KANVAS_WIN_CTRL_CLOSE;
    ctrl_x -= KANVAS_WIN_CTRL_SIZE + KANVAS_WIN_CTRL_SPACING;
    if (px >= ctrl_x && px <= ctrl_x + KANVAS_WIN_CTRL_SIZE && py >= ctrl_y && py <= ctrl_y + KANVAS_WIN_CTRL_SIZE)
        return KANVAS_WIN_CTRL_MAXIMIZE;
    ctrl_x -= KANVAS_WIN_CTRL_SIZE + KANVAS_WIN_CTRL_SPACING;
    if (px >= ctrl_x && px <= ctrl_x + KANVAS_WIN_CTRL_SIZE && py >= ctrl_y && py <= ctrl_y + KANVAS_WIN_CTRL_SIZE)
        return KANVAS_WIN_CTRL_MINIMIZE;
    return KANVAS_WIN_CTRL_NONE;
}

kanvas_resize_edge_t kanvas_window_hit_resize(kanvas_window_t* win, int px, int py)
{
    if (!win || !win->resizable || !win->visible) return KANVAS_RESIZE_NONE;
    int m = KANVAS_WIN_RESIZE_MARGIN;
    int x = win->x, y = win->y, w = win->width, h = win->height;
    bool left = px >= x - m && px <= x + m;
    bool right = px >= x + w - m && px <= x + w + m;
    bool top = py >= y - m && py <= y + m;
    bool bottom = py >= y + h - m && py <= y + h + m;
    if (top && left) return KANVAS_RESIZE_NW;
    if (top && right) return KANVAS_RESIZE_NE;
    if (bottom && left) return KANVAS_RESIZE_SW;
    if (bottom && right) return KANVAS_RESIZE_SE;
    if (top) return KANVAS_RESIZE_N;
    if (bottom) return KANVAS_RESIZE_S;
    if (left) return KANVAS_RESIZE_W;
    if (right) return KANVAS_RESIZE_E;
    return KANVAS_RESIZE_NONE;
}

void kanvas_window_minimize(kanvas_window_t* win)
{
    if (!win) return;
    win->minimized = true;
    win->visible = true;
}

void kanvas_window_maximize(kanvas_window_t* win)
{
    if (!win) return;
    if (win->maximized) {
        kanvas_window_restore(win);
        return;
    }
    win->prev_x = win->x; win->prev_y = win->y;
    win->prev_w = win->width; win->prev_h = win->height;
    win->maximized = true;
}

void kanvas_window_restore(kanvas_window_t* win)
{
    if (!win) return;
    if (win->maximized) {
        win->x = win->prev_x; win->y = win->prev_y;
        win->width = win->prev_w; win->height = win->prev_h;
        win->maximized = false;
    }
    if (win->minimized) win->minimized = false;
}

void kanvas_window_close(kanvas_window_t* win)
{
    if (!win) return;
    win->closing = true;
    win->visible = false;
}

void kanvas_window_set_title(kanvas_window_t* win, const char* title)
{
    if (!win || !title) return;
    size_t len = strlen(title);
    if (len >= 256) len = 255;
    memcpy(win->title, title, len);
    win->title[len] = '\0';
}

void kanvas_window_set_rect(kanvas_window_t* win, int x, int y, int w, int h)
{
    if (!win) return;
    if (w < win->min_width) w = win->min_width;
    if (h < win->min_height) h = win->min_height;
    win->x = x; win->y = y; win->width = w; win->height = h;
    win->buffer_stride = w * 4;
}

void kanvas_window_get_content_rect(kanvas_window_t* win, int* x, int* y, int* w, int* h)
{
    if (!win) return;
    if (x) *x = win->x;
    if (y) *y = win->y + (win->decorated ? KANVAS_WIN_TITLEBAR_H : 0);
    if (w) *w = win->width;
    if (h) *h = win->height - (win->decorated ? KANVAS_WIN_TITLEBAR_H : 0);
}

void kanvas_window_add_component(kanvas_window_t* win, kui_component_t* comp)
{
    if (!win || !comp) return;
    comp->next = win->components_head;
    if (win->components_head) win->components_head->prev = comp;
    win->components_head = comp;
    comp->parent_window = NULL;
    win->component_count++;
}

void kanvas_window_remove_component(kanvas_window_t* win, kui_component_t* comp)
{
    if (!win || !comp) return;
    if (comp->prev) comp->prev->next = comp->next;
    else win->components_head = comp->next;
    if (comp->next) comp->next->prev = comp->prev;
    win->component_count--;
}

void kanvas_window_invalidate(kanvas_window_t* win)
{
    if (!win) return;
    win->dirty_count = 0;
    if (win->dirty_count < 16) {
        win->dirty_rects[win->dirty_count++] = (kui_rect_t){win->x, win->y, win->width, win->height};
    }
}

void kanvas_window_invalidate_rect(kanvas_window_t* win, int x, int y, int w, int h)
{
    if (!win || win->dirty_count >= 16) return;
    win->dirty_rects[win->dirty_count++] = (kui_rect_t){x, y, w, h};
}