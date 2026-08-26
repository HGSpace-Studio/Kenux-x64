#ifndef ARCH_X86_64_AHCI_H
#define ARCH_X86_64_AHCI_H

#include <arch/types.h>

#define AHCI_MAX_CONTROLLERS 4
#define AHCI_MAX_PORTS 32
#define AHCI_MAX_CMD_SLOTS 32

typedef struct {
    uint32_t cmd_list_base;
    uint32_t cmd_list_base_upper;
    uint32_t fis_base;
    uint32_t fis_base_upper;
    uint32_t interrupt_status;
    uint32_t interrupt_enable;
    uint32_t cmd_sts;
    uint32_t reserved;
    uint32_t cmd_issue;
    uint32_t sata_notification;
    uint32_t fis_control;
} __attribute__((packed)) ahci_port_t;

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t base;
    uint32_t capabilities;
    uint32_t global_host_control;
    uint32_t interrupt_status;
    uint32_t ports_implemented;
    ahci_port_t* ports[AHCI_MAX_PORTS];
} __attribute__((packed)) ahci_controller_t;

void ahci_init(void);
ahci_controller_t* ahci_get_controller(uint8_t index);
uint8_t ahci_get_controller_count(void);
int ahci_read_sector(uint8_t port, uint64_t lba, void* buffer);
int ahci_write_sector(uint8_t port, uint64_t lba, const void* buffer);

#endif
