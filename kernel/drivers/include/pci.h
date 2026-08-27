#ifndef KERNEL_DRIVERS_PCI_H
#define KERNEL_DRIVERS_PCI_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_CLASS_CODE      0x09
#define PCI_SUBCLASS_CODE   0x0A
#define PCI_PROG_IF         0x0B
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

#define PCI_CMD_IO_SPACE    0x0001
#define PCI_CMD_MEMORY      0x0002
#define PCI_CMD_BUS_MASTER  0x0004
#define PCI_CMD_SPECIAL     0x0008
#define PCI_CMD_MWI         0x0010
#define PCI_CMD_VGA         0x0020
#define PCI_CMD_PARITY      0x0040
#define PCI_CMD_SERR        0x0080
#define PCI_CMD_FAST_B2B    0x0100
#define PCI_CMD_INT_DISABLE 0x0400

#define PCI_HEADER_NORMAL   0
#define PCI_HEADER_BRIDGE   1
#define PCI_HEADER_CARDBUS  2

#define PCI_CLASS_STORAGE       0x01
#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_MULTIMEDIA    0x04
#define PCI_CLASS_MEMORY        0x05
#define PCI_CLASS_BRIDGE        0x06
#define PCI_CLASS_SERIAL        0x0C
#define PCI_CLASS_USB           0x0C

#define PCI_MAX_BUSES      256
#define PCI_MAX_DEVICES    32
#define PCI_MAX_FUNCTIONS  8
#define PCI_MAX_DEVICES_TOTAL 4096

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass_code;
    uint8_t  class_code;
    uint8_t  header_type;
    uint8_t  interrupt_pin;
    uint8_t  interrupt_line;
    uint32_t bar[6];
    uint32_t bar_size[6];
    int      bar_is_io[6];
    int      bar_is_64[6];
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    int      valid;
    void*    driver_ctx;
    spinlock_t lock;
} pci_device_t;

typedef struct {
    int (*probe)(pci_device_t* dev);
    void (*remove)(pci_device_t* dev);
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t class_code;
    uint16_t subclass_code;
    int      match_class;
} pci_driver_t;

typedef struct {
    pci_device_t devices[PCI_MAX_DEVICES_TOTAL];
    int          device_count;
    pci_driver_t drivers[256];
    int          driver_count;
    uint8_t      buses_scanned[PCI_MAX_BUSES];
    spinlock_t   lock;
} pci_bus_t;

void     pci_init(pci_bus_t* bus);
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
void     pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value);
void     pci_scan_bus(pci_bus_t* bus);
void     pci_scan_slot(pci_bus_t* bus, uint8_t bus_num, uint8_t device);
void     pci_scan_function(pci_bus_t* bus, uint8_t bus_num, uint8_t device, uint8_t func);
pci_device_t* pci_find_device(pci_bus_t* bus, uint16_t vendor, uint16_t device_id);
pci_device_t* pci_find_class(pci_bus_t* bus, uint8_t class_code, uint8_t subclass);
void     pci_enable_device(pci_device_t* dev);
void     pci_disable_device(pci_device_t* dev);
void     pci_set_master(pci_device_t* dev);
void     pci_enable_interrupt(pci_device_t* dev);
void     pci_disable_interrupt(pci_device_t* dev);
uint32_t pci_read_bar(pci_device_t* dev, int bar_index);
uint32_t pci_get_bar_size(pci_device_t* dev, int bar_index);
void*    pci_map_bar(pci_device_t* dev, int bar_index);
int      pci_register_driver(pci_bus_t* bus, pci_driver_t* driver);
void     pci_unregister_driver(pci_bus_t* bus, pci_driver_t* driver);
void     pci_irq_handler(int irq, void* ctx);

#endif