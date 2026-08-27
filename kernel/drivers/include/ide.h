#ifndef KERNEL_DRIVERS_IDE_H
#define KERNEL_DRIVERS_IDE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define IDE_PRIMARY_DATA         0x1F0
#define IDE_PRIMARY_ERROR        0x1F1
#define IDE_PRIMARY_SECTOR_COUNT 0x1F2
#define IDE_PRIMARY_SECTOR_NUM   0x1F3
#define IDE_PRIMARY_CYL_LOW      0x1F4
#define IDE_PRIMARY_CYL_HIGH     0x1F5
#define IDE_PRIMARY_DRIVE        0x1F6
#define IDE_PRIMARY_STATUS       0x1F7
#define IDE_PRIMARY_COMMAND      0x1F7
#define IDE_PRIMARY_ALT_STATUS   0x3F6
#define IDE_PRIMARY_CONTROL      0x3F6

#define IDE_SECONDARY_DATA         0x170
#define IDE_SECONDARY_STATUS       0x177
#define IDE_SECONDARY_COMMAND      0x177
#define IDE_SECONDARY_ALT_STATUS   0x376
#define IDE_SECONDARY_CONTROL      0x376

#define IDE_CMD_READ_PIO    0x20
#define IDE_CMD_READ_PIO_EXT 0x24
#define IDE_CMD_WRITE_PIO   0x30
#define IDE_CMD_WRITE_PIO_EXT 0x34
#define IDE_CMD_READ_DMA    0xC8
#define IDE_CMD_READ_DMA_EXT 0x25
#define IDE_CMD_WRITE_DMA   0xCA
#define IDE_CMD_WRITE_DMA_EXT 0x35
#define IDE_CMD_IDENTIFY    0xEC
#define IDE_CMD_SET_FEATURES 0xEF

#define IDE_STATUS_ERR      0x01
#define IDE_STATUS_DRQ      0x08
#define IDE_STATUS_SRV      0x10
#define IDE_STATUS_DF       0x20
#define IDE_STATUS_RDY      0x40
#define IDE_STATUS_BSY      0x80

#define IDE_CTRL_NIEN       0x02
#define IDE_CTRL_SRST       0x04

#define IDE_IDENTIFY_SECTORS48    100
#define IDE_IDENTIFY_LBA48_LOW    100
#define IDE_IDENTIFY_LBA48_HIGH   102
#define IDE_IDENTIFY_SECTORS28    60

#define IDE_MAX_CHANNELS   2
#define IDE_MAX_DRIVES     2

typedef struct {
    uint16_t data_port;
    uint16_t error_port;
    uint16_t sector_count_port;
    uint16_t sector_num_port;
    uint16_t cyl_low_port;
    uint16_t cyl_high_port;
    uint16_t drive_port;
    uint16_t status_port;
    uint16_t command_port;
    uint16_t alt_status_port;
    uint16_t control_port;
    int      present;
} ide_channel_t;

typedef struct {
    int          present;
    int          channel;
    int          drive;
    int          is_ata;
    int          lba48;
    uint64_t     sector_count;
    uint32_t     sector_size;
    uint16_t     identify[256];
    ide_channel_t* ch;
    spinlock_t   lock;
} ide_drive_t;

typedef struct {
    ide_channel_t channels[IDE_MAX_CHANNELS];
    ide_drive_t   drives[IDE_MAX_CHANNELS * IDE_MAX_DRIVES];
    int           drive_count;
    spinlock_t    lock;
} ide_controller_t;

void  ide_init(ide_controller_t* ctrl);
int   ide_identify(ide_channel_t* ch, int drive, uint16_t* identify);
int   ide_read_pio(ide_drive_t* drv, uint64_t lba, void* buf, uint32_t count);
int   ide_write_pio(ide_drive_t* drv, uint64_t lba, const void* buf, uint32_t count);
int   ide_read_dma(ide_drive_t* drv, uint64_t lba, void* buf, uint32_t count);
int   ide_write_dma(ide_drive_t* drv, uint64_t lba, const void* buf, uint32_t count);
int   ide_wait_status(ide_channel_t* ch, uint8_t mask, uint8_t value, uint32_t timeout_ms);

#endif