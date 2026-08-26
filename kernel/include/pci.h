#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

#include <arch/types.h>

void pci_init(void);

#ifdef __cplusplus
extern "C" {
#endif

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
