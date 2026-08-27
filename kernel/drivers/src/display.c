#include "display.h"
#include <string.h>

void fbdev_init(fbdev_t* fb, void* addr, uint32_t w, uint32_t h, uint32_t bpp, uint32_t pitch)
{
    if (!fb) return;
    memset(fb, 0, sizeof(fbdev_t));
    spin_init(&fb->lock);
    fb->framebuffer = addr;
    fb->width = w;
    fb->height = h;
    fb->bpp = bpp;
    fb->pitch = pitch;
    fb->size = pitch * h;

    if (bpp == 32) {
        fb->red_mask = 0x00FF0000; fb->green_mask = 0x0000FF00; fb->blue_mask = 0x000000FF;
        fb->red_shift = 16; fb->green_shift = 8; fb->blue_shift = 0;
    } else if (bpp == 24) {
        fb->red_mask = 0xFF0000; fb->green_mask = 0x00FF00; fb->blue_mask = 0x0000FF;
        fb->red_shift = 16; fb->green_shift = 8; fb->blue_shift = 0;
    } else if (bpp == 16) {
        fb->red_mask = 0xF800; fb->green_mask = 0x07E0; fb->blue_mask = 0x001F;
        fb->red_shift = 11; fb->green_shift = 5; fb->blue_shift = 0;
    }
}

void fbdev_put_pixel(fbdev_t* fb, uint32_t x, uint32_t y, uint32_t color)
{
    if (!fb || !fb->framebuffer || x >= fb->width || y >= fb->height) return;
    spinlock_acquire(&fb->lock);
    uint8_t* addr = (uint8_t*)fb->framebuffer + y * fb->pitch + x * (fb->bpp / 8);
    if (fb->bpp == 32) {
        *((uint32_t*)addr) = color;
    } else if (fb->bpp == 24) {
        addr[0] = (uint8_t)(color & 0xFF);
        addr[1] = (uint8_t)((color >> 8) & 0xFF);
        addr[2] = (uint8_t)((color >> 16) & 0xFF);
    } else if (fb->bpp == 16) {
        *((uint16_t*)addr) = (uint16_t)color;
    }
    spinlock_release(&fb->lock);
}

uint32_t fbdev_get_pixel(fbdev_t* fb, uint32_t x, uint32_t y)
{
    if (!fb || !fb->framebuffer || x >= fb->width || y >= fb->height) return 0;
    uint8_t* addr = (uint8_t*)fb->framebuffer + y * fb->pitch + x * (fb->bpp / 8);
    if (fb->bpp == 32) return *((uint32_t*)addr);
    if (fb->bpp == 16) return *((uint16_t*)addr);
    return 0;
}

void fbdev_fill_rect(fbdev_t* fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (!fb || !fb->framebuffer) return;
    spinlock_acquire(&fb->lock);
    for (uint32_t row = y; row < y + h && row < fb->height; row++) {
        uint8_t* line = (uint8_t*)fb->framebuffer + row * fb->pitch;
        for (uint32_t col = x; col < x + w && col < fb->width; col++) {
            uint8_t* addr = line + col * (fb->bpp / 8);
            if (fb->bpp == 32) *((uint32_t*)addr) = color;
            else if (fb->bpp == 16) *((uint16_t*)addr) = (uint16_t)color;
        }
    }
    spinlock_release(&fb->lock);
}

void fbdev_blit(fbdev_t* fb, uint32_t x, uint32_t y, const void* data, uint32_t w, uint32_t h, uint32_t src_pitch)
{
    if (!fb || !fb->framebuffer || !data) return;
    spinlock_acquire(&fb->lock);
    const uint8_t* src = (const uint8_t*)data;
    for (uint32_t row = 0; row < h && y + row < fb->height; row++) {
        uint8_t* dst = (uint8_t*)fb->framebuffer + (y + row) * fb->pitch + x * (fb->bpp / 8);
        const uint8_t* src_row = src + row * src_pitch;
        uint32_t copy_bytes = w * (fb->bpp / 8);
        if (x + w > fb->width) copy_bytes = (fb->width - x) * (fb->bpp / 8);
        memcpy(dst, src_row, copy_bytes);
    }
    spinlock_release(&fb->lock);
}

