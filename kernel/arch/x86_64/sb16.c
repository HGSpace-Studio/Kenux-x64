#include <arch/sb16.h>
#include <arch/io.h>
#include <arch/memory.h>
#include <arch/dma.h>
#include <string.h>

static sb16_t sb16_device;

static inline void sb16_dsp_write(sb16_t* dev, uint8_t val)
{
    for (volatile int i = 0; i < 1000; i++) {
        if ((inb(dev->base + SB16_DSP_STATUS) & 0x80) == 0) break;
    }
    outb(dev->base + SB16_DSP_WRITE, val);
}

static inline uint8_t sb16_dsp_read(sb16_t* dev)
{
    for (volatile int i = 0; i < 1000; i++) {
        if (inb(dev->base + SB16_DSP_DATA_AVAIL) & 0x80) break;
    }
    return inb(dev->base + SB16_DSP_READ);
}

static inline void sb16_mixer_write(sb16_t* dev, uint8_t reg, uint8_t val)
{
    outb(dev->base + SB16_MIXER_ADDR, reg);
    outb(dev->base + SB16_MIXER_DATA, val);
}

static inline uint8_t sb16_mixer_read(sb16_t* dev, uint8_t reg)
{
    outb(dev->base + SB16_MIXER_ADDR, reg);
    return inb(dev->base + SB16_MIXER_DATA);
}

void sb16_init(void)
{
    memset(&sb16_device, 0, sizeof(sb16_t));
    sb16_device.base = SB16_BASE_ADDR;
    spin_init(&sb16_device.lock);

    if (sb16_reset(&sb16_device) != 0) return;

    sb16_dsp_write(&sb16_device, SB16_CMD_GET_VERSION);
    sb16_device.major_ver = sb16_dsp_read(&sb16_device);
    sb16_device.minor_ver = sb16_dsp_read(&sb16_device);

    if (sb16_device.major_ver < 4) return;

    sb16_mixer_write(&sb16_device, SB16_MIXER_IRQ, SB16_IRQ_5);
    sb16_mixer_write(&sb16_device, SB16_MIXER_DMA, SB16_DMA_1 | SB16_DMA_5);
    sb16_device.irq = 5;
    sb16_device.dma8 = 1;
    sb16_device.dma16 = 5;

    sb16_device.dma_buffer = memory_alloc(SB16_BUFFER_SIZE * 2);
    if (!sb16_device.dma_buffer) return;
    sb16_device.dma_buffer_phys = (uint64_t)(uintptr_t)sb16_device.dma_buffer;
    sb16_device.buffer_size = SB16_BUFFER_SIZE;
    sb16_device.buffer_pos = 0;

    sb16_device.sample_rate = 44100;
    sb16_device.channels = 2;
    sb16_device.bits = 16;
    sb16_device.playing = 0;
    sb16_device.initialized = 1;
}

sb16_t* sb16_get_device(void)
{
    return sb16_device.initialized ? &sb16_device : NULL;
}

int sb16_reset(sb16_t* dev)
{
    if (!dev) return -1;

    outb(dev->base + SB16_DSP_RESET, 1);
    for (volatile int i = 0; i < 10000; i++);
    outb(dev->base + SB16_DSP_RESET, 0);
    for (volatile int i = 0; i < 10000; i++);

    uint8_t val = sb16_dsp_read(dev);
    if (val != 0xAA) return -2;

    return 0;
}

int sb16_set_sample_rate(sb16_t* dev, uint32_t rate)
{
    if (!dev || !dev->initialized) return -1;
    if (rate < SB16_SAMPLE_RATE_MIN || rate > SB16_SAMPLE_RATE_MAX) return -2;

    spinlock_acquire(&dev->lock);
    sb16_dsp_write(dev, SB16_CMD_SET_SAMPLE_RATE);
    sb16_dsp_write(dev, (uint8_t)(rate >> 8));
    sb16_dsp_write(dev, (uint8_t)(rate & 0xFF));
    dev->sample_rate = rate;
    spinlock_release(&dev->lock);
    return 0;
}

int sb16_set_format(sb16_t* dev, uint8_t channels, uint8_t bits)
{
    if (!dev || !dev->initialized) return -1;
    if (channels < 1 || channels > 2) return -2;
    if (bits != 8 && bits != 16) return -3;

    spinlock_acquire(&dev->lock);
    dev->channels = channels;
    dev->bits = bits;
    spinlock_release(&dev->lock);
    return 0;
}

