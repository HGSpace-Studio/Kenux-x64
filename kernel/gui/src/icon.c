#include "icon.h"
#include "icon_data.h"
#include "kenux_assets.h"
#include "logo_data.h"
#include "framebuffer.h"
#include "graphics.h"
#include "font.h"

static float cos_approx(float x);
static float sin_approx(float x);

#define IX(x, y) (x)
#define IY(y) (y)

static uint32_t col_folder = 0;
static uint32_t col_folder_dark = 0;

static void px(uint32_t x, uint32_t y, uint32_t color) {
    fb_set_pixel(x, y, color);
}

static void rect_abs(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t c) {
    fb_fill_rect(x, y, w, h, c);
}

static void hline_abs(uint32_t x, uint32_t y, uint32_t w, uint32_t c) {
    fb_fill_rect(x, y, w, 1, c);
}

static void vline_abs(uint32_t x, uint32_t y, uint32_t h, uint32_t c) {
    fb_fill_rect(x, y, 1, h, c);
}

static void line_abs(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) {
    gfx_draw_line(x0, y0, x1, y1, c);
}

#define C_WIN_BLUE   RGB(0, 120, 215)
#define C_WIN_BLUE_D RGB(0, 80, 160)
#define C_WIN_GREEN  RGB(0, 153, 0)
#define C_WIN_RED    RGB(232, 17, 35)
#define C_WIN_YELLOW RGB(255, 185, 0)
#define C_WIN_GRAY   RGB(128, 128, 128)
#define C_WIN_LTGRAY RGB(200, 200, 200)
#define C_WIN_DKGRAY RGB(64, 64, 64)
#define C_WIN_WHITE  RGB(255, 255, 255)
#define C_WIN_BLACK  RGB(0, 0, 0)
#define C_WIN_ORANGE RGB(255, 140, 0)
#define C_WIN_PURPLE RGB(120, 0, 160)
#define C_WIN_TEAL   RGB(0, 168, 168)
#define C_WIN_PINK   RGB(255, 192, 203)

static void draw_folder(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t tab_h = s / 5;
    uint32_t tab_w = s * 2 / 5;
    rect_abs(x, y + tab_h, s, s - tab_h, C_WIN_YELLOW);
    rect_abs(x, y, tab_w, tab_h, C_WIN_YELLOW);
    vline_abs(x, y, s, RGB(180, 130, 0));
    vline_abs(x + s - 1, y + tab_h, s - tab_h, RGB(180, 130, 0));
    hline_abs(x, y + tab_h, s, RGB(180, 130, 0));
    hline_abs(x, y + s - 1, s, RGB(180, 130, 0));
    vline_abs(x + tab_w, y, tab_h, RGB(180, 130, 0));
    hline_abs(x, y, tab_w, RGB(180, 130, 0));
}

static void draw_file(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t fold = s / 4;
    rect_abs(x, y, s - fold, s, C_WIN_WHITE);
    vline_abs(x, y, s, C_WIN_GRAY);
    vline_abs(x + s - fold - 1, y, s, C_WIN_GRAY);
    hline_abs(x, y, s - fold, C_WIN_GRAY);
    hline_abs(x, y + s - 1, s - fold, C_WIN_GRAY);
    int32_t fx = (int32_t)x + (int32_t)s - (int32_t)fold - 1;
    line_abs(fx, (int32_t)y, (int32_t)x + (int32_t)s - 1, (int32_t)y + (int32_t)fold, C_WIN_GRAY);
    line_abs((int32_t)x + (int32_t)s - 1, (int32_t)y + (int32_t)fold, (int32_t)x + (int32_t)s - 1, (int32_t)y, C_WIN_GRAY);
    line_abs(fx, (int32_t)y, (int32_t)x + (int32_t)s - 1, (int32_t)y, C_WIN_GRAY);
    for (uint32_t i = 0; i < 3; i++) {
        hline_abs(x + 2, y + 4 + i * 3, s - fold - 4, C_WIN_LTGRAY);
    }
}

