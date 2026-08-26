#include <arch/hda.h>

#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/pcspk.h>
#include <string.h>

static hda_controller_t hda_controllers[HDA_MAX_CONTROLLERS];
static uint8_t hda_controller_count = 0;

void hda_init(void)
{
    memset(hda_controllers, 0, sizeof(hda_controllers));
    hda_controller_count = 0;

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
                    if (hda_controller_count < HDA_MAX_CONTROLLERS) {
                        uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                        cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
                        pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                        hda_controller_t* ctrl = &hda_controllers[hda_controller_count++];
                        ctrl->bus = b;
                        ctrl->device = d;
                        ctrl->function = f;
                        ctrl->base_address = pci_read_config(b, d, f, 0x10) & 0xFFFFFFF0;
                        ctrl->vendor_id = pci_read_config(b, d, f, 0x00) & 0xFFFF;
                        ctrl->device_id = (pci_read_config(b, d, f, 0x00) >> 16) & 0xFFFF;
                        ctrl->interrupt = pci_read_config(b, d, f, 0x3C) & 0xFF;
                    }
                }
            }
        }
    }
}

hda_controller_t* hda_get_controller(uint8_t index)
{
    if (index < hda_controller_count) {
        return &hda_controllers[index];
    }
    return NULL;
}

uint8_t hda_get_controller_count(void)
{
    return hda_controller_count;
}

int hda_get_controller_info(uint8_t index, hda_controller_t* info)
{
    if (index >= hda_controller_count) {
        return -1;
    }

    memcpy(info, &hda_controllers[index], sizeof(hda_controller_t));
    return 0;
}

uint16_t hda_get_vendor_id(uint8_t index)
{
    if (index >= hda_controller_count) {
        return 0;
    }

    return hda_controllers[index].vendor_id;
}

uint16_t hda_get_device_id(uint8_t index)
{
    if (index >= hda_controller_count) {
        return 0;
    }

    return hda_controllers[index].device_id;
}

int hda_read_register(uint32_t reg, uint32_t* value)
{
    if (hda_controller_count == 0 || !value) return -1;
    volatile uint32_t* addr = (volatile uint32_t*)(hda_controllers[0].base_address + reg);
    *value = *addr;
    return 0;
}

int hda_write_register(uint32_t reg, uint32_t value)
{
    if (hda_controller_count == 0) return -1;
    volatile uint32_t* addr = (volatile uint32_t*)(hda_controllers[0].base_address + reg);
    *addr = value;
    return 0;
}

int hda_play(const uint8_t* data, uint32_t length)
{
    if (!data || length == 0) return -1;
    if (hda_controller_count == 0) {
        pcspk_play_tone(880, 70);
        return 0;
    }

    /* HDA DMA/codec routing is not enabled in this minimal path yet.
     * Keep a safe audible fallback instead of reporting success silently. */
    pcspk_play_tone(880, 70);
    return 0;
}

int hda_record(uint8_t* data, uint32_t* length)
{
    if (!data || !length) return -1;
    if (hda_controller_count == 0) return -1;
    return 0;
}
