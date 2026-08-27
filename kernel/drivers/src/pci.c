#include "pci.h"
#include <arch/io.h>
#include <string.h>

static inline void pci_outl(uint16_t port, uint32_t val) { outl(port, val); }
static inline uint32_t pci_inl(uint16_t port) { return inl(port); }

void pci_init(pci_bus_t* bus)
{
    if (!bus) return;
    memset(bus, 0, sizeof(pci_bus_t));
    spin_init(&bus->lock);
    pci_scan_bus(bus);
}

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset)
{
    uint32_t addr = 0x80000000 | ((uint32_t)bus << 16) |
                    ((uint32_t)device << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    pci_outl(PCI_CONFIG_ADDRESS, addr);
    return pci_inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value)
{
    uint32_t addr = 0x80000000 | ((uint32_t)bus << 16) |
                    ((uint32_t)device << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    pci_outl(PCI_CONFIG_ADDRESS, addr);
    pci_outl(PCI_CONFIG_DATA, value);
}

void pci_scan_function(pci_bus_t* bus, uint8_t bus_num, uint8_t device, uint8_t func)
{
    if (!bus) return;

    uint16_t vendor = (uint16_t)(pci_config_read(bus_num, device, func, PCI_VENDOR_ID) & 0xFFFF);
    if (vendor == 0xFFFF) return;

    spinlock_acquire(&bus->lock);
    if (bus->device_count >= PCI_MAX_DEVICES_TOTAL) {
        spinlock_release(&bus->lock);
        return;
    }

    pci_device_t* dev = &bus->devices[bus->device_count++];
    memset(dev, 0, sizeof(pci_device_t));
    spin_init(&dev->lock);

    dev->bus = bus_num;
    dev->device = device;
    dev->function = func;
    dev->valid = 1;

    uint32_t val0 = pci_config_read(bus_num, device, func, 0x00);
    dev->vendor_id = (uint16_t)(val0 & 0xFFFF);
    dev->device_id = (uint16_t)(val0 >> 16);

    uint32_t val4 = pci_config_read(bus_num, device, func, 0x04);
    dev->command = (uint16_t)(val4 & 0xFFFF);
    dev->status = (uint16_t)(val4 >> 16);

    uint32_t val8 = pci_config_read(bus_num, device, func, 0x08);
    dev->revision_id = (uint8_t)(val8 & 0xFF);
    dev->prog_if = (uint8_t)((val8 >> 8) & 0xFF);
    dev->subclass_code = (uint8_t)((val8 >> 16) & 0xFF);
    dev->class_code = (uint8_t)((val8 >> 24) & 0xFF);

    uint32_t valC = pci_config_read(bus_num, device, func, 0x0C);
    dev->header_type = (uint8_t)((valC >> 16) & 0xFF);

    uint32_t val3C = pci_config_read(bus_num, device, func, 0x3C);
    dev->interrupt_line = (uint8_t)(val3C & 0xFF);
    dev->interrupt_pin = (uint8_t)((val3C >> 8) & 0xFF);

    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_config_read(bus_num, device, func, PCI_BAR0 + i * 4);
    }

    spinlock_release(&bus->lock);

    if (dev->class_code == PCI_CLASS_BRIDGE && dev->subclass_code == 0x04 &&
        dev->header_type == PCI_HEADER_BRIDGE) {
        uint32_t val18 = pci_config_read(bus_num, device, func, 0x18);
        uint8_t secondary_bus = (uint8_t)((val18 >> 8) & 0xFF);
        if (secondary_bus < PCI_MAX_BUSES && !bus->buses_scanned[secondary_bus]) {
            bus->buses_scanned[secondary_bus] = 1;
            pci_scan_bus(bus);
        }
    }
}

void pci_scan_slot(pci_bus_t* bus, uint8_t bus_num, uint8_t device)
{
    uint32_t val0 = pci_config_read(bus_num, device, 0, 0x00);
    if ((val0 & 0xFFFF) == 0xFFFF) return;

    uint32_t valC = pci_config_read(bus_num, device, 0, 0x0C);
    uint8_t header_type = (uint8_t)((valC >> 16) & 0xFF);

    if (header_type & 0x80) {
        for (uint8_t func = 0; func < PCI_MAX_FUNCTIONS; func++) {
            pci_scan_function(bus, bus_num, device, func);
        }
    } else {
        pci_scan_function(bus, bus_num, device, 0);
    }
}

void pci_scan_bus(pci_bus_t* bus)
{
    if (!bus) return;
    for (uint8_t device = 0; device < PCI_MAX_DEVICES; device++) {
        pci_scan_slot(bus, 0, device);
    }
}

pci_device_t* pci_find_device(pci_bus_t* bus, uint16_t vendor, uint16_t device_id)
{
    if (!bus) return NULL;
    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i].valid &&
            bus->devices[i].vendor_id == vendor &&
            bus->devices[i].device_id == device_id) {
            return &bus->devices[i];
        }
    }
    return NULL;
}

