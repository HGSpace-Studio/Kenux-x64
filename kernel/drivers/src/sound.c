#include "sound.h"
#include <arch/io.h>
#include <arch/memory.h>
#include <string.h>

void sound_device_init(sound_device_t* dev, int type)
{
    if (!dev) return;
    memset(dev, 0, sizeof(sound_device_t));
    spin_init(&dev->lock);
    dev->type = type;
    dev->format.sample_rate = 44100;
    dev->format.channels = 2;
    dev->format.bits_per_sample = 16;
    dev->volume_master = 75;
    dev->volume_pcm = 75;
    dev->volume_mic = 50;
}

int sound_write(sound_device_t* dev, const void* data, uint32_t size)
{
    if (!dev || !data) return -1;
    spinlock_acquire(&dev->playback_buf.lock);
    const uint8_t* src = (const uint8_t*)data;
    uint32_t written = 0;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t next = (dev->playback_buf.head + 1) % dev->playback_buf.size;
        if (next == dev->playback_buf.tail) break;
        ((uint8_t*)dev->playback_buf.buffer)[dev->playback_buf.head] = src[i];
        dev->playback_buf.head = next;
        written++;
    }
    spinlock_release(&dev->playback_buf.lock);
    return (int)written;
}

int sound_read(sound_device_t* dev, void* data, uint32_t size)
{
    if (!dev || !data) return -1;
    spinlock_acquire(&dev->capture_buf.lock);
    uint8_t* dst = (uint8_t*)data;
    uint32_t read = 0;
    while (read < size && dev->capture_buf.tail != dev->capture_buf.head) {
        dst[read++] = ((uint8_t*)dev->capture_buf.buffer)[dev->capture_buf.tail];
        dev->capture_buf.tail = (dev->capture_buf.tail + 1) % dev->capture_buf.size;
    }
    spinlock_release(&dev->capture_buf.lock);
    return (int)read;
}

int sound_set_volume(sound_device_t* dev, int channel, int left, int right)
{
    if (!dev) return -1;
    if (dev->set_volume) return dev->set_volume(dev, channel, left, right);
    spinlock_acquire(&dev->lock);
    if (channel == 0) dev->volume_master = (left + right) / 2;
    else if (channel == 1) dev->volume_pcm = (left + right) / 2;
    else if (channel == 2) dev->volume_mic = (left + right) / 2;
    spinlock_release(&dev->lock);
    return 0;
}

int sound_get_volume(sound_device_t* dev, int channel, int* left, int* right)
{
    if (!dev) return -1;
    if (dev->get_volume) return dev->get_volume(dev, channel, left, right);
    spinlock_acquire(&dev->lock);
    int vol = 0;
    if (channel == 0) vol = dev->volume_master;
    else if (channel == 1) vol = dev->volume_pcm;
    else if (channel == 2) vol = dev->volume_mic;
    if (left) *left = vol;
    if (right) *right = vol;
    spinlock_release(&dev->lock);
    return 0;
}

static uint16_t ac97_read_codec(ac97_dev_t* dev, uint8_t reg)
{
    outw(dev->nam_port + reg, 0);
    return inw(dev->nam_port + reg);
}

static void ac97_write_codec(ac97_dev_t* dev, uint8_t reg, uint16_t val)
{
    outw(dev->nam_port + reg, val);
}

int ac97_init(ac97_dev_t* dev, pci_device_t* pci_dev)
{
    if (!dev || !pci_dev) return -1;
    memset(dev, 0, sizeof(ac97_dev_t));

    dev->pci_dev = pci_dev;
    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    dev->nam_port = (uint16_t)pci_dev->bar[0];
    dev->nabm_port = (uint16_t)pci_dev->bar[1];

    if (dev->nam_port == 0 || dev->nabm_port == 0) return -2;

    ac97_write_codec(dev, AC97_RESET, 0);
    uint16_t vendor1 = ac97_read_codec(dev, AC97_VENDOR_ID1);
    uint16_t vendor2 = ac97_read_codec(dev, AC97_VENDOR_ID2);
    (void)vendor1; (void)vendor2;

    ac97_write_codec(dev, AC97_MASTER_VOL, 0x0000);
    ac97_write_codec(dev, AC97_PCM_VOL, 0x0000);
    ac97_write_codec(dev, AC97_PCM_FRONT_DAC_RATE, 44100);
    ac97_write_codec(dev, AC97_PCM_ADC_RATE, 44100);

    dev->bdl_virt = memory_alloc_aligned(32 * 8, 4096);
    dev->dma_buf = memory_alloc_aligned(SOUND_BUF_SIZE * 2, 4096);
    if (!dev->bdl_virt || !dev->dma_buf) return -3;
    dev->bdl_phys = (uint32_t)(uintptr_t)dev->bdl_virt;
    dev->dma_phys = (uint32_t)(uintptr_t)dev->dma_buf;

    uint32_t* bdl = (uint32_t*)dev->bdl_virt;
    for (int i = 0; i < 32; i++) {
        bdl[i * 2] = dev->dma_phys;
        bdl[i * 2 + 1] = SOUND_BUF_SIZE;
    }

    outl(dev->nabm_port + AC97_PCM_OUT_BDL, dev->bdl_phys);

    sound_device_init(&dev->sound_dev, 0);
    dev->sound_dev.hw_ctx = dev;
    dev->sound_dev.start_playback = ac97_start_playback;
    dev->sound_dev.stop_playback = ac97_stop_playback;
    dev->sound_dev.set_volume = ac97_set_volume;
    dev->sound_dev.get_volume = ac97_get_volume;
    dev->sound_dev.set_format = ac97_set_format;
    dev->sound_dev.playback_buf.buffer = dev->dma_buf;
    dev->sound_dev.playback_buf.size = SOUND_BUF_SIZE;

    return 0;
}