static void draw_app(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + 1, y + 1, s - 2, s - 2, C_WIN_WHITE);
    vline_abs(x, y, s, C_WIN_BLUE_D);
    vline_abs(x + s - 1, y, s, C_WIN_BLUE_D);
    hline_abs(x, y, s, C_WIN_BLUE_D);
    hline_abs(x, y + s - 1, s, C_WIN_BLUE_D);
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    uint32_t r = s / 4;
    gfx_draw_filled_circle((int32_t)cx, (int32_t)cy, (int32_t)r, C_WIN_BLUE);
    gfx_draw_filled_circle((int32_t)cx, (int32_t)cy, (int32_t)(r / 2), C_WIN_WHITE);
}

static void draw_settings(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    int32_t r = (int32_t)s / 3;
    gfx_draw_circle((int32_t)cx, (int32_t)cy, r, C_WIN_DKGRAY);
    gfx_draw_filled_circle((int32_t)cx, (int32_t)cy, r / 2, C_WIN_DKGRAY);
    for (int32_t a = 0; a < 360; a += 45) {
        float ang = a * 3.14159265f / 180.0f;
        int32_t tx = (int32_t)(cx + (r + 2) * cos_approx(ang));
        int32_t ty = (int32_t)(cy + (r + 2) * sin_approx(ang));
        gfx_draw_filled_circle(tx, ty, 2, C_WIN_DKGRAY);
    }
    gfx_draw_filled_circle((int32_t)cx, (int32_t)cy, r / 3, C_WIN_WHITE);
}

static void draw_search(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s * 2 / 5;
    uint32_t cy = y + s * 2 / 5;
    int32_t r = (int32_t)s / 3;
    gfx_draw_circle((int32_t)cx, (int32_t)cy, r, C_WIN_DKGRAY);
    int32_t hx = (int32_t)x + (int32_t)s - 2;
    int32_t hy = (int32_t)y + (int32_t)s - 2;
    int32_t sx = (int32_t)cx + r;
    int32_t sy = (int32_t)cy + r;
    for (int32_t i = 0; i < 3; i++) {
        line_abs(sx + i, sy, hx, hy, C_WIN_DKGRAY);
    }
}

static void draw_user(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t hr = s / 4;
    gfx_draw_filled_circle((int32_t)cx, (int32_t)(y + hr + 1), (int32_t)hr, C_WIN_BLUE);
    uint32_t bw = s * 3 / 5;
    uint32_t bx = cx - bw / 2;
    uint32_t by = y + hr * 2 + 2;
    gfx_draw_filled_circle((int32_t)cx, (int32_t)(by + bw / 3), (int32_t)(bw / 2), C_WIN_BLUE);
}

static void draw_save(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x, y, s, s, C_WIN_BLUE);
    rect_abs(x + 1, y + 1, s - 2, s - 2, C_WIN_BLUE_D);
    rect_abs(x + s / 4, y + 1, s / 2, s / 3, C_WIN_WHITE);
    rect_abs(x + 1, y + s * 2 / 3, s - 2, s / 3 - 1, C_WIN_WHITE);
}

static void draw_delete(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    int32_t r = (int32_t)s / 2 - 1;
    gfx_draw_circle((int32_t)cx, (int32_t)cy, r, C_WIN_RED);
    line_abs((int32_t)cx - r / 2, (int32_t)cy - r / 2, (int32_t)cx + r / 2, (int32_t)cy + r / 2, C_WIN_RED);
    line_abs((int32_t)cx - r / 2, (int32_t)cy + r / 2, (int32_t)cx + r / 2, (int32_t)cy - r / 2, C_WIN_RED);
}

static void draw_refresh(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    int32_t r = (int32_t)s / 3;
    int32_t a;
    for (a = 45; a < 360; a += 5) {
        float ang1 = a * 3.14159265f / 180.0f;
        float ang2 = (a + 5) * 3.14159265f / 180.0f;
        line_abs((int32_t)(cx + r * cos_approx(ang1)), (int32_t)(cy + r * sin_approx(ang1)),
                 (int32_t)(cx + r * cos_approx(ang2)), (int32_t)(cy + r * sin_approx(ang2)), C_WIN_GREEN);
    }
    int32_t ax = (int32_t)(cx + r * cos_approx(45 * 3.14159265f / 180.0f));
    int32_t ay = (int32_t)(cy + r * sin_approx(45 * 3.14159265f / 180.0f));
    line_abs(ax - 2, ay, ax + 3, ay + 2, C_WIN_GREEN);
    line_abs(ax, ay - 2, ax + 2, ay + 3, C_WIN_GREEN);
}

