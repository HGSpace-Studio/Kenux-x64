#ifndef KAPI_PCI_H
#define KAPI_PCI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAPI_PCI_MAX_BUSES      256
#define KAPI_PCI_MAX_DEVICES    32
#define KAPI_PCI_MAX_FUNCTIONS  8
#define KAPI_PCI_MAX_BARS       6

#define KAPI_PCI_COMMAND_IO          0x0001
#define KAPI_PCI_COMMAND_MEMORY      0x0002
#define KAPI_PCI_COMMAND_MASTER      0x0004
#define KAPI_PCI_COMMAND_INTX_DISABLE 0x0400

#define KAPI_PCI_HEADER_STANDARD    0x00
#define KAPI_PCI_HEADER_BRIDGE      0x01
#define KAPI_PCI_HEADER_CARDBUS     0x02

#define KAPI_PCI_CAP_MSI            0x05
#define KAPI_PCI_CAP_MSIX           0x11
#define KAPI_PCI_CAP_PCIE           0x10
#define KAPI_PCI_CAP_PM             0x01

#define KAPI_PCI_MSI_ENABLE         0x0001
#define KAPI_PCI_MSI_64BIT          0x0080
#define KAPI_PCI_MSI_MASK           0x0100

#define KAPI_PCI_MSIX_ENABLE        0x8000
#define KAPI_PCI_MSIX_MASKALL       0x4000

typedef struct {
    uint8_t  bus;
    uint8_t  dev;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint8_t  irq_line;
    uint8_t  irq_pin;
    uint32_t bar[KAPI_PCI_MAX_BARS];
    uint32_t bar_size[KAPI_PCI_MAX_BARS];
    int      enabled;
} kapi_pci_dev_t;

typedef struct {
    uint32_t msg_addr_lo;
    uint32_t msg_addr_hi;
    uint32_t msg_data;
    uint32_t vector_ctrl;
} kapi_pci_msix_entry_t;

uint32_t kapi_pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint8_t  kapi_pci_read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t kapi_pci_read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint32_t kapi_pci_read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

void kapi_pci_write_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value);
void kapi_pci_write_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t value);
void kapi_pci_write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value);
void kapi_pci_write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value);

int kapi_pci_find_device(uint16_t vendor_id, uint16_t device_id, uint8_t* bus, uint8_t* dev, uint8_t* func);
int kapi_pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t* bus, uint8_t* dev, uint8_t* func);

int kapi_pci_enable_device(kapi_pci_dev_t* dev);
int kapi_pci_disable_device(kapi_pci_dev_t* dev);
int kapi_pci_set_master(kapi_pci_dev_t* dev, int enable);

uint32_t kapi_pci_read_bar(kapi_pci_dev_t* dev, int bar_num);
uint32_t kapi_pci_get_bar_size(kapi_pci_dev_t* dev, int bar_num);

int kapi_pci_enable_msi(kapi_pci_dev_t* dev, uint32_t vector);
int kapi_pci_enable_msix(kapi_pci_dev_t* dev, kapi_pci_msix_entry_t* entries, uint16_t count);
void kapi_pci_disable_msix(kapi_pci_dev_t* dev);

uint8_t kapi_pci_find_capability(kapi_pci_dev_t* dev, uint8_t cap_id);

int kapi_pci_get_device_info(uint8_t bus, uint8_t dev, uint8_t func, kapi_pci_dev_t* info);

int kapi_pci_init(void);

#ifdef __cplusplus
}
#endif

#endif