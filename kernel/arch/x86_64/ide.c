#include <arch/ide.h>
#include <arch/io.h>
#include <arch/memory.h>
#include <string.h>

static ide_channel_t ide_channels[IDE_MAX_CHANNELS];

static inline uint8_t ide_inb(uint16_t port) { return inb(port); }
static inline void ide_outb(uint16_t port, uint8_t val) { outb(port, val); }
static inline uint16_t ide_inw(uint16_t port) { return inw(port); }
static inline void ide_outw(uint16_t port, uint16_t val) { outw(port, val); }

static void ide_wait_bsy(uint16_t base)
{
    while (ide_inb(base + 7) & IDE_STATUS_BSY);
}

static int ide_wait_drq(uint16_t base, int write)
{
    for (int timeout = 0; timeout < 1000000; timeout++) {
        uint8_t status = ide_inb(base + 7);
        if (write) {
            if ((status & (IDE_STATUS_DRQ | IDE_STATUS_ERR)) == IDE_STATUS_DRQ) return 0;
        } else {
            if ((status & (IDE_STATUS_DRQ | IDE_STATUS_BSY | IDE_STATUS_ERR)) == IDE_STATUS_DRQ) return 0;
        }
        if (status & IDE_STATUS_ERR) return -1;
    }
    return -2;
}

static void ide_select_drive(uint16_t base, uint8_t drive, uint8_t lba_bits)
{
    uint8_t val = 0xA0 | (drive << 4) | lba_bits;
    ide_outb(base + 6, val);
    for (volatile int i = 0; i < 4; i++) ide_inb(base + 7);
}

static int ide_read_pio(uint16_t base, void* buf, int count)
{
    uint16_t* dst = (uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        if (ide_wait_drq(base, 0) < 0) return -1;
        for (int i = 0; i < IDE_SECTOR_SIZE / 2; i++) {
            dst[i] = ide_inw(base);
        }
        dst += IDE_SECTOR_SIZE / 2;
    }
    return 0;
}

static int ide_write_pio(uint16_t base, const void* buf, int count)
{
    const uint16_t* src = (const uint16_t*)buf;
    for (int s = 0; s < count; s++) {
        if (ide_wait_drq(base, 1) < 0) return -1;
        for (int i = 0; i < IDE_SECTOR_SIZE / 2; i++) {
            ide_outw(base, src[i]);
        }
        src += IDE_SECTOR_SIZE / 2;
        ide_wait_bsy(base);
    }
    return 0;
}

void ide_init(void)
{
    ide_channels[0].base = 0x1F0;
    ide_channels[0].ctrl = 0x3F6;
    ide_channels[0].n_int = 14;
    ide_channels[0].lock = SPINLOCK_INIT;

    ide_channels[1].base = 0x170;
    ide_channels[1].ctrl = 0x376;
    ide_channels[1].n_int = 15;
    ide_channels[1].lock = SPINLOCK_INIT;

    for (int ch = 0; ch < IDE_MAX_CHANNELS; ch++) {
        ide_outb(ide_channels[ch].ctrl, 0x04);
        for (volatile int i = 0; i < 4; i++);
        ide_outb(ide_channels[ch].ctrl, 0x00);

        for (int dr = 0; dr < 2; dr++) {
            ide_device_t* dev = &ide_channels[ch].devices[dr];
            memset(dev, 0, sizeof(ide_device_t));
            dev->channel = ch;
            dev->drive = dr;

            ide_select_drive(ide_channels[ch].base, dr, 0);
            ide_outb(ide_channels[ch].base + 2, 0);
            ide_outb(ide_channels[ch].base + 3, 0);
            ide_outb(ide_channels[ch].base + 4, 0);
            ide_outb(ide_channels[ch].base + 5, 0);
            ide_outb(ide_channels[ch].base + 7, IDE_CMD_IDENTIFY);

            uint8_t status = ide_inb(ide_channels[ch].base + 7);
            if (status == 0 || status == 0xFF) continue;

            while (1) {
                status = ide_inb(ide_channels[ch].base + 7);
                if ((status & IDE_STATUS_ERR) || (!(status & IDE_STATUS_BSY) && (status & IDE_STATUS_DRQ)))
                    break;
            }

            if (status & IDE_STATUS_ERR) continue;

            uint16_t id[256];
            for (int i = 0; i < 256; i++) id[i] = ide_inw(ide_channels[ch].base);

            dev->present = 1;
            dev->cylinders = id[1];
            dev->heads = id[3];
            dev->sectors = id[6];

            if (id[49] & 0x0200) dev->lba_supported = 1;
            if (id[49] & 0x0100) dev->dma_supported = 1;

            if (dev->lba_supported) {
                dev->lba_sectors = ((uint32_t)id[61] << 16) | id[60];
                dev->total_sectors = dev->lba_sectors;
            } else {
                dev->total_sectors = (uint32_t)dev->cylinders * dev->heads * dev->sectors;
            }

            for (int i = 0; i < 40; i += 2) {
                dev->model[i] = (char)(id[27 + i/2] >> 8);
                dev->model[i+1] = (char)(id[27 + i/2] & 0xFF);
            }
            dev->model[40] = '\0';

            for (int i = 0; i < 20; i += 2) {
                dev->serial[i] = (char)(id[10 + i/2] >> 8);
                dev->serial[i+1] = (char)(id[10 + i/2] & 0xFF);
            }
            dev->serial[20] = '\0';

            for (int i = 0; i < 8; i += 2) {
                dev->firmware[i] = (char)(id[23 + i/2] >> 8);
                dev->firmware[i+1] = (char)(id[23 + i/2] & 0xFF);
            }
            dev->firmware[8] = '\0';
        }
    }
}