static void draw_close(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    int32_t r = (int32_t)s / 3;
    line_abs((int32_t)cx - r, (int32_t)cy - r, (int32_t)cx + r, (int32_t)cy + r, C_WIN_DKGRAY);
    line_abs((int32_t)cx - r, (int32_t)cy + r, (int32_t)cx + r, (int32_t)cy - r, C_WIN_DKGRAY);
    line_abs((int32_t)cx - r + 1, (int32_t)cy - r, (int32_t)cx + r, (int32_t)cy + r - 1, C_WIN_DKGRAY);
    line_abs((int32_t)cx - r, (int32_t)cy - r + 1, (int32_t)cx + r - 1, (int32_t)cy + r, C_WIN_DKGRAY);
}

static void draw_minimize(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cy = y + s / 2;
    hline_abs(x + s / 4, cy, s / 2, C_WIN_DKGRAY);
}

static void draw_maximize(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + s / 4, y + s / 4, s / 2, s / 2, C_WIN_DKGRAY);
    rect_abs(x + s / 4 + 1, y + s / 4 + 1, s / 2 - 2, s / 2 - 2, C_WIN_WHITE);
    vline_abs(x + s / 4, y + s / 4, s / 2, C_WIN_DKGRAY);
    vline_abs(x + s * 3 / 4 - 1, y + s / 4, s / 2, C_WIN_DKGRAY);
    hline_abs(x + s / 4, y + s / 4, s / 2, C_WIN_DKGRAY);
    hline_abs(x + s / 4, y + s * 3 / 4 - 1, s / 2, C_WIN_DKGRAY);
}

static void draw_restore(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + s / 5, y + s / 4, s / 2, s / 2, C_WIN_DKGRAY);
    rect_abs(x + s / 5 + 1, y + s / 4 + 1, s / 2 - 2, s / 2 - 2, C_WIN_WHITE);
    rect_abs(x + s / 3, y + s / 5, s / 3, 3, C_WIN_DKGRAY);
}

static void draw_menu(uint32_t x, uint32_t y, uint32_t s) {
    for (uint32_t i = 0; i < 3; i++) {
        hline_abs(x + 2, y + 2 + i * (s / 3 + 1), s - 4, C_WIN_DKGRAY);
    }
}

static void draw_check(uint32_t x, uint32_t y, uint32_t s) {
    line_abs((int32_t)x + 2, (int32_t)y + s / 2, (int32_t)x + s / 3, (int32_t)y + s * 2 / 3, C_WIN_GREEN);
    line_abs((int32_t)x + s / 3, (int32_t)y + s * 2 / 3, (int32_t)x + s - 2, (int32_t)y + 2, C_WIN_GREEN);
    line_abs((int32_t)x + 2, (int32_t)y + s / 2 + 1, (int32_t)x + s / 3, (int32_t)y + s * 2 / 3 + 1, C_WIN_GREEN);
}

static void draw_warning(uint32_t x, uint32_t y, uint32_t s) {
    int32_t h = (int32_t)s / 2;
    int32_t cx = (int32_t)x + (int32_t)s / 2;
    int32_t by = (int32_t)y + (int32_t)s - 1;
    for (int32_t i = 0; i < h; i++) {
        hline_abs((uint32_t)(cx - i), (uint32_t)(by - i * 2), (uint32_t)(i * 2 + 1), C_WIN_ORANGE);
    }
    hline_abs((uint32_t)cx, (uint32_t)(y + s / 3), 2, C_WIN_DKGRAY);
    hline_abs((uint32_t)cx, (uint32_t)(y + s / 3 + 2), 2, C_WIN_DKGRAY);
}

static void draw_info(uint32_t x, uint32_t y, uint32_t s) {
    gfx_draw_filled_circle((int32_t)(x + s / 2), (int32_t)(y + s / 2), (int32_t)(s / 2 - 1), C_WIN_BLUE);
    gfx_draw_filled_circle((int32_t)(x + s / 2), (int32_t)(y + s / 3), 1, C_WIN_WHITE);
    vline_abs(x + s / 2, y + s / 2, s / 3, C_WIN_WHITE);
}

