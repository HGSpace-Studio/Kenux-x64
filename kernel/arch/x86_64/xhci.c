#include <arch/xhci.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <string.h>

static xhci_controller_t xhci_controllers[XHCI_MAX_CONTROLLERS];
static uint8_t xhci_controller_count = 0;

void xhci_init(void)
{
    memset(xhci_controllers, 0, sizeof(xhci_controllers));
    xhci_controller_count = 0;

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t class_code_reg = pci_read_config(b, d, f, 0x08);
                if (class_code_reg == 0xFFFFFFFF) {
                    continue;
                }
                uint8_t class_code = (class_code_reg >> 24) & 0xFF;
                uint8_t subclass = (class_code_reg >> 16) & 0xFF;
                uint8_t prog_if = (class_code_reg >> 8) & 0xFF;
                if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x30) {
                    if (xhci_controller_count < XHCI_MAX_CONTROLLERS) {
                        xhci_controller_t* ctrl = &xhci_controllers[xhci_controller_count++];
                        ctrl->bus = b;
                        ctrl->device = d;
                        ctrl->function = f;
                        ctrl->base = pci_read_config(b, d, f, 0x10) & 0xFFFFFFF0;
                    }
                }
            }
        }
    }
}

xhci_controller_t* xhci_get_controller(uint8_t index)
{
    if (index < xhci_controller_count) {
        return &xhci_controllers[index];
    }
    return NULL;
}

uint8_t xhci_get_controller_count(void)
{
    return xhci_controller_count;
}

int xhci_port_enabled(uint8_t port)
{
    if (xhci_controller_count == 0) return 0;
    xhci_controller_t* ctrl = &xhci_controllers[0];
    uint8_t caplen = *(volatile uint8_t*)(uintptr_t)ctrl->base;
    uint32_t portsc = *(volatile uint32_t*)(uintptr_t)(ctrl->base + caplen + 0x400 + port * 0x10);
    return (portsc & 0x02) ? 1 : 0;
}

int xhci_device_connected(uint8_t port)
{
    if (xhci_controller_count == 0) return 0;
    xhci_controller_t* ctrl = &xhci_controllers[0];
    uint8_t caplen = *(volatile uint8_t*)(uintptr_t)ctrl->base;
    uint32_t portsc = *(volatile uint32_t*)(uintptr_t)(ctrl->base + caplen + 0x400 + port * 0x10);
    return (portsc & 0x01) ? 1 : 0;
}
