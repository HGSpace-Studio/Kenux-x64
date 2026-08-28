#include "ide.h"
#include <arch/io.h>
#include <string.h>

static void ide_delay(void)
{
    for (volatile int i = 0; i < 1000; i++);
}

int ide_wait_status(ide_channel_t* ch, uint8_t mask, uint8_t value, uint32_t timeout_ms)
{
    if (!ch) return -1;
    uint32_t count = timeout_ms * 100;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t status = inb(ch->alt_status_port);
        if ((status & mask) == value) return 0;
        ide_delay();
    }
    return -1;
}

static int ide_wait_drq(ide_channel_t* ch, int write, uint32_t timeout_ms)
{
    uint32_t count = timeout_ms * 100;
    for (uint32_t i = 0; i < count; i++) {
        uint8_t status = inb(ch->alt_status_port);
        if (status & IDE_STATUS_ERR) return -1;
        if (status & IDE_STATUS_DF) return -2;
        if (write) {
            if ((status & (IDE_STATUS_DRQ | IDE_STATUS_BSY)) == IDE_STATUS_DRQ) return 0;
        } else {
            if ((status & (IDE_STATUS_DRQ | IDE_STATUS_BSY)) == IDE_STATUS_DRQ) return 0;
        }
        ide_delay();
    }
    return -3;
}

int ide_identify(ide_channel_t* ch, int drive, uint16_t* identify)
{
    if (!ch || !identify) return -1;

    outb(ch->drive_port, drive == 0 ? 0xA0 : 0xB0);
    ide_delay();
    outb(ch->sector_count_port, 0);
    outb(ch->sector_num_port, 0);
    outb(ch->cyl_low_port, 0);
    outb(ch->cyl_high_port, 0);
    outb(ch->command_port, IDE_CMD_IDENTIFY);

    uint8_t status = inb(ch->status_port);
    if (status == 0) return -2;

    int result = ide_wait_status(ch, IDE_STATUS_BSY, 0, 5000);
    if (result != 0) return -3;

    status = inb(ch->status_port);
    if (status & IDE_STATUS_ERR) return -4;

    result = ide_wait_drq(ch, 0, 5000);
    if (result != 0) return -5;

    for (int i = 0; i < 256; i++) {
        identify[i] = inw(ch->data_port);
    }

    return 0;
}

void ide_init(ide_controller_t* ctrl)
{
    if (!ctrl) return;
    memset(ctrl, 0, sizeof(ide_controller_t));
    spin_init(&ctrl->lock);

    ide_channel_t* ch0 = &ctrl->channels[0];
    ch0->data_port = IDE_PRIMARY_DATA;
    ch0->error_port = IDE_PRIMARY_ERROR;
    ch0->sector_count_port = IDE_PRIMARY_SECTOR_COUNT;
    ch0->sector_num_port = IDE_PRIMARY_SECTOR_NUM;
    ch0->cyl_low_port = IDE_PRIMARY_CYL_LOW;
    ch0->cyl_high_port = IDE_PRIMARY_CYL_HIGH;
    ch0->drive_port = IDE_PRIMARY_DRIVE;
    ch0->status_port = IDE_PRIMARY_STATUS;
    ch0->command_port = IDE_PRIMARY_COMMAND;
    ch0->alt_status_port = IDE_PRIMARY_ALT_STATUS;
    ch0->control_port = IDE_PRIMARY_CONTROL;

    ide_channel_t* ch1 = &ctrl->channels[1];
    ch1->data_port = IDE_SECONDARY_DATA;
    ch1->error_port = 0x171;
    ch1->sector_count_port = 0x172;
    ch1->sector_num_port = 0x173;
    ch1->cyl_low_port = 0x174;
    ch1->cyl_high_port = 0x175;
    ch1->drive_port = 0x176;
    ch1->status_port = IDE_SECONDARY_STATUS;
    ch1->command_port = IDE_SECONDARY_COMMAND;
    ch1->alt_status_port = IDE_SECONDARY_ALT_STATUS;
    ch1->control_port = IDE_SECONDARY_CONTROL;

    for (int c = 0; c < IDE_MAX_CHANNELS; c++) {
        ide_channel_t* ch = &ctrl->channels[c];
        outb(ch->control_port, IDE_CTRL_SRST);
        ide_delay();
        outb(ch->control_port, 0);
        ide_delay();

        for (int d = 0; d < IDE_MAX_DRIVES; d++) {
            ide_drive_t* drv = &ctrl->drives[ctrl->drive_count];
            uint16_t identify[256];

            int result = ide_identify(ch, d, identify);
            if (result != 0) continue;

            drv->present = 1;
            drv->channel = c;
            drv->drive = d;
            drv->ch = ch;
            drv->sector_size = 512;
            spin_init(&drv->lock);

            if ((identify[0] & 0x8000) == 0) {
                drv->is_ata = 1;
            }

            uint64_t lba28 = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
            uint64_t lba48 = (uint64_t)identify[100] | ((uint64_t)identify[101] << 16) |
                             ((uint64_t)identify[102] << 32) | ((uint64_t)identify[103] << 48);

            if (lba48 > 0 && (identify[83] & 0x0400)) {
                drv->lba48 = 1;
                drv->sector_count = lba48;
            } else {
                drv->lba48 = 0;
                drv->sector_count = lba28;
            }

            memcpy(drv->identify, identify, 512);
            ctrl->drive_count++;
        }
    }
}

