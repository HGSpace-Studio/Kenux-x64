#ifndef KERNEL_DRIVERS_DISPLAY_H
#define KERNEL_DRIVERS_DISPLAY_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define FBDEV_MAX          8

typedef struct {
    void*    framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint32_t size;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t red_shift;
    uint32_t green_shift;
    uint32_t blue_shift;
    int      index;
    spinlock_t lock;
} fbdev_t;

void     fbdev_init(fbdev_t* fb, void* addr, uint32_t w, uint32_t h, uint32_t bpp, uint32_t pitch);
void     fbdev_put_pixel(fbdev_t* fb, uint32_t x, uint32_t y, uint32_t color);
uint32_t fbdev_get_pixel(fbdev_t* fb, uint32_t x, uint32_t y);
void     fbdev_fill_rect(fbdev_t* fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void     fbdev_blit(fbdev_t* fb, uint32_t x, uint32_t y, const void* data, uint32_t w, uint32_t h, uint32_t src_pitch);
void     fbdev_scroll(fbdev_t* fb, int lines, uint32_t bg_color);
uint32_t fbdev_rgb(fbdev_t* fb, uint8_t r, uint8_t g, uint8_t b);

#define VT100_MAX_ROWS    80
#define VT100_MAX_COLS    200
#define VT100_MAX_HISTORY 4096

typedef struct {
    uint8_t ch;
    uint8_t fg;
    uint8_t bg;
    uint8_t attr;
} vt100_cell_t;

typedef struct {
    vt100_cell_t screen[VT100_MAX_ROWS][VT100_MAX_COLS];
    int          rows;
    int          cols;
    int          cursor_row;
    int          cursor_col;
    int          saved_row;
    int          saved_col;
    uint8_t      fg_color;
    uint8_t      bg_color;
    uint8_t      attr;
    int          esc_state;
    int          esc_params[16];
    int          esc_param_count;
    int          scroll_top;
    int          scroll_bottom;
    fbdev_t*     fb;
    int          font_width;
    int          font_height;
    spinlock_t   lock;
} vt100_t;

void  vt100_init(vt100_t* vt, int rows, int cols, fbdev_t* fb);
void  vt100_write(vt100_t* vt, const char* data, int len);
void  vt100_render(vt100_t* vt);
void  vt100_clear(vt100_t* vt);
void  vt100_scroll_up(vt100_t* vt, int lines);
void  vt100_set_cursor(vt100_t* vt, int row, int col);

#define PTY_MAX          64
#define PTY_BUFFER_SIZE  4096

typedef struct {
    int          index;
    int          master_fd;
    int          slave_fd;
    vt100_t      vt;
    uint8_t      master_buf[PTY_BUFFER_SIZE];
    uint32_t     master_head;
    uint32_t     master_tail;
    uint8_t      slave_buf[PTY_BUFFER_SIZE];
    uint32_t     slave_head;
    uint32_t     slave_tail;
    int          echo;
    int          canonical;
    spinlock_t   lock;
} pty_t;

typedef struct {
    pty_t        ptys[PTY_MAX];
    int          count;
    spinlock_t   lock;
} pty_manager_t;

void     pty_init(pty_manager_t* mgr);
pty_t*   pty_create(pty_manager_t* mgr);
int      pty_write_master(pty_t* pty, const void* data, uint32_t len);
int      pty_read_master(pty_t* pty, void* buf, uint32_t len);
int      pty_write_slave(pty_t* pty, const void* data, uint32_t len);
int      pty_read_slave(pty_t* pty, void* buf, uint32_t len);

#endif