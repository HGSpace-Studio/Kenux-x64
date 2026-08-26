#ifndef ARCH_X86_64_HDA_H
#define ARCH_X86_64_HDA_H

#include <arch/types.h>

#define HDA_MAX_CONTROLLERS 4

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint64_t base_address;
    uint8_t interrupt;
    uint16_t vendor_id;
    uint16_t device_id;
} hda_controller_t;

void hda_init(void);
hda_controller_t* hda_get_controller(uint8_t index);
uint8_t hda_get_controller_count(void);
int hda_get_controller_info(uint8_t index, hda_controller_t* info);
uint16_t hda_get_vendor_id(uint8_t index);
uint16_t hda_get_device_id(uint8_t index);
int hda_read_register(uint32_t reg, uint32_t* value);
int hda_write_register(uint32_t reg, uint32_t value);
int hda_play(const uint8_t* data, uint32_t length);
int hda_record(uint8_t* data, uint32_t* length);

#endif
