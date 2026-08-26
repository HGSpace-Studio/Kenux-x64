#include <arch/sound.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/hda.h>
#include <arch/pcspk.h>
#include <string.h>

static sound_device_t sound_devices[SOUND_MAX_DEVICES];
static uint8_t sound_device_count = 0;

void sound_init(void)
{
    memset(sound_devices, 0, sizeof(sound_devices));
    sound_device_count = 0;
    pcspk_init();

    uint8_t bus, device, function;
    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t class_code_reg = pci_read_config(b, d, f, 0x08);
                if (class_code_reg == 0xFFFFFFFF) {
                    continue;
                }
                uint8_t class_code = (class_code_reg >> 24) & 0xFF;
                uint8_t subclass = (class_code_reg >> 16) & 0xFF;
                if (class_code == 0x04 && subclass == 0x03) {
                    if (sound_device_count < SOUND_MAX_DEVICES) {
                        uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                        cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
                        pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);
                        sound_device_t* dev = &sound_devices[sound_device_count++];
                        dev->type = SOUND_HDA;
                        dev->bus = b;
                        dev->device = d;
                        dev->function = f;
                        dev->vendor_id = pci_read_config(b, d, f, 0x00) & 0xFFFF;
                        dev->device_id = (pci_read_config(b, d, f, 0x00) >> 16) & 0xFFFF;
                        dev->interrupt = pci_read_config(b, d, f, 0x3C) & 0xFF;
                        dev->base_address = pci_read_config(b, d, f, 0x10) & 0xFFFFFFF0;
                    }
                } else if (class_code == 0x04 && subclass == 0x01) {
                    if (sound_device_count < SOUND_MAX_DEVICES) {
                        uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                        cmd |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER;
                        pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);
                        sound_device_t* dev = &sound_devices[sound_device_count++];
                        dev->type = SOUND_AC97;
                        dev->bus = b;
                        dev->device = d;
                        dev->function = f;
                        dev->vendor_id = pci_read_config(b, d, f, 0x00) & 0xFFFF;
                        dev->device_id = (pci_read_config(b, d, f, 0x00) >> 16) & 0xFFFF;
                        dev->interrupt = pci_read_config(b, d, f, 0x3C) & 0xFF;
                        dev->base_address = pci_read_config(b, d, f, 0x10) & 0xFFFFFFF0;
                    }
                }
            }
        }
    }
}

int sound_get_device_info(uint8_t index, sound_device_t* info)
{
    if (index >= sound_device_count) {
        return -1;
    }

    memcpy(info, &sound_devices[index], sizeof(sound_device_t));
    return 0;
}

uint16_t sound_get_vendor_id(uint8_t index)
{
    if (index >= sound_device_count) {
        return 0;
    }
    return sound_devices[index].vendor_id;
}

uint16_t sound_get_device_id(uint8_t index)
{
    if (index >= sound_device_count) {
        return 0;
    }
    return sound_devices[index].device_id;
}

sound_device_t* sound_get_device(uint8_t index)
{
    if (index >= sound_device_count) {
        return NULL;
    }
    return &sound_devices[index];
}

int sound_play(sound_device_t* dev, const uint8_t* data, uint32_t length)
{
    if (!dev || !data) {
        return -1;
    }

    if (dev->type == SOUND_HDA) {
        return hda_play(data, length);
    }

    pcspk_play_tone(880, 70);
    return 0;
}

int sound_record(sound_device_t* dev, uint8_t* buffer, uint32_t* length)
{
    if (!dev || !buffer || !length) {
        return -1;
    }

    if (dev->type == SOUND_HDA) {
        return hda_record(buffer, length);
    }

    return -1;
}
