#include "svga.h"
#include <arch/io.h>
#include <arch/memory.h>
#include <string.h>

static void svga_write_reg(svga_dev_t* dev, uint32_t index, uint32_t value)
{
    outl(dev->index_port, index);
    outl(dev->value_port, value);
}

static uint32_t svga_read_reg(svga_dev_t* dev, uint32_t index)
{
    outl(dev->index_port, index);
    return inl(dev->value_port);
}

static void svga_fifo_write(svga_dev_t* dev, uint32_t value)
{
    volatile uint32_t* fifo = dev->fifo;
    uint32_t next_cmd = fifo[SVGA_FIFO_NEXT_CMD];
    uint32_t max = fifo[SVGA_FIFO_MAX];
    uint32_t min = fifo[SVGA_FIFO_MIN];

    fifo[next_cmd / 4] = value;
    next_cmd += 4;
    if (next_cmd >= max) next_cmd = min;
    fifo[SVGA_FIFO_NEXT_CMD] = next_cmd;
}

static void svga_commit(svga_dev_t* dev)
{
    svga_write_reg(dev, SVGA_REG_SYNC, 1);
    while (svga_read_reg(dev, SVGA_REG_BUSY) != 0);
}

int svga_init(svga_dev_t* dev, pci_device_t* pci_dev)
{
    if (!dev || !pci_dev) return -1;
    memset(dev, 0, sizeof(svga_dev_t));
    spin_init(&dev->lock);

    dev->pci_dev = pci_dev;
    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    dev->index_port = (uint32_t)pci_dev->bar[0];
    dev->value_port = (uint32_t)pci_dev->bar[1];

    uint32_t id = svga_read_reg(dev, SVGA_REG_ID);
    if (id != 0 && id != 1) return -2;

    svga_write_reg(dev, SVGA_REG_ID, 2);
    id = svga_read_reg(dev, SVGA_REG_ID);
    if (id != 2) return -3;

    dev->capabilities = svga_read_reg(dev, SVGA_REG_CAPABILITIES);
    dev->vram_size = svga_read_reg(dev, SVGA_REG_VRAM_SIZE);
    dev->fb_size = svga_read_reg(dev, SVGA_REG_FB_SIZE);

    uint32_t fb_start = svga_read_reg(dev, SVGA_REG_FB_START);
    dev->framebuffer = (void*)(uintptr_t)fb_start;

    if (dev->capabilities & SVGA_CAP_COMMAND_BUFFER) {
        uint32_t fifo_start = svga_read_reg(dev, SVGA_REG_MEM_START);
        uint32_t fifo_size = svga_read_reg(dev, SVGA_REG_MEM_SIZE);
        dev->fifo = (volatile uint32_t*)(uintptr_t)fifo_start;
        dev->fifo_size = fifo_size;
    }

    svga_write_reg(dev, SVGA_REG_GUEST_ID, SVGA_GUEST_ID_LINUX);
    svga_write_reg(dev, SVGA_REG_ENABLE, 1);

    dev->width = svga_read_reg(dev, SVGA_REG_WIDTH);
    dev->height = svga_read_reg(dev, SVGA_REG_HEIGHT);
    dev->bpp = svga_read_reg(dev, SVGA_REG_BITS_PER_PIXEL);
    dev->pitch = dev->width * (dev->bpp / 8);
    dev->enabled = 1;

    return 0;
}

void svga_shutdown(svga_dev_t* dev)
{
    if (!dev) return;
    svga_write_reg(dev, SVGA_REG_ENABLE, 0);
    dev->enabled = 0;
}

int svga_set_mode(svga_dev_t* dev, uint32_t width, uint32_t height, uint32_t bpp)
{
    if (!dev) return -1;
    spinlock_acquire(&dev->lock);

    svga_write_reg(dev, SVGA_REG_WIDTH, width);
    svga_write_reg(dev, SVGA_REG_HEIGHT, height);
    svga_write_reg(dev, SVGA_REG_BITS_PER_PIXEL, bpp);
    svga_write_reg(dev, SVGA_REG_CONFIG_DONE, 1);

    dev->width = width;
    dev->height = height;
    dev->bpp = bpp;
    dev->pitch = width * (bpp / 8);

    spinlock_release(&dev->lock);
    return 0;
}

void svga_update(svga_dev_t* dev, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!dev || !dev->fifo) return;
    spinlock_acquire(&dev->lock);
    svga_fifo_write(dev, SVGA_CMD_UPDATE);
    svga_fifo_write(dev, x);
    svga_fifo_write(dev, y);
    svga_fifo_write(dev, w);
    svga_fifo_write(dev, h);
    spinlock_release(&dev->lock);
}

void svga_rect_fill(svga_dev_t* dev, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (!dev || !dev->fifo) return;
    spinlock_acquire(&dev->lock);
    svga_fifo_write(dev, SVGA_CMD_RECT_FILL);
    svga_fifo_write(dev, color);
    svga_fifo_write(dev, x);
    svga_fifo_write(dev, y);
    svga_fifo_write(dev, w);
    svga_fifo_write(dev, h);
    spinlock_release(&dev->lock);
}

void svga_rect_copy(svga_dev_t* dev, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h)
{
    if (!dev || !dev->fifo) return;
    spinlock_acquire(&dev->lock);
    svga_fifo_write(dev, SVGA_CMD_RECT_COPY);
    svga_fifo_write(dev, sx);
    svga_fifo_write(dev, sy);
    svga_fifo_write(dev, dx);
    svga_fifo_write(dev, dy);
    svga_fifo_write(dev, w);
    svga_fifo_write(dev, h);
    spinlock_release(&dev->lock);
}

void svga_put_pixel(svga_dev_t* dev, uint32_t x, uint32_t y, uint32_t color)
{
    if (!dev || !dev->framebuffer || x >= dev->width || y >= dev->height) return;
    spinlock_acquire(&dev->lock);
    uint32_t* fb = (uint32_t*)dev->framebuffer;
    fb[y * dev->width + x] = color;
    spinlock_release(&dev->lock);
}

uint32_t svga_get_pixel(svga_dev_t* dev, uint32_t x, uint32_t y)
{
    if (!dev || !dev->framebuffer || x >= dev->width || y >= dev->height) return 0;
    uint32_t* fb = (uint32_t*)dev->framebuffer;
    return fb[y * dev->width + x];
}

void svga_sync(svga_dev_t* dev)
{
    if (!dev) return;
    spinlock_acquire(&dev->lock);
    dev->fence++;
    if (dev->fifo) {
        dev->fifo[SVGA_FIFO_FENCE] = dev->fence;
    }
    svga_commit(dev);
    spinlock_release(&dev->lock);
}

void svga_define_gmr(svga_dev_t* dev, uint32_t gmr_id, uint32_t num_pages)
{
    if (!dev || !dev->fifo) return;
    spinlock_acquire(&dev->lock);
    svga_fifo_write(dev, SVGA_CMD_DEFINE_GMR2);
    svga_fifo_write(dev, gmr_id);
    svga_fifo_write(dev, num_pages);
    spinlock_release(&dev->lock);
}

void svga_irq_handler(int irq, void* ctx)
{
    svga_dev_t* dev = (svga_dev_t*)ctx;
    if (!dev) return;
    (void)irq;
}