int ide_read_sectors(uint8_t channel, uint8_t drive, uint32_t lba,
                     uint32_t count, void* buf)
{
    if (channel >= IDE_MAX_CHANNELS || drive > 1 || !buf || count == 0) return -1;

    ide_channel_t* ch = &ide_channels[channel];
    if (!ch->devices[drive].present) return -2;

    spinlock_acquire(&ch->lock);

    ide_wait_bsy(ch->base);

    if (ch->devices[drive].lba_supported) {
        ide_select_drive(ch->base, drive, (uint8_t)((lba >> 24) & 0x0F));
        ide_outb(ch->base + 1, 0);
        ide_outb(ch->base + 2, (uint8_t)count);
        ide_outb(ch->base + 3, (uint8_t)(lba & 0xFF));
        ide_outb(ch->base + 4, (uint8_t)((lba >> 8) & 0xFF));
        ide_outb(ch->base + 5, (uint8_t)((lba >> 16) & 0xFF));
        ide_outb(ch->base + 7, IDE_CMD_READ);
    } else {
        uint32_t cyl = lba / (ch->devices[drive].heads * ch->devices[drive].sectors);
        uint32_t head = (lba / ch->devices[drive].sectors) % ch->devices[drive].heads;
        uint32_t sect = (lba % ch->devices[drive].sectors) + 1;

        ide_select_drive(ch->base, drive, (uint8_t)head);
        ide_outb(ch->base + 1, 0);
        ide_outb(ch->base + 2, (uint8_t)count);
        ide_outb(ch->base + 3, (uint8_t)sect);
        ide_outb(ch->base + 4, (uint8_t)(cyl & 0xFF));
        ide_outb(ch->base + 5, (uint8_t)((cyl >> 8) & 0xFF));
        ide_outb(ch->base + 7, IDE_CMD_READ);
    }

    int ret = ide_read_pio(ch->base, buf, count);
    spinlock_release(&ch->lock);
    return ret;
}

int ide_write_sectors(uint8_t channel, uint8_t drive, uint32_t lba,
                      uint32_t count, const void* buf)
{
    if (channel >= IDE_MAX_CHANNELS || drive > 1 || !buf || count == 0) return -1;

    ide_channel_t* ch = &ide_channels[channel];
    if (!ch->devices[drive].present) return -2;

    spinlock_acquire(&ch->lock);

    ide_wait_bsy(ch->base);

    if (ch->devices[drive].lba_supported) {
        ide_select_drive(ch->base, drive, (uint8_t)((lba >> 24) & 0x0F));
        ide_outb(ch->base + 1, 0);
        ide_outb(ch->base + 2, (uint8_t)count);
        ide_outb(ch->base + 3, (uint8_t)(lba & 0xFF));
        ide_outb(ch->base + 4, (uint8_t)((lba >> 8) & 0xFF));
        ide_outb(ch->base + 5, (uint8_t)((lba >> 16) & 0xFF));
        ide_outb(ch->base + 7, IDE_CMD_WRITE);
    } else {
        uint32_t cyl = lba / (ch->devices[drive].heads * ch->devices[drive].sectors);
        uint32_t head = (lba / ch->devices[drive].sectors) % ch->devices[drive].heads;
        uint32_t sect = (lba % ch->devices[drive].sectors) + 1;

        ide_select_drive(ch->base, drive, (uint8_t)head);
        ide_outb(ch->base + 1, 0);
        ide_outb(ch->base + 2, (uint8_t)count);
        ide_outb(ch->base + 3, (uint8_t)sect);
        ide_outb(ch->base + 4, (uint8_t)(cyl & 0xFF));
        ide_outb(ch->base + 5, (uint8_t)((cyl >> 8) & 0xFF));
        ide_outb(ch->base + 7, IDE_CMD_WRITE);
    }

    int ret = ide_write_pio(ch->base, buf, count);
    spinlock_release(&ch->lock);
    return ret;
}

int ide_identify(uint8_t channel, uint8_t drive, ide_device_t* info)
{
    if (channel >= IDE_MAX_CHANNELS || drive > 1 || !info) return -1;
    memcpy(info, &ide_channels[channel].devices[drive], sizeof(ide_device_t));
    return info->present ? 0 : -2;
}

const ide_device_t* ide_get_device(uint8_t channel, uint8_t drive)
{
    if (channel >= IDE_MAX_CHANNELS || drive > 1) return NULL;
    return &ide_channels[channel].devices[drive];
}

void ide_irq_handler(int channel)
{
    if (channel >= 0 && channel < IDE_MAX_CHANNELS) {
        ide_inb(ide_channels[channel].base + 7);
    }
}