int sb16_play(sb16_t* dev, const void* data, uint32_t size)
{
    if (!dev || !dev->initialized || !data || size == 0) return -1;

    spinlock_acquire(&dev->lock);

    uint32_t copy_size = size;
    if (copy_size > dev->buffer_size) copy_size = dev->buffer_size;

    memcpy(dev->dma_buffer, data, copy_size);
    dev->buffer_pos = 0;

    if (dev->bits == 16) {
        dma_start(dev->dma16, dev->dma_buffer_phys, copy_size, DMA_MODE_WRITE | DMA_MODE_AUTO | DMA_MODE_BLOCK);

        sb16_dsp_write(dev, SB16_CMD_SET_SAMPLE_RATE);
        sb16_dsp_write(dev, (uint8_t)(dev->sample_rate >> 8));
        sb16_dsp_write(dev, (uint8_t)(dev->sample_rate & 0xFF));

        sb16_dsp_write(dev, SB16_CMD_DMA_16BIT_DAC);
        uint16_t block_size = (uint16_t)(copy_size / (dev->bits / 8) - 1);
        sb16_dsp_write(dev, (uint8_t)(block_size & 0xFF));
        sb16_dsp_write(dev, (uint8_t)(block_size >> 8));
    } else {
        dma_start(dev->dma8, dev->dma_buffer_phys, copy_size, DMA_MODE_WRITE | DMA_MODE_AUTO | DMA_MODE_BLOCK);

        sb16_dsp_write(dev, SB16_CMD_DMA_8BIT_DAC);
        uint16_t block_size = (uint16_t)(copy_size - 1);
        sb16_dsp_write(dev, (uint8_t)(block_size & 0xFF));
        sb16_dsp_write(dev, (uint8_t)(block_size >> 8));
    }

    dev->playing = 1;
    spinlock_release(&dev->lock);
    return (int)copy_size;
}

int sb16_stop(sb16_t* dev)
{
    if (!dev || !dev->initialized) return -1;

    spinlock_acquire(&dev->lock);
    if (dev->bits == 16) {
        sb16_dsp_write(dev, SB16_CMD_PAUSE_16BIT);
    } else {
        sb16_dsp_write(dev, SB16_CMD_PAUSE_8BIT);
    }
    dev->playing = 0;
    spinlock_release(&dev->lock);
    return 0;
}

int sb16_pause(sb16_t* dev)
{
    if (!dev || !dev->initialized || !dev->playing) return -1;

    spinlock_acquire(&dev->lock);
    if (dev->bits == 16) {
        sb16_dsp_write(dev, SB16_CMD_PAUSE_16BIT);
    } else {
        sb16_dsp_write(dev, SB16_CMD_PAUSE_8BIT);
    }
    spinlock_release(&dev->lock);
    return 0;
}

int sb16_resume(sb16_t* dev)
{
    if (!dev || !dev->initialized) return -1;

    spinlock_acquire(&dev->lock);
    if (dev->bits == 16) {
        sb16_dsp_write(dev, SB16_CMD_CONTINUE_16BIT);
    } else {
        sb16_dsp_write(dev, SB16_CMD_CONTINUE_8BIT);
    }
    spinlock_release(&dev->lock);
    return 0;
}

void sb16_set_master_volume(sb16_t* dev, uint8_t left, uint8_t right)
{
    if (!dev || !dev->initialized) return;
    if (left > 31) left = 31;
    if (right > 31) right = 31;
    spinlock_acquire(&dev->lock);
    sb16_mixer_write(dev, SB16_MIXER_MASTER_L, left | (left << 3));
    sb16_mixer_write(dev, SB16_MIXER_MASTER_R, right | (right << 3));
    spinlock_release(&dev->lock);
}

void sb16_get_master_volume(sb16_t* dev, uint8_t* left, uint8_t* right)
{
    if (!dev || !dev->initialized) return;
    spinlock_acquire(&dev->lock);
    uint8_t l = sb16_mixer_read(dev, SB16_MIXER_MASTER_L) & 0x1F;
    uint8_t r = sb16_mixer_read(dev, SB16_MIXER_MASTER_R) & 0x1F;
    if (left) *left = l;
    if (right) *right = r;
    spinlock_release(&dev->lock);
}

void sb16_irq_handler(void)
{
    if (!sb16_device.initialized) return;
    inb(sb16_device.base + SB16_DSP_ACK_16BIT);
    inb(sb16_device.base + SB16_DSP_ACK_8BIT);
}