static void draw_error(uint32_t x, uint32_t y, uint32_t s) {
    gfx_draw_filled_circle((int32_t)(x + s / 2), (int32_t)(y + s / 2), (int32_t)(s / 2 - 1), C_WIN_RED);
    line_abs((int32_t)x + s / 4, (int32_t)y + s / 4, (int32_t)x + s * 3 / 4, (int32_t)y + s * 3 / 4, C_WIN_WHITE);
    line_abs((int32_t)x + s / 4, (int32_t)y + s * 3 / 4, (int32_t)x + s * 3 / 4, (int32_t)y + s / 4, C_WIN_WHITE);
}

static void draw_lock(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t bw = s * 3 / 5;
    uint32_t bx = x + (s - bw) / 2;
    uint32_t by = y + s / 3;
    rect_abs(bx, by, bw, s * 2 / 3, C_WIN_YELLOW);
    rect_abs(bx + 1, by + 1, bw - 2, s * 2 / 3 - 2, C_WIN_ORANGE);
    int32_t r = (int32_t)(bw / 2);
    int32_t cx = (int32_t)(bx + bw / 2);
    int32_t cy = (int32_t)by;
    for (int32_t a = 180; a < 360; a += 2) {
        float ang = a * 3.14159265f / 180.0f;
        px((uint32_t)(cx + r * cos_approx(ang)), (uint32_t)(cy + r * sin_approx(ang) / 2), C_WIN_DKGRAY);
        px((uint32_t)(cx + (r - 1) * cos_approx(ang)), (uint32_t)(cy + (r - 1) * sin_approx(ang) / 2), C_WIN_DKGRAY);
    }
    gfx_draw_filled_circle(cx, (int32_t)(by + s / 3), 2, C_WIN_DKGRAY);
}

static void draw_grid(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cs = (s - 2) / 2;
    rect_abs(x, y, cs, cs, C_WIN_BLUE);
    rect_abs(x + cs + 2, y, cs, cs, C_WIN_GREEN);
    rect_abs(x, y + cs + 2, cs, cs, C_WIN_ORANGE);
    rect_abs(x + cs + 2, y + cs + 2, cs, cs, C_WIN_RED);
}

static void draw_list(uint32_t x, uint32_t y, uint32_t s) {
    for (uint32_t i = 0; i < 3; i++) {
        rect_abs(x, y + i * (s / 3) + 1, 3, 3, C_WIN_BLUE);
        hline_abs(x + 5, y + i * (s / 3) + 2, s - 5, C_WIN_GRAY);
    }
}

static void draw_home(uint32_t x, uint32_t y, uint32_t s) {
    int32_t cx = (int32_t)(x + s / 2);
    int32_t top = (int32_t)y + 1;
    int32_t bot = (int32_t)y + (int32_t)s - 1;
    line_abs(cx, top, (int32_t)x + 1, (int32_t)(y + s / 2), C_WIN_DKGRAY);
    line_abs(cx, top, (int32_t)x + (int32_t)s - 2, (int32_t)(y + s / 2), C_WIN_DKGRAY);
    vline_abs((uint32_t)(x + 2), (uint32_t)(y + s / 2), (uint32_t)(s / 2), C_WIN_DKGRAY);
    vline_abs((uint32_t)(x + s - 3), (uint32_t)(y + s / 2), (uint32_t)(s / 2), C_WIN_DKGRAY);
    hline_abs(x + 2, (uint32_t)bot, s - 4, C_WIN_DKGRAY);
    rect_abs((uint32_t)cx - 2, (uint32_t)(y + s / 2), 4, (uint32_t)(s / 3), C_WIN_DKGRAY);
}

static void draw_arrow_left(uint32_t x, uint32_t y, uint32_t s) {
    int32_t cy = (int32_t)(y + s / 2);
    line_abs((int32_t)x + s / 2, (int32_t)y + 2, (int32_t)x + 2, cy, C_WIN_DKGRAY);
    line_abs((int32_t)x + 2, cy, (int32_t)x + s / 2, (int32_t)(y + s - 2), C_WIN_DKGRAY);
}

static void draw_arrow_right(uint32_t x, uint32_t y, uint32_t s) {
    int32_t cy = (int32_t)(y + s / 2);
    line_abs((int32_t)x + 2, (int32_t)y + 2, (int32_t)x + s / 2, cy, C_WIN_DKGRAY);
    line_abs((int32_t)x + s / 2, cy, (int32_t)x + 2, (int32_t)(y + s - 2), C_WIN_DKGRAY);
}