int ide_read_pio(ide_drive_t* drv, uint64_t lba, void* buf, uint32_t count)
{
    if (!drv || !drv->present || !buf || count == 0) return -1;

    spinlock_acquire(&drv->lock);
    ide_channel_t* ch = drv->ch;
    uint8_t drive_sel = drv->drive == 0 ? 0xA0 : 0xB0;

    if (drv->lba48) {
        outb(ch->drive_port, drive_sel | 0x40);
        ide_delay();
        outb(ch->sector_count_port, (uint8_t)((count >> 8) & 0xFF));
        outb(ch->sector_num_port, (uint8_t)((lba >> 24) & 0xFF));
        outb(ch->cyl_low_port, (uint8_t)((lba >> 32) & 0xFF));
        outb(ch->cyl_high_port, (uint8_t)((lba >> 40) & 0xFF));
        outb(ch->sector_count_port, (uint8_t)(count & 0xFF));
        outb(ch->sector_num_port, (uint8_t)(lba & 0xFF));
        outb(ch->cyl_low_port, (uint8_t)((lba >> 8) & 0xFF));
        outb(ch->cyl_high_port, (uint8_t)((lba >> 16) & 0xFF));
        outb(ch->command_port, IDE_CMD_READ_PIO_EXT);
    } else {
        outb(ch->drive_port, drive_sel | 0xE0 | (uint8_t)((lba >> 24) & 0x0F));
        ide_delay();
        outb(ch->sector_count_port, (uint8_t)count);
        outb(ch->sector_num_port, (uint8_t)(lba & 0xFF));
        outb(ch->cyl_low_port, (uint8_t)((lba >> 8) & 0xFF));
        outb(ch->cyl_high_port, (uint8_t)((lba >> 16) & 0xFF));
        outb(ch->command_port, IDE_CMD_READ_PIO);
    }

    uint16_t* dst = (uint16_t*)buf;
    for (uint32_t s = 0; s < count; s++) {
        int result = ide_wait_drq(ch, 0, 5000);
        if (result != 0) {
            spinlock_release(&drv->lock);
            return result;
        }
        for (int i = 0; i < 256; i++) {
            dst[s * 256 + i] = inw(ch->data_port);
        }
    }

    spinlock_release(&drv->lock);
    return (int)(count * 512);
}

int ide_write_pio(ide_drive_t* drv, uint64_t lba, const void* buf, uint32_t count)
{
    if (!drv || !drv->present || !buf || count == 0) return -1;

    spinlock_acquire(&drv->lock);
    ide_channel_t* ch = drv->ch;
    uint8_t drive_sel = drv->drive == 0 ? 0xA0 : 0xB0;

    if (drv->lba48) {
        outb(ch->drive_port, drive_sel | 0x40);
        ide_delay();
        outb(ch->sector_count_port, (uint8_t)((count >> 8) & 0xFF));
        outb(ch->sector_num_port, (uint8_t)((lba >> 24) & 0xFF));
        outb(ch->cyl_low_port, (uint8_t)((lba >> 32) & 0xFF));
        outb(ch->cyl_high_port, (uint8_t)((lba >> 40) & 0xFF));
        outb(ch->sector_count_port, (uint8_t)(count & 0xFF));
        outb(ch->sector_num_port, (uint8_t)(lba & 0xFF));
        outb(ch->cyl_low_port, (uint8_t)((lba >> 8) & 0xFF));
        outb(ch->cyl_high_port, (uint8_t)((lba >> 16) & 0xFF));
        outb(ch->command_port, IDE_CMD_WRITE_PIO_EXT);
    } else {
        outb(ch->drive_port, drive_sel | 0xE0 | (uint8_t)((lba >> 24) & 0x0F));
        ide_delay();
        outb(ch->sector_count_port, (uint8_t)count);
        outb(ch->sector_num_port, (uint8_t)(lba & 0xFF));
        outb(ch->cyl_low_port, (uint8_t)((lba >> 8) & 0xFF));
        outb(ch->cyl_high_port, (uint8_t)((lba >> 16) & 0xFF));
        outb(ch->command_port, IDE_CMD_WRITE_PIO);
    }

    const uint16_t* src = (const uint16_t*)buf;
    for (uint32_t s = 0; s < count; s++) {
        int result = ide_wait_drq(ch, 1, 5000);
        if (result != 0) {
            spinlock_release(&drv->lock);
            return result;
        }
        for (int i = 0; i < 256; i++) {
            outw(ch->data_port, src[s * 256 + i]);
        }
    }

    spinlock_release(&drv->lock);
    return (int)(count * 512);
}

int ide_read_dma(ide_drive_t* drv, uint64_t lba, void* buf, uint32_t count)
{
    if (!drv || !drv->present || !buf) return -1;
    return ide_read_pio(drv, lba, buf, count);
}

int ide_write_dma(ide_drive_t* drv, uint64_t lba, const void* buf, uint32_t count)
{
    if (!drv || !drv->present || !buf) return -1;
    return ide_write_pio(drv, lba, buf, count);
}