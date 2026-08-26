#ifndef ARCH_X86_64_AC97_H
#define ARCH_X86_64_AC97_H

#include <arch/types.h>

#define AC97_MAX_CONTROLLERS 4

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint64_t base_address;
    uint64_t dma_base;
    uint8_t interrupt;
    uint16_t vendor_id;
    uint16_t device_id;
} ac97_controller_t;

void ac97_init(void);
ac97_controller_t* ac97_get_controller(uint8_t index);
uint8_t ac97_get_controller_count(void);
int ac97_get_controller_info(uint8_t index, ac97_controller_t* info);
uint16_t ac97_get_vendor_id(uint8_t index);
uint16_t ac97_get_device_id(uint8_t index);
int ac97_read_register(uint16_t reg, uint16_t* value);
int ac97_write_register(uint16_t reg, uint16_t value);
int ac97_play(const uint8_t* data, uint32_t length);
int ac97_record(uint8_t* data, uint32_t* length);

#endif