static void draw_arrow_up(uint32_t x, uint32_t y, uint32_t s) {
    int32_t cx = (int32_t)(x + s / 2);
    line_abs((int32_t)x + 2, (int32_t)(y + s / 2), cx, (int32_t)y + 2, C_WIN_DKGRAY);
    line_abs(cx, (int32_t)y + 2, (int32_t)(x + s - 2), (int32_t)(y + s / 2), C_WIN_DKGRAY);
}

static void draw_arrow_down(uint32_t x, uint32_t y, uint32_t s) {
    int32_t cx = (int32_t)(x + s / 2);
    line_abs((int32_t)x + 2, (int32_t)(y + s / 2), cx, (int32_t)(y + s - 2), C_WIN_DKGRAY);
    line_abs(cx, (int32_t)(y + s - 2), (int32_t)(x + s - 2), (int32_t)(y + s / 2), C_WIN_DKGRAY);
}

static void draw_plus(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    uint32_t l = s / 3;
    vline_abs(cx - l / 2, cy - l / 2, l, C_WIN_DKGRAY);
    hline_abs(cx - l / 2, cy - l / 2, l, C_WIN_DKGRAY);
}

static void draw_minus(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cy = y + s / 2;
    hline_abs(x + s / 4, cy, s / 2, C_WIN_DKGRAY);
}

static void draw_edit(uint32_t x, uint32_t y, uint32_t s) {
    int32_t p1x = (int32_t)(x + s / 4);
    int32_t p1y = (int32_t)(y + s * 3 / 4);
    int32_t p2x = (int32_t)(x + s * 3 / 4);
    int32_t p2y = (int32_t)(y + s / 4);
    line_abs(p1x, p1y, p2x, p2y, C_WIN_DKGRAY);
    line_abs(p1x - 1, p1y, p2x - 1, p2y, C_WIN_DKGRAY);
    line_abs(p1x, p1y, p1x - 3, p1y + 3, C_WIN_DKGRAY);
    line_abs(p2x, p2y, p2x + 3, p2y - 3, C_WIN_DKGRAY);
}

static void draw_copy(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + 1, y + 3, s / 2, s - 4, C_WIN_WHITE);
    vline_abs(x + 1, y + 3, s - 4, C_WIN_GRAY);
    hline_abs(x + 1, y + 3, s / 2, C_WIN_GRAY);
    rect_abs(x + s / 3, y + 1, s / 2, s - 4, C_WIN_WHITE);
    vline_abs(x + s / 3, y + 1, s - 4, C_WIN_GRAY);
    vline_abs(x + s / 3 + s / 2 - 1, y + 1, s - 4, C_WIN_GRAY);
    hline_abs(x + s / 3, y + 1, s / 2, C_WIN_GRAY);
    hline_abs(x + s / 3, y + s - 3, s / 2, C_WIN_GRAY);
}

static void draw_brush(uint32_t x, uint32_t y, uint32_t s) {
    line_abs((int32_t)(x + 2), (int32_t)(y + s - 2), (int32_t)(x + s / 2), (int32_t)(y + s / 2), C_WIN_DKGRAY);
    line_abs((int32_t)(x + 3), (int32_t)(y + s - 2), (int32_t)(x + s / 2 + 1), (int32_t)(y + s / 2), C_WIN_DKGRAY);
    rect_abs(x + s / 2 - 1, y + s / 2 - 2, 4, 4, C_WIN_ORANGE);
    gfx_draw_filled_circle((int32_t)(x + s / 2 + 1), (int32_t)(y + s / 2), 2, C_WIN_ORANGE);
}

static void draw_pencil(uint32_t x, uint32_t y, uint32_t s) {
    int32_t tipx = (int32_t)(x + 2);
    int32_t tipy = (int32_t)(y + s - 2);
    int32_t endx = (int32_t)(x + s - 1);
    int32_t endy = (int32_t)y;
    line_abs(tipx, tipy, endx, endy, C_WIN_YELLOW);
    line_abs(tipx + 1, tipy, endx, endy + 1, C_WIN_YELLOW);
    line_abs(tipx, tipy, endx, endy, C_WIN_DKGRAY);
    rect_abs((uint32_t)(endx - 3), (uint32_t)endy, 4, 4, C_WIN_DKGRAY);
}