void fbdev_scroll(fbdev_t* fb, int lines, uint32_t bg_color)
{
    if (!fb || !fb->framebuffer || lines <= 0) return;
    spinlock_acquire(&fb->lock);
    uint32_t scroll_bytes = (uint32_t)lines * fb->pitch;
    uint8_t* base = (uint8_t*)fb->framebuffer;
    memmove(base, base + scroll_bytes, fb->size - scroll_bytes);
    for (uint32_t i = 0; i < scroll_bytes; i += (fb->bpp / 8)) {
        if (fb->bpp == 32) *((uint32_t*)(base + fb->size - scroll_bytes + i)) = bg_color;
        else if (fb->bpp == 16) *((uint16_t*)(base + fb->size - scroll_bytes + i)) = (uint16_t)bg_color;
    }
    spinlock_release(&fb->lock);
}

uint32_t fbdev_rgb(fbdev_t* fb, uint8_t r, uint8_t g, uint8_t b)
{
    if (!fb) return 0;
    if (fb->bpp == 32) return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if (fb->bpp == 16) return ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3);
    return 0;
}

void vt100_init(vt100_t* vt, int rows, int cols, fbdev_t* fb)
{
    if (!vt) return;
    memset(vt, 0, sizeof(vt100_t));
    spin_init(&vt->lock);
    vt->rows = rows > VT100_MAX_ROWS ? VT100_MAX_ROWS : rows;
    vt->cols = cols > VT100_MAX_COLS ? VT100_MAX_COLS : cols;
    vt->fg_color = 7;
    vt->bg_color = 0;
    vt->scroll_top = 0;
    vt->scroll_bottom = vt->rows - 1;
    vt->fb = fb;
    vt->font_width = 8;
    vt->font_height = 16;
    vt100_clear(vt);
}

void vt100_clear(vt100_t* vt)
{
    if (!vt) return;
    for (int r = 0; r < vt->rows; r++) {
        for (int c = 0; c < vt->cols; c++) {
            vt->screen[r][c].ch = ' ';
            vt->screen[r][c].fg = vt->fg_color;
            vt->screen[r][c].bg = vt->bg_color;
            vt->screen[r][c].attr = 0;
        }
    }
    vt->cursor_row = 0;
    vt->cursor_col = 0;
}

void vt100_scroll_up(vt100_t* vt, int lines)
{
    if (!vt || lines <= 0) return;
    for (int l = 0; l < lines; l++) {
        for (int r = vt->scroll_top; r < vt->scroll_bottom; r++) {
            memcpy(vt->screen[r], vt->screen[r + 1], sizeof(vt100_cell_t) * vt->cols);
        }
        for (int c = 0; c < vt->cols; c++) {
            vt->screen[vt->scroll_bottom][c].ch = ' ';
            vt->screen[vt->scroll_bottom][c].fg = vt->fg_color;
            vt->screen[vt->scroll_bottom][c].bg = vt->bg_color;
        }
    }
}

void vt100_set_cursor(vt100_t* vt, int row, int col)
{
    if (!vt) return;
    if (row >= 0 && row < vt->rows) vt->cursor_row = row;
    if (col >= 0 && col < vt->cols) vt->cursor_col = col;
}

static void vt100_put_char(vt100_t* vt, char ch)
{
    if (ch == '\n') {
        vt->cursor_col = 0;
        vt->cursor_row++;
        if (vt->cursor_row > vt->scroll_bottom) {
            vt100_scroll_up(vt, 1);
            vt->cursor_row = vt->scroll_bottom;
        }
    } else if (ch == '\r') {
        vt->cursor_col = 0;
    } else if (ch == '\b') {
        if (vt->cursor_col > 0) vt->cursor_col--;
    } else if (ch == '\t') {
        vt->cursor_col = (vt->cursor_col + 8) & ~7;
        if (vt->cursor_col >= vt->cols) vt->cursor_col = vt->cols - 1;
    } else {
        if (vt->cursor_col >= vt->cols) {
            vt->cursor_col = 0;
            vt->cursor_row++;
            if (vt->cursor_row > vt->scroll_bottom) {
                vt100_scroll_up(vt, 1);
                vt->cursor_row = vt->scroll_bottom;
            }
        }
        vt->screen[vt->cursor_row][vt->cursor_col].ch = (uint8_t)ch;
        vt->screen[vt->cursor_row][vt->cursor_col].fg = vt->fg_color;
        vt->screen[vt->cursor_row][vt->cursor_col].bg = vt->bg_color;
        vt->cursor_col++;
    }
}

static void vt100_process_csi(vt100_t* vt)
{
    int* p = vt->esc_params;
    int n = vt->esc_param_count;
    if (n == 0) { p[0] = 0; n = 1; }

    char cmd = (char)(vt->esc_state == 2 ? 0 : 0);
    (void)cmd;

    switch (vt->esc_state) {
    default: break;
    }
}