void ac97_shutdown(ac97_dev_t* dev)
{
    if (!dev) return;
    ac97_stop_playback(&dev->sound_dev);
}

int ac97_start_playback(sound_device_t* dev)
{
    if (!dev) return -1;
    ac97_dev_t* ac97 = (ac97_dev_t*)dev->hw_ctx;
    if (!ac97) return -2;

    spinlock_acquire(&dev->lock);
    outb(ac97->nabm_port + AC97_PCM_OUT_LVI, 31);
    uint8_t ctrl = inb(ac97->nabm_port + AC97_PCM_OUT_LVI);
    ctrl |= 0x01;
    outb(ac97->nabm_port + AC97_PCM_OUT_LVI, ctrl);
    ac97->playing = 1;
    spinlock_release(&dev->lock);
    return 0;
}

int ac97_stop_playback(sound_device_t* dev)
{
    if (!dev) return -1;
    ac97_dev_t* ac97 = (ac97_dev_t*)dev->hw_ctx;
    if (!ac97) return -2;

    spinlock_acquire(&dev->lock);
    outb(ac97->nabm_port + AC97_PCM_OUT_LVI, 0);
    ac97->playing = 0;
    spinlock_release(&dev->lock);
    return 0;
}

int ac97_set_volume(sound_device_t* dev, int channel, int left, int right)
{
    if (!dev) return -1;
    ac97_dev_t* ac97 = (ac97_dev_t*)dev->hw_ctx;
    if (!ac97) return -2;

    uint16_t vol_l = (uint16_t)((100 - left) * 31 / 100);
    uint16_t vol_r = (uint16_t)((100 - right) * 31 / 100);
    uint16_t val = (vol_l << 8) | vol_r;

    if (channel == 0) ac97_write_codec(ac97, AC97_MASTER_VOL, val);
    else if (channel == 1) ac97_write_codec(ac97, AC97_PCM_VOL, val);
    return 0;
}

int ac97_get_volume(sound_device_t* dev, int channel, int* left, int* right)
{
    if (!dev) return -1;
    ac97_dev_t* ac97 = (ac97_dev_t*)dev->hw_ctx;
    if (!ac97) return -2;

    uint16_t val = 0;
    if (channel == 0) val = ac97_read_codec(ac97, AC97_MASTER_VOL);
    else if (channel == 1) val = ac97_read_codec(ac97, AC97_PCM_VOL);

    int l = (int)((val >> 8) & 0x1F);
    int r = (int)(val & 0x1F);
    if (left) *left = 100 - (l * 100 / 31);
    if (right) *right = 100 - (r * 100 / 31);
    return 0;
}

int ac97_set_format(sound_device_t* dev, const sound_format_t* fmt)
{
    if (!dev || !fmt) return -1;
    ac97_dev_t* ac97 = (ac97_dev_t*)dev->hw_ctx;
    if (!ac97) return -2;

    ac97_write_codec(ac97, AC97_PCM_FRONT_DAC_RATE, (uint16_t)fmt->sample_rate);
    ac97_write_codec(ac97, AC97_PCM_ADC_RATE, (uint16_t)fmt->sample_rate);
    dev->format = *fmt;
    return 0;
}

void ac97_irq_handler(int irq, void* ctx)
{
    ac97_dev_t* dev = (ac97_dev_t*)ctx;
    if (!dev) return;
    uint16_t sr = inw(dev->nabm_port + AC97_PCM_OUT_SR);
    outw(dev->nabm_port + AC97_PCM_OUT_SR, sr);
    (void)irq;
}