static void draw_eraser(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + 2, y + 2, s - 4, s / 2, C_WIN_PINK);
    rect_abs(x + 2, y + 2 + s / 2, s - 4, s / 3, C_WIN_WHITE);
    vline_abs(x + 2, y + 2, s / 2, C_WIN_DKGRAY);
    vline_abs(x + s - 3, y + 2, s / 2, C_WIN_DKGRAY);
    hline_abs(x + 2, y + 2, s - 4, C_WIN_DKGRAY);
    hline_abs(x + 2, y + 2 + s / 2, s - 4, C_WIN_DKGRAY);
}

static void draw_fill(uint32_t x, uint32_t y, uint32_t s) {
    int32_t cx = (int32_t)(x + s / 2);
    int32_t cy = (int32_t)(y + s / 3);
    for (int32_t a = 0; a < 360; a += 2) {
        float ang = a * 3.14159265f / 180.0f;
        line_abs(cx, cy, (int32_t)(cx + s / 3 * cos_approx(ang)), (int32_t)(cy + s / 3 * sin_approx(ang) / 2), C_WIN_BLUE);
    }
    px((uint32_t)(cx + 1), (uint32_t)(y + s - 2), C_WIN_RED);
    px((uint32_t)(cx + 2), (uint32_t)(y + s - 1), C_WIN_RED);
}

static void draw_color(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cs = s / 3;
    rect_abs(x, y, cs, cs, C_WIN_RED);
    rect_abs(x + cs, y, cs, cs, C_WIN_GREEN);
    rect_abs(x + cs * 2, y, cs, cs, C_WIN_BLUE);
    rect_abs(x, y + cs, cs, cs, C_WIN_YELLOW);
    rect_abs(x + cs, y + cs, cs, cs, C_WIN_PURPLE);
    rect_abs(x + cs * 2, y + cs, cs, cs, C_WIN_TEAL);
    rect_abs(x, y + cs * 2, s, cs, C_WIN_DKGRAY);
}

static void draw_text_icon(uint32_t x, uint32_t y, uint32_t s) {
    font_draw_text(x + 2, y + (s - FONT_HEIGHT) / 2, "T", C_WIN_DKGRAY);
    line_abs((int32_t)(x + 2), (int32_t)(y + s - 2), (int32_t)(x + s - 2), (int32_t)(y + s - 2), C_WIN_DKGRAY);
}

static void draw_select(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + 2, y + 2, s - 4, s - 4, 0);
    vline_abs(x + 2, y + 2, s - 4, C_WIN_BLUE);
    vline_abs(x + s - 3, y + 2, s - 4, C_WIN_BLUE);
    hline_abs(x + 2, y + 2, s - 4, C_WIN_BLUE);
    hline_abs(x + 2, y + s - 3, s - 4, C_WIN_BLUE);
    gfx_draw_filled_circle((int32_t)(x + 2), (int32_t)(y + 2), 2, C_WIN_WHITE);
    gfx_draw_filled_circle((int32_t)(x + s - 3), (int32_t)(y + 2), 2, C_WIN_WHITE);
    gfx_draw_filled_circle((int32_t)(x + 2), (int32_t)(y + s - 3), 2, C_WIN_WHITE);
    gfx_draw_filled_circle((int32_t)(x + s - 3), (int32_t)(y + s - 3), 2, C_WIN_WHITE);
}

static void draw_computer(uint32_t x, uint32_t y, uint32_t s) {
    rect_abs(x + 1, y + 1, s - 2, s * 2 / 3, C_WIN_LTGRAY);
    vline_abs(x, y, s * 2 / 3, C_WIN_DKGRAY);
    vline_abs(x + s - 1, y, s * 2 / 3, C_WIN_DKGRAY);
    hline_abs(x, y, s, C_WIN_DKGRAY);
    rect_abs(x + 2, y + 2, s - 4, s * 2 / 3 - 4, C_WIN_BLUE);
    rect_abs(x + s / 4, y + s * 2 / 3 + 1, s / 2, 2, C_WIN_DKGRAY);
    rect_abs(x + s / 3, y + s * 2 / 3 + 3, s / 3, s / 6, C_WIN_DKGRAY);
}

