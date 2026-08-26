#ifndef ARCH_X86_64_XHCI_H
#define ARCH_X86_64_XHCI_H

#include <arch/types.h>

#define XHCI_MAX_CONTROLLERS 4
#define XHCI_MAX_PORTS 255
#define XHCI_MAX_SLOTS 255

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t base;
    uint64_t capability_registers;
    uint64_t operational_registers;
} __attribute__((packed)) xhci_controller_t;

void xhci_init(void);
xhci_controller_t* xhci_get_controller(uint8_t index);
uint8_t xhci_get_controller_count(void);
int xhci_port_enabled(uint8_t port);
int xhci_device_connected(uint8_t port);

#endif