void vt100_write(vt100_t* vt, const char* data, int len)
{
    if (!vt || !data) return;
    spinlock_acquire(&vt->lock);
    for (int i = 0; i < len; i++) {
        char ch = data[i];
        if (vt->esc_state == 0) {
            if (ch == 0x1B) { vt->esc_state = 1; vt->esc_param_count = 0; }
            else vt100_put_char(vt, ch);
        } else if (vt->esc_state == 1) {
            if (ch == '[') { vt->esc_state = 2; vt->esc_param_count = 0; }
            else { vt->esc_state = 0; vt100_put_char(vt, ch); }
        } else if (vt->esc_state == 2) {
            if (ch >= '0' && ch <= '9') {
                if (vt->esc_param_count < 16) {
                    vt->esc_params[vt->esc_param_count] *= 10;
                    vt->esc_params[vt->esc_param_count] += ch - '0';
                }
            } else if (ch == ';') {
                if (vt->esc_param_count < 15) vt->esc_param_count++;
                vt->esc_params[vt->esc_param_count] = 0;
            } else if (ch == 'A') {
                int rows = vt->esc_param_count > 0 ? vt->esc_params[0] : 1;
                vt->cursor_row -= rows;
                if (vt->cursor_row < 0) vt->cursor_row = 0;
                vt->esc_state = 0;
            } else if (ch == 'B') {
                int rows = vt->esc_param_count > 0 ? vt->esc_params[0] : 1;
                vt->cursor_row += rows;
                if (vt->cursor_row >= vt->rows) vt->cursor_row = vt->rows - 1;
                vt->esc_state = 0;
            } else if (ch == 'C') {
                int cols = vt->esc_param_count > 0 ? vt->esc_params[0] : 1;
                vt->cursor_col += cols;
                if (vt->cursor_col >= vt->cols) vt->cursor_col = vt->cols - 1;
                vt->esc_state = 0;
            } else if (ch == 'D') {
                int cols = vt->esc_param_count > 0 ? vt->esc_params[0] : 1;
                vt->cursor_col -= cols;
                if (vt->cursor_col < 0) vt->cursor_col = 0;
                vt->esc_state = 0;
            } else if (ch == 'H' || ch == 'f') {
                int r = vt->esc_param_count > 0 ? vt->esc_params[0] : 1;
                int c = vt->esc_param_count > 1 ? vt->esc_params[1] : 1;
                vt->cursor_row = r - 1; vt->cursor_col = c - 1;
                if (vt->cursor_row < 0) vt->cursor_row = 0;
                if (vt->cursor_row >= vt->rows) vt->cursor_row = vt->rows - 1;
                if (vt->cursor_col < 0) vt->cursor_col = 0;
                if (vt->cursor_col >= vt->cols) vt->cursor_col = vt->cols - 1;
                vt->esc_state = 0;
            } else if (ch == 'J') {
                int mode = vt->esc_param_count > 0 ? vt->esc_params[0] : 0;
                if (mode == 0) {
                    for (int c = vt->cursor_col; c < vt->cols; c++) vt->screen[vt->cursor_row][c].ch = ' ';
                    for (int r = vt->cursor_row + 1; r < vt->rows; r++)
                        for (int c = 0; c < vt->cols; c++) vt->screen[r][c].ch = ' ';
                } else if (mode == 2) {
                    vt100_clear(vt);
                }
                vt->esc_state = 0;
            } else if (ch == 'K') {
                int mode = vt->esc_param_count > 0 ? vt->esc_params[0] : 0;
                if (mode == 0) {
                    for (int c = vt->cursor_col; c < vt->cols; c++) vt->screen[vt->cursor_row][c].ch = ' ';
                } else if (mode == 1) {
                    for (int c = 0; c <= vt->cursor_col; c++) vt->screen[vt->cursor_row][c].ch = ' ';
                } else if (mode == 2) {
                    for (int c = 0; c < vt->cols; c++) vt->screen[vt->cursor_row][c].ch = ' ';
                }
                vt->esc_state = 0;
            } else if (ch == 'm') {
                for (int pi = 0; pi <= vt->esc_param_count; pi++) {
                    int v = vt->esc_params[pi];
                    if (v == 0) { vt->fg_color = 7; vt->bg_color = 0; vt->attr = 0; }
                    else if (v == 1) vt->attr |= 1;
                    else if (v == 7) vt->attr |= 2;
                    else if (v >= 30 && v <= 37) vt->fg_color = (uint8_t)(v - 30);
                    else if (v >= 40 && v <= 47) vt->bg_color = (uint8_t)(v - 40);
                    else if (v == 38 && pi + 2 <= vt->esc_param_count && vt->esc_params[pi + 1] == 5) {
                        vt->fg_color = (uint8_t)vt->esc_params[pi + 2]; pi += 2;
                    } else if (v == 48 && pi + 2 <= vt->esc_param_count && vt->esc_params[pi + 1] == 5) {
                        vt->bg_color = (uint8_t)vt->esc_params[pi + 2]; pi += 2;
                    }
                }
                vt->esc_state = 0;
            } else if (ch == 's') {
                vt->saved_row = vt->cursor_row; vt->saved_col = vt->cursor_col; vt->esc_state = 0;
            } else if (ch == 'u') {
                vt->cursor_row = vt->saved_row; vt->cursor_col = vt->saved_col; vt->esc_state = 0;
            } else if (ch == 'S') {
                vt100_scroll_up(vt, vt->esc_param_count > 0 ? vt->esc_params[0] : 1); vt->esc_state = 0;
            } else if (ch == 'r') {
                vt->scroll_top = vt->esc_param_count > 0 ? vt->esc_params[0] - 1 : 0;
                vt->scroll_bottom = vt->esc_param_count > 1 ? vt->esc_params[1] - 1 : vt->rows - 1;
                vt->cursor_row = vt->scroll_top; vt->cursor_col = 0; vt->esc_state = 0;
            } else {
                vt->esc_state = 0;
            }
        }
    }
    spinlock_release(&vt->lock);
}

