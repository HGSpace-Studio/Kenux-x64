#ifndef ARCH_X86_64_IDE_H
#define ARCH_X86_64_IDE_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define IDE_PRIMARY      0
#define IDE_SECONDARY    1

#define IDE_MASTER       0
#define IDE_SLAVE        1

#define IDE_MAX_CHANNELS 2
#define IDE_MAX_DRIVES   4

#define IDE_SECTOR_SIZE  512

#define IDE_CMD_READ     0x20
#define IDE_CMD_WRITE    0x30
#define IDE_CMD_IDENTIFY 0xEC
#define IDE_CMD_READ_DMA 0xC8
#define IDE_CMD_WRITE_DMA 0xCA
#define IDE_CMD_SET_FEATURES 0xEF

#define IDE_FEATURE_DMA  0x01
#define IDE_FEATURE_PIO  0x00

#define IDE_STATUS_BSY   0x80
#define IDE_STATUS_DRDY  0x40
#define IDE_STATUS_DRQ   0x08
#define IDE_STATUS_ERR   0x01

#define IDE_ERROR_AMNF   0x01
#define IDE_ERROR_TK0NF  0x02
#define IDE_ERROR_ABRT   0x04
#define IDE_ERROR_MCR    0x08
#define IDE_ERROR_IDNF   0x10
#define IDE_ERROR_MC     0x20
#define IDE_ERROR_UNC    0x40
#define IDE_ERROR_BBK    0x80

typedef struct {
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint32_t total_sectors;
    uint32_t lba_sectors;
    char     model[41];
    char     serial[21];
    char     firmware[9];
    uint8_t  lba_supported;
    uint8_t  dma_supported;
    uint8_t  present;
    uint8_t  channel;
    uint8_t  drive;
} ide_device_t;

typedef struct {
    uint16_t base;
    uint16_t ctrl;
    uint8_t  n_int;
    ide_device_t devices[2];
    spinlock_t lock;
} ide_channel_t;

void ide_init(void);
int ide_read_sectors(uint8_t channel, uint8_t drive, uint32_t lba,
                     uint32_t count, void* buf);
int ide_write_sectors(uint8_t channel, uint8_t drive, uint32_t lba,
                      uint32_t count, const void* buf);
int ide_identify(uint8_t channel, uint8_t drive, ide_device_t* info);
const ide_device_t* ide_get_device(uint8_t channel, uint8_t drive);
void ide_irq_handler(int channel);

#endif