int es1371_init(es1371_dev_t* dev, pci_device_t* pci_dev)
{
    if (!dev || !pci_dev) return -1;
    memset(dev, 0, sizeof(es1371_dev_t));

    dev->pci_dev = pci_dev;
    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    dev->mmio = (volatile uint32_t*)pci_map_bar(pci_dev, 0);
    if (!dev->mmio) return -2;

    dev->mmio[ES1371_REG_CONTROL / 4] = 0x00030000;

    dev->dac1_rate = 44100;
    dev->dac2_rate = 44100;
    dev->adc_rate = 44100;

    dev->dma_buf = memory_alloc_aligned(SOUND_BUF_SIZE * 2, 4096);
    if (!dev->dma_buf) return -3;
    dev->dma_phys = (uint32_t)(uintptr_t)dev->dma_buf;

    sound_device_init(&dev->sound_dev, 1);
    dev->sound_dev.hw_ctx = dev;
    dev->sound_dev.start_playback = es1371_start_playback;
    dev->sound_dev.stop_playback = es1371_stop_playback;
    dev->sound_dev.playback_buf.buffer = dev->dma_buf;
    dev->sound_dev.playback_buf.size = SOUND_BUF_SIZE;

    return 0;
}

void es1371_shutdown(es1371_dev_t* dev)
{
    if (!dev) return;
    es1371_stop_playback(&dev->sound_dev);
}

int es1371_start_playback(sound_device_t* dev)
{
    if (!dev) return -1;
    es1371_dev_t* es = (es1371_dev_t*)dev->hw_ctx;
    if (!es) return -2;

    spinlock_acquire(&dev->lock);
    uint32_t ctrl = es->mmio[ES1371_REG_CONTROL / 4];
    ctrl |= (1 << 2);
    es->mmio[ES1371_REG_CONTROL / 4] = ctrl;
    es->playing = 1;
    spinlock_release(&dev->lock);
    return 0;
}

int es1371_stop_playback(sound_device_t* dev)
{
    if (!dev) return -1;
    es1371_dev_t* es = (es1371_dev_t*)dev->hw_ctx;
    if (!es) return -2;

    spinlock_acquire(&dev->lock);
    uint32_t ctrl = es->mmio[ES1371_REG_CONTROL / 4];
    ctrl &= ~(1 << 2);
    es->mmio[ES1371_REG_CONTROL / 4] = ctrl;
    es->playing = 0;
    spinlock_release(&dev->lock);
    return 0;
}

void es1371_irq_handler(int irq, void* ctx)
{
    es1371_dev_t* dev = (es1371_dev_t*)ctx;
    if (!dev) return;
    uint32_t status = dev->mmio[ES1371_REG_STATUS / 4];
    dev->mmio[ES1371_REG_STATUS / 4] = status;
    (void)irq;
}

void i2c_init(i2c_bus_t* bus)
{
    if (!bus) return;
    memset(bus, 0, sizeof(i2c_bus_t));
    spin_init(&bus->lock);
}

i2c_adapter_t* i2c_register(i2c_bus_t* bus, uint32_t speed)
{
    if (!bus) return NULL;
    spinlock_acquire(&bus->lock);
    if (bus->count >= I2C_MAX_ADAPTERS) { spinlock_release(&bus->lock); return NULL; }
    i2c_adapter_t* adapter = &bus->adapters[bus->count];
    memset(adapter, 0, sizeof(i2c_adapter_t));
    spin_init(&adapter->lock);
    adapter->id = bus->count;
    adapter->speed = speed;
    bus->count++;
    spinlock_release(&bus->lock);
    return adapter;
}

int i2c_write(i2c_adapter_t* adapter, uint8_t addr, const uint8_t* data, int len)
{
    if (!adapter || !data) return -1;
    return i2c_xfer(adapter, addr, (uint8_t*)data, len, NULL, 0);
}

int i2c_read(i2c_adapter_t* adapter, uint8_t addr, uint8_t* data, int len)
{
    if (!adapter || !data) return -1;
    return i2c_xfer(adapter, addr, NULL, 0, data, len);
}

int i2c_xfer(i2c_adapter_t* adapter, uint8_t addr, uint8_t* wbuf, int wlen, uint8_t* rbuf, int rlen)
{
    if (!adapter) return -1;
    if (!adapter->xfer) return -2;
    spinlock_acquire(&adapter->lock);
    int result = adapter->xfer(adapter, addr, wbuf, wlen, rbuf, rlen);
    spinlock_release(&adapter->lock);
    return result;
}