#ifndef KERNEL_DRIVERS_SVGA_H
#define KERNEL_DRIVERS_SVGA_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define VMWARE_SVGA_VENDOR  0x15AD
#define VMWARE_SVGA_DEVICE  0x0405

#define SVGA_REG_ID                0
#define SVGA_REG_ENABLE            1
#define SVGA_REG_WIDTH             2
#define SVGA_REG_HEIGHT            3
#define SVGA_REG_MAX_WIDTH         4
#define SVGA_REG_MAX_HEIGHT        5
#define SVGA_REG_DEPTH             6
#define SVGA_REG_BITS_PER_PIXEL    7
#define SVGA_REG_PSEUDOCOLOR       8
#define SVGA_REG_RED_MASK          9
#define SVGA_REG_GREEN_MASK        10
#define SVGA_REG_BLUE_MASK         11
#define SVGA_REG_BYTES_PER_PIXEL   12
#define SVGA_REG_FB_START          13
#define SVGA_REG_FB_OFFSET         14
#define SVGA_REG_VRAM_SIZE         15
#define SVGA_REG_FB_SIZE           16
#define SVGA_REG_CAPABILITIES      17
#define SVGA_REG_MEM_START         18
#define SVGA_REG_MEM_SIZE          19
#define SVGA_REG_CONFIG_DONE       20
#define SVGA_REG_SYNC              21
#define SVGA_REG_BUSY              22
#define SVGA_REG_GUEST_ID          23
#define SVGA_REG_CURSOR_ID         24
#define SVGA_REG_CURSOR_X          25
#define SVGA_REG_CURSOR_Y          26
#define SVGA_REG_CURSOR_ON         27
#define SVGA_REG_HOST_BITS_PER_PIXEL 28
#define SVGA_REG_TOP               29
#define SVGA_REG_LEFT              30
#define SVGA_REG_DISPLAY_WIDTH     31
#define SVGA_REG_DISPLAY_HEIGHT    32
#define SVGA_REG_GMR_ID            33
#define SVGA_REG_GMR_DESCRIPTOR    34
#define SVGA_REG_GMR_DATA          35
#define SVGA_REG_SCREENS           36
#define SVGA_REG_COMMAND_LOW       37
#define SVGA_REG_COMMAND_HIGH      38
#define SVGA_REG_PITCHLOCK         39
#define SVGA_REG_IRQ_MASK          40

#define SVGA_CAP_NONE              0
#define SVGA_CAP_RECT_COPY         (1 << 0)
#define SVGA_CAP_RECT_FILL         (1 << 1)
#define SVGA_CAP_RECT_PAT_FILL     (1 << 2)
#define SVGA_CAP_OFFSCREEN_1       (1 << 3)
#define SVGA_CAP_RASTER_OP         (1 << 4)
#define SVGA_CAP_CURSOR            (1 << 5)
#define SVGA_CAP_EXT_CURSOR        (1 << 6)
#define SVGA_CAP_ALPHA_CURSOR      (1 << 7)
#define SVGA_CAP_8BIT_EMULATION    (1 << 8)
#define SVGA_CAP_TWO_COLOR_PATTERN (1 << 9)
#define SVGA_CAP_X_CURSOR          (1 << 10)
#define SVGA_CAP_MULTIMON          (1 << 11)
#define SVGA_CAP_COMMAND_BUFFER    (1 << 12)
#define SVGA_CAP_CMD_LENGTH        (1 << 13)
#define SVGA_CAP_GMR              (1 << 14)
#define SVGA_CAP_GMR2             (1 << 15)
#define SVGA_CAP_SCREEN_OBJECT_2  (1 << 16)
#define SVGA_CAP_TRACES           (1 << 17)
#define SVGA_CAP_TOPDOWN_WINDOW    (1 << 18)
#define SVGA_CAP_READBACK          (1 << 19)
#define SVGA_CAP_IRQ              (1 << 23)

#define SVGA_CMD_INVALID           0
#define SVGA_CMD_UPDATE            1
#define SVGA_CMD_RECT_FILL         2
#define SVGA_CMD_RECT_COPY         3
#define SVGA_CMD_RECT_PAT_FILL     4
#define SVGA_CMD_RECT_ROP_FILL     5
#define SVGA_CMD_RECT_ROP_COPY     6
#define SVGA_CMD_RECT_ROP_PAT_FILL 7
#define SVGA_CMD_LINE              8
#define SVGA_CMD_BLIT_GMRFB_TO_SCREEN 14
#define SVGA_CMD_BLIT_SCREEN_TO_GMRFB 15
#define SVGA_CMD_ANNOTATION_FILL   16
#define SVGA_CMD_ANNOTATION_COPY   17
#define SVGA_CMD_DEFINE_GMR2      18
#define SVGA_CMD_REMAP_GMR2       19
#define SVGA_CMD_DEFINE_SCREEN    20
#define SVGA_CMD_DESTROY_SCREEN   21
#define SVGA_CMD_DEFINE_GMRFB     22
#define SVGA_CMD_BLIT_GMRFB_TO_SCREEN 23
#define SVGA_CMD_BLIT_SCREEN_TO_GMRFB 24
#define SVGA_CMD_UPDATE_CURSOR    25

#define SVGA_FIFO_MIN             0
#define SVGA_FIFO_MAX             1
#define SVGA_FIFO_NEXT_CMD        2
#define SVGA_FIFO_STOP            3
#define SVGA_FIFO_CAPABILITIES    4
#define SVGA_FIFO_FLAGS           5
#define SVGA_FIFO_FENCE           6
#define SVGA_FIFO_3D_HWVERSION    7
#define SVGA_FIFO_PITCHLOCK       8
#define SVGA_FIFO_CURSOR_X        9
#define SVGA_FIFO_CURSOR_Y        10
#define SVGA_FIFO_CURSOR_COUNT    11
#define SVGA_FIFO_CURSOR_LAST_UPDATED 12
#define SVGA_FIFO_RESERVED        13

#define SVGA_GUEST_ID_LINUX       0x0500

typedef struct {
    pci_device_t*       pci_dev;
    volatile uint32_t*  io_base;
    uint32_t            index_port;
    uint32_t            value_port;
    volatile uint32_t*  fifo;
    uint32_t            fifo_size;
    void*               framebuffer;
    uint32_t            fb_size;
    uint32_t            vram_size;
    uint32_t            width;
    uint32_t            height;
    uint32_t            bpp;
    uint32_t            pitch;
    uint32_t            capabilities;
    uint32_t            fence;
    int                 enabled;
    spinlock_t          lock;
} svga_dev_t;

int  svga_init(svga_dev_t* dev, pci_device_t* pci_dev);
void svga_shutdown(svga_dev_t* dev);
int  svga_set_mode(svga_dev_t* dev, uint32_t width, uint32_t height, uint32_t bpp);
void svga_update(svga_dev_t* dev, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void svga_rect_fill(svga_dev_t* dev, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void svga_rect_copy(svga_dev_t* dev, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h);
void svga_put_pixel(svga_dev_t* dev, uint32_t x, uint32_t y, uint32_t color);
uint32_t svga_get_pixel(svga_dev_t* dev, uint32_t x, uint32_t y);
void svga_sync(svga_dev_t* dev);
void svga_define_gmr(svga_dev_t* dev, uint32_t gmr_id, uint32_t num_pages);
void svga_irq_handler(int irq, void* ctx);

#endif