pci_device_t* pci_find_class(pci_bus_t* bus, uint8_t class_code, uint8_t subclass)
{
    if (!bus) return NULL;
    for (int i = 0; i < bus->device_count; i++) {
        if (bus->devices[i].valid &&
            bus->devices[i].class_code == class_code &&
            bus->devices[i].subclass_code == subclass) {
            return &bus->devices[i];
        }
    }
    return NULL;
}

void pci_enable_device(pci_device_t* dev)
{
    if (!dev) return;
    uint32_t cmd = pci_config_read(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= PCI_CMD_IO_SPACE | PCI_CMD_MEMORY;
    pci_config_write(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);
    dev->command = (uint16_t)cmd;
}

void pci_disable_device(pci_device_t* dev)
{
    if (!dev) return;
    uint32_t cmd = pci_config_read(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd &= ~(PCI_CMD_IO_SPACE | PCI_CMD_MEMORY);
    pci_config_write(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);
    dev->command = (uint16_t)cmd;
}

void pci_set_master(pci_device_t* dev)
{
    if (!dev) return;
    uint32_t cmd = pci_config_read(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= PCI_CMD_BUS_MASTER;
    pci_config_write(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);
    dev->command = (uint16_t)cmd;
}

void pci_enable_interrupt(pci_device_t* dev)
{
    if (!dev) return;
    uint32_t cmd = pci_config_read(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd &= ~PCI_CMD_INT_DISABLE;
    pci_config_write(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);
}

void pci_disable_interrupt(pci_device_t* dev)
{
    if (!dev) return;
    uint32_t cmd = pci_config_read(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= PCI_CMD_INT_DISABLE;
    pci_config_write(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);
}

uint32_t pci_get_bar_size(pci_device_t* dev, int bar_index)
{
    if (!dev || bar_index < 0 || bar_index >= 6) return 0;

    uint32_t bar_orig = pci_config_read(dev->bus, dev->device, dev->function, PCI_BAR0 + bar_index * 4);
    pci_config_write(dev->bus, dev->device, dev->function, PCI_BAR0 + bar_index * 4, 0xFFFFFFFF);
    uint32_t bar_read = pci_config_read(dev->bus, dev->device, dev->function, PCI_BAR0 + bar_index * 4);
    pci_config_write(dev->bus, dev->device, dev->function, PCI_BAR0 + bar_index * 4, bar_orig);

    if (bar_orig & 1) {
        return (~(bar_read & ~0x3) + 1) & 0xFFFF;
    }
    return ~(bar_read & ~0xF) + 1;
}

void* pci_map_bar(pci_device_t* dev, int bar_index)
{
    if (!dev || bar_index < 0 || bar_index >= 6) return NULL;
    uint32_t bar = dev->bar[bar_index];
    if (bar & 1) return (void*)(uintptr_t)(bar & ~0x3);
    return (void*)(uintptr_t)(bar & ~0xF);
}

int pci_register_driver(pci_bus_t* bus, pci_driver_t* driver)
{
    if (!bus || !driver) return -1;
    spinlock_acquire(&bus->lock);
    if (bus->driver_count >= 256) {
        spinlock_release(&bus->lock);
        return -2;
    }
    bus->drivers[bus->driver_count++] = *driver;
    spinlock_release(&bus->lock);

    for (int i = 0; i < bus->device_count; i++) {
        pci_device_t* dev = &bus->devices[i];
        if (!dev->valid || dev->driver_ctx) continue;
        int match = 0;
        if (driver->match_class) {
            match = (dev->class_code == (driver->class_code & 0xFF) &&
                     dev->subclass_code == (driver->subclass_code & 0xFF));
        } else {
            match = (dev->vendor_id == driver->vendor_id &&
                     dev->device_id == driver->device_id);
        }
        if (match && driver->probe) {
            driver->probe(dev);
        }
    }
    return 0;
}

void pci_unregister_driver(pci_bus_t* bus, pci_driver_t* driver)
{
    if (!bus || !driver) return;
    spinlock_acquire(&bus->lock);
    for (int i = 0; i < bus->driver_count; i++) {
        if (&bus->drivers[i] == driver) {
            bus->drivers[i] = bus->drivers[bus->driver_count - 1];
            bus->driver_count--;
            break;
        }
    }
    spinlock_release(&bus->lock);
}

void pci_irq_handler(int irq, void* ctx)
{
    (void)irq; (void)ctx;
}