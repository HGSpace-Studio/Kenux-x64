#ifndef ARCH_X86_64_PCI_H
#define ARCH_X86_64_PCI_H

#include <arch/types.h>

#define PCI_MAX_BUSSES 256
#define PCI_MAX_DEVICES 32
#define PCI_MAX_FUNCTIONS 8
#define PCI_MAX_BARS 6

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_COMMAND_IO_SPACE         0x0001
#define PCI_COMMAND_MEMORY_SPACE     0x0002
#define PCI_COMMAND_BUS_MASTER       0x0004
#define PCI_COMMAND_SPECIAL_CYCLES   0x0008
#define PCI_COMMAND_WRITE_INVLD_EN   0x0010
#define PCI_COMMAND_VGA_PALETTE_SNOOP 0x0020
#define PCI_COMMAND_PARITY_ERR_RESP  0x0040
#define PCI_COMMAND_STEPPING_CTRL    0x0080
#define PCI_COMMAND_SERR_ERR_EN      0x0100
#define PCI_COMMAND_FAST_B2B         0x0200
#define PCI_COMMAND_INTX_DISABLE     0x0400

#define PCI_STATUS_CAP_LIST          0x0010
#define PCI_STATUS_66MHZ            0x0020
#define PCI_STATUS_FAST_B2B          0x0080
#define PCI_STATUS_DEVSEL_MASK       0x0600
#define PCI_STATUS_SIG_TARGET_ABORT  0x0800
#define PCI_STATUS_REC_TARGET_ABORT 0x1000
#define PCI_STATUS_REC_MASTER_ABORT  0x2000
#define PCI_STATUS_SIG_SYSTEM_ERROR  0x4000
#define PCI_STATUS_PARITY_ERR_DETECTED 0x8000

#define PCI_HEADER_TYPE_MASK         0x7F
#define PCI_HEADER_TYPE_MULTIFUNC    0x80

#define PCI_HEADER_TYPE_STANDARD     0x00
#define PCI_HEADER_TYPE_PCI_BRIDGE   0x01
#define PCI_HEADER_TYPE_CARDBUS      0x02

#define PCI_CAP_ID_MSI               0x05
#define PCI_CAP_ID_MSIX              0x11
#define PCI_CAP_ID_PCIE              0x10
#define PCI_CAP_ID_PM                0x01
#define PCI_CAP_ID_AGP               0x02
#define PCI_CAP_ID_VPD               0x03
#define PCI_CAP_ID_SLOTID            0x04
#define PCI_CAP_ID_HOTPLUG           0x06
#define PCI_CAP_ID_SUBSYSTEM_ID      0x0D
#define PCI_CAP_ID_SATA              0x12
#define PCI_CAP_ID_AF                0x13
#define PCI_CAP_ID_EA                0x14
#define PCI_CAP_ID_SATA_NC           0x17

#define PCI_PCIE_CAP_ID_AER          0x01
#define PCI_PCIE_CAP_ID_VC           0x02
#define PCI_PCIE_CAP_ID_DSN          0x03
#define PCI_PCIE_CAP_ID_PWR_BUDGET   0x04
#define PCI_PCIE_CAP_ID_RCLINK       0x06
#define PCI_PCIE_CAP_ID_LTR          0x18
#define PCI_PCIE_CAP_ID_DPC          0x1D
#define PCI_PCIE_CAP_ID_L1PM         0x24
#define PCI_PCIE_CAP_ID_PTMR         0x1F
#define PCI_PCIE_CAP_ID_DVSEC        0x23

#define PCI_MSI_CTRL_ENABLE          0x0001
#define PCI_MSI_CTRL_MULTIPLIER_MASK 0x000E
#define PCI_MSI_CTRL_64BIT           0x0080
#define PCI_MSI_CTRL_MASKBIT         0x0100

#define PCI_MSIX_CTRL_ENABLE         0x8000
#define PCI_MSIX_CTRL_MASKALL        0x4000
#define PCI_MSIX_CTRL_TBL_SIZE_MASK  0x07FF
#define PCI_MSIX_FLAGS_BIR_MASK      0x0007
#define PCI_MSIX_FLAGS_OFFSET_MASK   0xFFF8

#define PCI_MSIX_ENTRY_CTRL_MASKBIT  0x0001

#define PCI_IRQ_ROUTES_MAX           16

#define PCI_EXP_TYPE_ENDPOINT         0x00
#define PCI_EXP_TYPE_LEGACY_ENDPOINT  0x01
#define PCI_EXP_TYPE_ROOT_PORT        0x04
#define PCI_EXP_TYPE_UPSTREAM         0x05
#define PCI_EXP_TYPE_DOWNSTREAM       0x06
#define PCI_EXP_TYPE_PCI_BRIDGE       0x07
#define PCI_EXP_TYPE_RC_EC            0x09

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t class_code[3];
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
} __attribute__((packed)) pci_header0_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    uint8_t latency_timer;
    uint8_t io_base;
    uint8_t io_limit;
    uint16_t secondary_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint16_t prefetchable_memory_base;
    uint16_t prefetchable_memory_limit;
    uint32_t prefetchable_base_upper;
    uint32_t prefetchable_limit_upper;
    uint16_t io_base_upper;
    uint16_t io_limit_upper;
    uint8_t capabilities_ptr;
    uint8_t reserved[3];
    uint32_t rom_base;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint16_t min_gnt;
    uint16_t max_lat;
} __attribute__((packed)) pci_header1_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    uint8_t latency_timer;
    uint8_t io_base;
    uint8_t io_limit;
    uint16_t secondary_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint16_t prefetchable_memory_base;
    uint16_t prefetchable_memory_limit;
    uint32_t prefetchable_base_upper;
    uint32_t prefetchable_limit_upper;
    uint16_t io_base_upper;
    uint16_t io_limit_upper;
    uint8_t capabilities_ptr;
    uint8_t reserved[3];
    uint32_t rom_base;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint16_t min_gnt;
    uint16_t max_lat;
} __attribute__((packed)) pci_header2_t;

typedef struct {
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t irq_line;
    uint8_t irq_pin;
} pci_device_t;

typedef struct {
    uint32_t msg_addr_lo;
    uint32_t msg_addr_hi;
    uint32_t msg_data;
    uint32_t vector_control;
} __attribute__((packed)) pci_msix_entry_t;

void pci_init(void);
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);
void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

int pci_find_device(uint16_t vendor_id, uint16_t device_id, uint8_t* bus, uint8_t* device, uint8_t* function);
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t* bus, uint8_t* device, uint8_t* function);

uint8_t pci_find_capability(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_id);
uint16_t pci_find_ext_capability(uint8_t bus, uint8_t dev, uint8_t func, uint16_t cap_id);

int pci_enable_msi(uint8_t bus, uint8_t dev, uint8_t func, uint32_t vector);
int pci_enable_msix(uint8_t bus, uint8_t dev, uint8_t func, pci_msix_entry_t* entries, uint16_t count);
void pci_disable_msix(uint8_t bus, uint8_t dev, uint8_t func);

void pci_set_bus_master(uint8_t bus, uint8_t dev, uint8_t func, int enable);
int pci_request_irq(uint8_t bus, uint8_t dev, uint8_t func);

uint32_t pci_read_bar(uint8_t bus, uint8_t dev, uint8_t func, int bar_num);
uint32_t pci_get_bar_size(uint8_t bus, uint8_t dev, uint8_t func, int bar_num);

#endif