static void draw_trash(uint32_t x, uint32_t y, uint32_t s) {
    hline_abs(x, y, s, C_WIN_DKGRAY);
    rect_abs(x + 1, y + 1, s - 2, s * 3 / 4, C_WIN_LTGRAY);
    hline_abs(x + 1, y + s * 3 / 4 + 1, s - 2, C_WIN_DKGRAY);
    vline_abs(x + s / 3, y + 2, s / 2, C_WIN_WHITE);
    vline_abs(x + s * 2 / 3, y + 2, s / 2, C_WIN_WHITE);
}

static void draw_clock(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    int32_t r = (int32_t)s / 2 - 1;
    gfx_draw_circle((int32_t)cx, (int32_t)cy, r, C_WIN_DKGRAY);
    gfx_draw_filled_circle((int32_t)cx, (int32_t)cy, r - 1, C_WIN_WHITE);
    vline_abs(cx, cy, s / 3, C_WIN_DKGRAY);
    hline_abs(cx, cy, s / 4, C_WIN_DKGRAY);
}

static void draw_network(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    gfx_draw_filled_circle((int32_t)cx, (int32_t)(y + s / 2), 3, C_WIN_GREEN);
    gfx_draw_circle((int32_t)cx, (int32_t)(y + s / 2), s / 3, C_WIN_GREEN);
    gfx_draw_circle((int32_t)cx, (int32_t)(y + s / 2), s / 2 - 1, C_WIN_GREEN);
}

static void draw_power(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t cx = x + s / 2;
    uint32_t cy = y + s / 2;
    int32_t r = (int32_t)s / 2 - 1;
    for (int32_t a = 45; a < 360; a += 2) {
        float ang = a * 3.14159265f / 180.0f;
        px((uint32_t)((int32_t)cx + (int32_t)(r * cos_approx(ang))), 
           (uint32_t)((int32_t)cy + (int32_t)(r * sin_approx(ang))), C_WIN_RED);
    }
    vline_abs(cx, y + 2, s / 3, C_WIN_RED);
}

static float cos_approx(float x) {
    x = x - (int32_t)(x / (2 * 3.14159265f)) * 2 * 3.14159265f;
    if (x < 0) x = -x;
    if (x > 3.14159265f) x = 2 * 3.14159265f - x;
    return 1.0f - x * x * (1.0f / 2.0f - x * x / 24.0f);
}

static float sin_approx(float x) {
    return cos_approx(x - 3.14159265f / 2.0f);
}

static icon_id_t icon_bitmap_alias(icon_id_t icon) {
    /* Prefer real curvo-CN bitmaps for legacy/base Kenux icon IDs.
     * These IDs existed before the full icon pack and otherwise fall back to
     * slow vector/code rendering or draw nothing.
     */
    switch (icon) {
        case ICON_MINIMIZE:     return ICON_UNDERLINE;
        case ICON_ERROR:        return ICON_代码错误;
        case ICON_LIST:         return ICON_菜单2;
        case ICON_HOME:         return ICON_房子;
        case ICON_BACK:
        case ICON_LEFT:         return ICON_上一个;
        case ICON_FORWARD:
        case ICON_RIGHT:        return ICON_下一个;
        case ICON_UP:           return ICON_上传;
        case ICON_DOWN:         return ICON_下载;
        case ICON_PLUS:         return ICON_网格添加;
        case ICON_MINUS:        return ICON_移除用户;
        case ICON_EDIT:         return ICON_铅笔;
        case ICON_COPY:         return ICON_复制;
        case ICON_CUT:          return ICON_分割;
        case ICON_PASTE:        return ICON_导入;
        case ICON_UNDO:         return ICON_撤销;
        case ICON_REDO:         return ICON_重做;
        case ICON_BOLD:         return ICON_粗体;
        case ICON_ITALIC:       return ICON_斜体;
        case ICON_ALIGN_CENTER: return ICON_水平居中;
        case ICON_BRUSH:        return ICON_画刷;
        case ICON_PENCIL:       return ICON_铅笔;
        case ICON_ERASER:       return ICON_橡皮;
        case ICON_FILL:         return ICON_填充;
        case ICON_COLOR:        return ICON_调色盘;
        case ICON_PEN:          return ICON_钢笔;
        case ICON_TEXT:         return ICON_文本;
        case ICON_SELECT:       return ICON_选择;
        case ICON_COMPUTER:     return ICON_显示器;
        default:                return icon;
    }
}

