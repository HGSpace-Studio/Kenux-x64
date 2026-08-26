#ifndef ARCH_X86_64_EHCI_H
#define ARCH_X86_64_EHCI_H

#include <arch/types.h>

#define EHCI_MAX_CONTROLLERS 4
#define EHCI_MAX_HCS 4
#define EHCI_MAX_PORTS 15

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t base;
    uint32_t capability_registers;
    uint32_t operational_registers;
} __attribute__((packed)) ehci_controller_t;

void ehci_init(void);
ehci_controller_t* ehci_get_controller(uint8_t index);
uint8_t ehci_get_controller_count(void);
int ehci_port_enabled(uint8_t port);
int ehci_device_connected(uint8_t port);

#endif
