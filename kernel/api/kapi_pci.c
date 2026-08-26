#include "kapi_pci.h"
#include "kapi.h"

#include <arch/pci.h>
#include <string.h>

#define KAPI_MAX_PCI_DEVICES 256

static kapi_pci_dev_t kapi_pci_devices[KAPI_MAX_PCI_DEVICES];
static int kapi_pci_dev_count = 0;
static int kapi_pci_initialized = 0;

int kapi_pci_init(void)
{
    if (kapi_pci_initialized) {
        return KAPI_OK;
    }

    memset(kapi_pci_devices, 0, sizeof(kapi_pci_devices));
    kapi_pci_dev_count = 0;
    kapi_pci_initialized = 1;
    return KAPI_OK;
}

uint32_t kapi_pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
    return pci_read_config(bus, dev, func, offset);
}

uint8_t kapi_pci_read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
    return pci_read_config_byte(bus, dev, func, offset);
}

uint16_t kapi_pci_read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
    return pci_read_config_word(bus, dev, func, offset);
}

uint32_t kapi_pci_read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
    return pci_read_config_dword(bus, dev, func, offset);
}

void kapi_pci_write_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value)
{
    pci_write_config(bus, dev, func, offset, value);
}

void kapi_pci_write_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t value)
{
    pci_write_config_byte(bus, dev, func, offset, value);
}

void kapi_pci_write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value)
{
    pci_write_config_word(bus, dev, func, offset, value);
}

void kapi_pci_write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value)
{
    pci_write_config_dword(bus, dev, func, offset, value);
}

int kapi_pci_find_device(uint16_t vendor_id, uint16_t device_id, uint8_t* bus, uint8_t* dev, uint8_t* func)
{
    return pci_find_device(vendor_id, device_id, bus, dev, func);
}

int kapi_pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t* bus, uint8_t* dev, uint8_t* func)
{
    return pci_find_class(class_code, subclass, bus, dev, func);
}

int kapi_pci_enable_device(kapi_pci_dev_t* dev)
{
    if (!dev) {
        return KAPI_EINVAL;
    }

    if (dev->enabled) {
        return KAPI_OK;
    }

    uint16_t cmd = pci_read_config_word(dev->bus, dev->dev, dev->func, 0x04);
    cmd |= KAPI_PCI_COMMAND_IO | KAPI_PCI_COMMAND_MEMORY | KAPI_PCI_COMMAND_MASTER;
    pci_write_config_word(dev->bus, dev->dev, dev->func, 0x04, cmd);

    dev->enabled = 1;
    return KAPI_OK;
}

int kapi_pci_disable_device(kapi_pci_dev_t* dev)
{
    if (!dev) {
        return KAPI_EINVAL;
    }

    if (!dev->enabled) {
        return KAPI_OK;
    }

    uint16_t cmd = pci_read_config_word(dev->bus, dev->dev, dev->func, 0x04);
    cmd &= ~(KAPI_PCI_COMMAND_IO | KAPI_PCI_COMMAND_MEMORY | KAPI_PCI_COMMAND_MASTER);
    pci_write_config_word(dev->bus, dev->dev, dev->func, 0x04, cmd);

    dev->enabled = 0;
    return KAPI_OK;
}

int kapi_pci_set_master(kapi_pci_dev_t* dev, int enable)
{
    if (!dev) {
        return KAPI_EINVAL;
    }

    pci_set_bus_master(dev->bus, dev->dev, dev->func, enable);
    return KAPI_OK;
}

uint32_t kapi_pci_read_bar(kapi_pci_dev_t* dev, int bar_num)
{
    if (!dev || bar_num < 0 || bar_num >= KAPI_PCI_MAX_BARS) {
        return 0;
    }

    return pci_read_bar(dev->bus, dev->dev, dev->func, bar_num);
}

uint32_t kapi_pci_get_bar_size(kapi_pci_dev_t* dev, int bar_num)
{
    if (!dev || bar_num < 0 || bar_num >= KAPI_PCI_MAX_BARS) {
        return 0;
    }

    return pci_get_bar_size(dev->bus, dev->dev, dev->func, bar_num);
}

int kapi_pci_enable_msi(kapi_pci_dev_t* dev, uint32_t vector)
{
    if (!dev) {
        return KAPI_EINVAL;
    }

    return pci_enable_msi(dev->bus, dev->dev, dev->func, vector);
}

int kapi_pci_enable_msix(kapi_pci_dev_t* dev, kapi_pci_msix_entry_t* entries, uint16_t count)
{
    if (!dev || !entries || count == 0) {
        return KAPI_EINVAL;
    }

    return pci_enable_msix(dev->bus, dev->dev, dev->func,
                           (pci_msix_entry_t*)entries, count);
}

void kapi_pci_disable_msix(kapi_pci_dev_t* dev)
{
    if (!dev) {
        return;
    }

    pci_disable_msix(dev->bus, dev->dev, dev->func);
}

uint8_t kapi_pci_find_capability(kapi_pci_dev_t* dev, uint8_t cap_id)
{
    if (!dev) {
        return 0;
    }

    return pci_find_capability(dev->bus, dev->dev, dev->func, cap_id);
}

int kapi_pci_get_device_info(uint8_t bus, uint8_t dev, uint8_t func, kapi_pci_dev_t* info)
{
    if (!info) {
        return KAPI_EINVAL;
    }

    if (bus >= KAPI_PCI_MAX_BUSES || dev >= KAPI_PCI_MAX_DEVICES || func >= KAPI_PCI_MAX_FUNCTIONS) {
        return KAPI_EINVAL;
    }

    uint16_t vendor_id = pci_read_config_word(bus, dev, func, 0x00);
    if (vendor_id == 0xFFFF) {
        return KAPI_ENOENT;
    }

    memset(info, 0, sizeof(*info));
    info->bus = bus;
    info->dev = dev;
    info->func = func;
    info->vendor_id = vendor_id;
    info->device_id = pci_read_config_word(bus, dev, func, 0x02);
    info->class_code = pci_read_config_byte(bus, dev, func, 0x0B);
    info->subclass = pci_read_config_byte(bus, dev, func, 0x0A);
    info->prog_if = pci_read_config_byte(bus, dev, func, 0x09);
    info->header_type = pci_read_config_byte(bus, dev, func, 0x0E);
    info->irq_line = pci_read_config_byte(bus, dev, func, 0x3C);
    info->irq_pin = pci_read_config_byte(bus, dev, func, 0x3D);

    for (int i = 0; i < KAPI_PCI_MAX_BARS; i++) {
        info->bar[i] = pci_read_bar(bus, dev, func, i);
        info->bar_size[i] = pci_get_bar_size(bus, dev, func, i);
    }

    return KAPI_OK;
}