void icon_draw(uint32_t x, uint32_t y, icon_id_t icon, uint32_t size) {
    if (icon == ICON_APP || icon == ICON_K) {
        fb_blit_scaled(x, y, size, size, kenux_logo_data, 256, 256);
        return;
    }
    if (icon == ICON_FOLDER) {
        fb_blit_scaled(x, y, size, size, kenux_asset_folder, 32, 32);
        return;
    }
    if (icon == ICON_FILE) {
        fb_blit_scaled(x, y, size, size, kenux_asset_file, 32, 32);
        return;
    }
    /* Try bitmap icon pack first (curvo-CN) */
    const icon_bitmap_t* bmp = icon_get_bitmap((int)icon);
    if (!bmp || !bmp->data) {
        icon_id_t alias = icon_bitmap_alias(icon);
        if (alias != icon) {
            bmp = icon_get_bitmap((int)alias);
        }
    }
    if (bmp && bmp->data) {
        if (size == bmp->width && size == bmp->height) {
            fb_blit_alpha(x, y, size, size, bmp->data);
        } else {
            fb_blit_scaled(x, y, size, size, bmp->data, bmp->width, bmp->height);
        }
        return;
    }
    /* Fall back to vector drawing */
    switch (icon) {
        case ICON_FOLDER:      draw_folder(x, y, size); break;
        case ICON_FILE:        draw_file(x, y, size); break;
        case ICON_APP:         draw_app(x, y, size); break;
        case ICON_SETTINGS:    draw_settings(x, y, size); break;
        case ICON_SEARCH:      draw_search(x, y, size); break;
        case ICON_USER:        draw_user(x, y, size); break;
        case ICON_SAVE:        draw_save(x, y, size); break;
        case ICON_DELETE:      draw_delete(x, y, size); break;
        case ICON_REFRESH:     draw_refresh(x, y, size); break;
        case ICON_CLOSE:       draw_close(x, y, size); break;
        case ICON_MINIMIZE:    draw_minimize(x, y, size); break;
        case ICON_MAXIMIZE:    draw_maximize(x, y, size); break;
        case ICON_RESTORE:     draw_restore(x, y, size); break;
        case ICON_MENU:        draw_menu(x, y, size); break;
        case ICON_CHECK:       draw_check(x, y, size); break;
        case ICON_WARNING:     draw_warning(x, y, size); break;
        case ICON_INFO:        draw_info(x, y, size); break;
        case ICON_ERROR:       draw_error(x, y, size); break;
        case ICON_LOCK:        draw_lock(x, y, size); break;
        case ICON_GRID:        draw_grid(x, y, size); break;
        case ICON_LIST:        draw_list(x, y, size); break;
        case ICON_HOME:        draw_home(x, y, size); break;
        case ICON_BACK:        draw_arrow_left(x, y, size); break;
        case ICON_FORWARD:     draw_arrow_right(x, y, size); break;
        case ICON_UP:          draw_arrow_up(x, y, size); break;
        case ICON_DOWN:        draw_arrow_down(x, y, size); break;
        case ICON_LEFT:        draw_arrow_left(x, y, size); break;
        case ICON_RIGHT:       draw_arrow_right(x, y, size); break;
        case ICON_PLUS:        draw_plus(x, y, size); break;
        case ICON_MINUS:       draw_minus(x, y, size); break;
        case ICON_EDIT:        draw_edit(x, y, size); break;
        case ICON_COPY:        draw_copy(x, y, size); break;
        case ICON_BRUSH:       draw_brush(x, y, size); break;
        case ICON_PENCIL:      draw_pencil(x, y, size); break;
        case ICON_ERASER:      draw_eraser(x, y, size); break;
        case ICON_FILL:        draw_fill(x, y, size); break;
        case ICON_COLOR:       draw_color(x, y, size); break;
        case ICON_TEXT:        draw_text_icon(x, y, size); break;
        case ICON_SELECT:      draw_select(x, y, size); break;
        case ICON_ZOOM_IN:     draw_search(x, y, size); break;
        case ICON_ZOOM_OUT:    draw_search(x, y, size); break;
        case ICON_COMPUTER:    draw_computer(x, y, size); break;
        case ICON_TRASH:      draw_trash(x, y, size); break;
        case ICON_CLOCK:       draw_clock(x, y, size); break;
        case ICON_NETWORK:    draw_network(x, y, size); break;
        case ICON_POWER:      draw_power(x, y, size); break;
        default: break;
    }
}
