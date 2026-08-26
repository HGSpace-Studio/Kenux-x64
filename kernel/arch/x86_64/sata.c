#include <arch/sata.h>
#include <arch/ahci.h>
#include <arch/memory.h>
#include <string.h>

static sata_device_t sata_devices[SATA_MAX_DEVICES];
static uint8_t sata_device_count = 0;

void sata_init(void)
{
    memset(sata_devices, 0, sizeof(sata_devices));
    sata_device_count = 0;

    for (uint8_t i = 0; i < ahci_get_controller_count(); i++) {
        ahci_controller_t* ctrl = ahci_get_controller(i);
        for (uint8_t port = 0; port < AHCI_MAX_PORTS; port++) {
            if (ctrl->ports[port] && ctrl->ports[port]->cmd_sts & 0x1) {
                if (sata_device_count < SATA_MAX_DEVICES) {
                    sata_device_t* dev = &sata_devices[sata_device_count++];
                    dev->port = port;
                    dev->device = 0;
                    dev->capacity = 0;
                    dev->sector_size = 512;
                }
            }
        }
    }
}

sata_device_t* sata_get_device(uint8_t port, uint8_t device)
{
    for (uint8_t i = 0; i < sata_device_count; i++) {
        if (sata_devices[i].port == port && sata_devices[i].device == device) {
            return &sata_devices[i];
        }
    }
    return NULL;
}

int sata_read(uint8_t port, uint8_t device, uint64_t lba, void* buffer, uint32_t sectors)
{
    sata_device_t* dev = sata_get_device(port, device);
    if (!dev) {
        return -1;
    }

    return ahci_read_sector(port, lba, buffer);
}

int sata_write(uint8_t port, uint8_t device, uint64_t lba, const void* buffer, uint32_t sectors)
{
    sata_device_t* dev = sata_get_device(port, device);
    if (!dev) {
        return -1;
    }

    return ahci_write_sector(port, lba, buffer);
}