void vt100_render(vt100_t* vt)
{
    if (!vt || !vt->fb) return;
    (void)vt100_process_csi;
}

void pty_init(pty_manager_t* mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(pty_manager_t));
    spin_init(&mgr->lock);
}

pty_t* pty_create(pty_manager_t* mgr)
{
    if (!mgr) return NULL;
    spinlock_acquire(&mgr->lock);
    if (mgr->count >= PTY_MAX) { spinlock_release(&mgr->lock); return NULL; }
    pty_t* pty = &mgr->ptys[mgr->count];
    memset(pty, 0, sizeof(pty_t));
    spin_init(&pty->lock);
    pty->index = mgr->count;
    pty->echo = 1;
    pty->canonical = 1;
    vt100_init(&pty->vt, 25, 80, NULL);
    mgr->count++;
    spinlock_release(&mgr->lock);
    return pty;
}

int pty_write_master(pty_t* pty, const void* data, uint32_t len)
{
    if (!pty || !data) return -1;
    spinlock_acquire(&pty->lock);
    const uint8_t* src = (const uint8_t*)data;
    uint32_t written = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t next = (pty->slave_head + 1) % PTY_BUFFER_SIZE;
        if (next == pty->slave_tail) break;
        pty->slave_buf[pty->slave_head] = src[i];
        pty->slave_head = next;
        written++;
    }
    spinlock_release(&pty->lock);
    return (int)written;
}

int pty_read_master(pty_t* pty, void* buf, uint32_t len)
{
    if (!pty || !buf) return -1;
    spinlock_acquire(&pty->lock);
    uint8_t* dst = (uint8_t*)buf;
    uint32_t read = 0;
    while (read < len && pty->master_tail != pty->master_head) {
        dst[read++] = pty->master_buf[pty->master_tail];
        pty->master_tail = (pty->master_tail + 1) % PTY_BUFFER_SIZE;
    }
    spinlock_release(&pty->lock);
    return (int)read;
}

int pty_write_slave(pty_t* pty, const void* data, uint32_t len)
{
    if (!pty || !data) return -1;
    spinlock_acquire(&pty->lock);
    const uint8_t* src = (const uint8_t*)data;
    uint32_t written = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (pty->echo) {
            uint32_t next = (pty->master_head + 1) % PTY_BUFFER_SIZE;
            if (next != pty->master_tail) {
                pty->master_buf[pty->master_head] = src[i];
                pty->master_head = next;
            }
        }
        uint32_t next = (pty->slave_head + 1) % PTY_BUFFER_SIZE;
        if (next == pty->slave_tail) break;
        pty->slave_buf[pty->slave_head] = src[i];
        pty->slave_head = next;
        written++;
    }
    spinlock_release(&pty->lock);
    return (int)written;
}

int pty_read_slave(pty_t* pty, void* buf, uint32_t len)
{
    if (!pty || !buf) return -1;
    spinlock_acquire(&pty->lock);
    uint8_t* dst = (uint8_t*)buf;
    uint32_t read = 0;
    while (read < len && pty->slave_tail != pty->slave_head) {
        dst[read++] = pty->slave_buf[pty->slave_tail];
        pty->slave_tail = (pty->slave_tail + 1) % PTY_BUFFER_SIZE;
    }
    spinlock_release(&pty->lock);
    return (int)read;
}