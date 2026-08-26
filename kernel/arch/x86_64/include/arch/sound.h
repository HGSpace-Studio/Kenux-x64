#ifndef ARCH_X86_64_SOUND_H
#define ARCH_X86_64_SOUND_H

#include <arch/types.h>

#define SOUND_MAX_DEVICES 8
#define SOUND_HDA 0
#define SOUND_AC97 1

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint64_t base_address;
    uint8_t interrupt;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t type;
} sound_device_t;

void sound_init(void);
int sound_get_device_info(uint8_t index, sound_device_t* info);
uint16_t sound_get_vendor_id(uint8_t index);
uint16_t sound_get_device_id(uint8_t index);
sound_device_t* sound_get_device(uint8_t index);
int sound_play(sound_device_t* dev, const uint8_t* data, uint32_t length);
int sound_record(sound_device_t* dev, uint8_t* buffer, uint32_t* length);

#endif
