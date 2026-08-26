#ifndef ARCH_X86_64_SATA_H
#define ARCH_X86_64_SATA_H

#include <arch/types.h>

#define SATA_MAX_DEVICES 4

typedef struct {
    uint8_t port;
    uint8_t device;
    uint64_t capacity;
    uint32_t sector_size;
} sata_device_t;

void sata_init(void);
int sata_read(uint8_t port, uint8_t device, uint64_t lba, void* buffer, uint32_t sectors);
int sata_write(uint8_t port, uint8_t device, uint64_t lba, const void* buffer, uint32_t sectors);
sata_device_t* sata_get_device(uint8_t port, uint8_t device);

#endif
