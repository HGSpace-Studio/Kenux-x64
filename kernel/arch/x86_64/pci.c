#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <arch/interrupt.h>
#include <arch/ioapic.h>
#include <arch/acpi_madt.h>
#include <string.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static uint8_t pci_bus_count = 0;
static uint8_t pci_device_count = 0;
static uint8_t pci_function_count = 0;

static uint8_t pci_irq_routes[PCI_IRQ_ROUTES_MAX][4];

void pci_init(void)
{
    pci_bus_count = 0;
    pci_device_count = 0;
    pci_function_count = 0;
    memset(pci_irq_routes, 0, sizeof(pci_irq_routes));
}

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = (1 << 31) |
                      ((uint32_t)bus << 16) |
                      ((uint32_t)device << 11) |
                      ((uint32_t)function << 8) |
                      (offset & 0xFC);

    __asm__ volatile ("movl %0, %%eax\n\t"
                      "movl %%eax, 0xCF8\n\t"
                      "movl 0xCFC, %%eax\n\t"
                      "movl %%eax, %0\n\t"
                      : "=r" (address)
                      : "r" (address)
                      : "eax");

    return address;
}

void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t address = (1 << 31) |
                      ((uint32_t)bus << 16) |
                      ((uint32_t)device << 11) |
                      ((uint32_t)function << 8) |
                      (offset & 0xFC);

    __asm__ volatile ("movl %0, %%eax\n\t"
                      "movl %%eax, 0xCF8\n\t"
                      "movl %1, %%eax\n\t"
                      "movl %%eax, 0xCFC\n\t"
                      :
                      : "r" (address), "r" (value)
                      : "eax");
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = pci_read_config(bus, device, function, offset);
    return (uint8_t)(val >> ((offset & 0x03) * 8));
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = pci_read_config(bus, device, function, offset);
    return (uint16_t)(val >> ((offset & 0x02) * 8));
}

uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    return pci_read_config(bus, device, function, offset);
}

void pci_write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value)
{
    uint8_t shift = (offset & 0x03) * 8;
    uint32_t mask = ~(0xFF << shift);
    uint32_t val = pci_read_config(bus, device, function, offset);
    val = (val & mask) | ((uint32_t)value << shift);
    pci_write_config(bus, device, function, offset, val);
}

void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value)
{
    uint8_t shift = (offset & 0x02) * 8;
    uint32_t mask = ~(0xFFFF << shift);
    uint32_t val = pci_read_config(bus, device, function, offset);
    val = (val & mask) | ((uint32_t)value << shift);
    pci_write_config(bus, device, function, offset, val);
}

void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    pci_write_config(bus, device, function, offset, value);
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t* bus, uint8_t* device, uint8_t* function)
{
    for (int b = 0; b < PCI_MAX_BUSSES; b++) {
        for (int d = 0; d < PCI_MAX_DEVICES; d++) {
            for (int f = 0; f < PCI_MAX_FUNCTIONS; f++) {
                uint32_t class_code_reg = pci_read_config(b, d, f, 0x08);
                if (class_code_reg == 0xFFFFFFFF) {
                    continue;
                }
                uint8_t class_code_current = (class_code_reg >> 24) & 0xFF;
                uint8_t subclass_current = (class_code_reg >> 16) & 0xFF;
                if (class_code_current == class_code && subclass_current == subclass) {
                    if (bus) *bus = b;
                    if (device) *device = d;
                    if (function) *function = f;
                    return 0;
                }
            }
        }
    }
    return -1;
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, uint8_t* bus, uint8_t* device, uint8_t* function)
{
    for (int b = 0; b < PCI_MAX_BUSSES; b++) {
        for (int d = 0; d < PCI_MAX_DEVICES; d++) {
            for (int f = 0; f < PCI_MAX_FUNCTIONS; f++) {
                uint32_t id_reg = pci_read_config(b, d, f, 0x00);
                if (id_reg == 0xFFFFFFFF) {
                    continue;
                }
                uint16_t vid = id_reg & 0xFFFF;
                uint16_t did = (id_reg >> 16) & 0xFFFF;
                if (vid == vendor_id && did == device_id) {
                    if (bus) *bus = b;
                    if (device) *device = d;
                    if (function) *function = f;
                    return 0;
                }
            }
        }
    }
    return -1;
}

uint8_t pci_find_capability(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_id)
{
    uint16_t status = pci_read_config_word(bus, dev, func, 0x06);
    if (!(status & PCI_STATUS_CAP_LIST)) return 0;

    uint8_t header_type = pci_read_config_byte(bus, dev, func, 0x0E);
    uint8_t cap_ptr;

    if ((header_type & PCI_HEADER_TYPE_MASK) == PCI_HEADER_TYPE_PCI_BRIDGE) {
        cap_ptr = pci_read_config_byte(bus, dev, func, 0x34);
    } else if ((header_type & PCI_HEADER_TYPE_MASK) == PCI_HEADER_TYPE_CARDBUS) {
        cap_ptr = pci_read_config_byte(bus, dev, func, 0x14);
    } else {
        cap_ptr = pci_read_config_byte(bus, dev, func, 0x34);
    }

    uint8_t visited[256];
    memset(visited, 0, sizeof(visited));

    while (cap_ptr && cap_ptr != 0xFF) {
        if (cap_ptr < 0x40 || cap_ptr > 0xFC) break;
        if (visited[cap_ptr]) break;
        visited[cap_ptr] = 1;

        if ((cap_ptr & 0x03) != 0) break;

        uint8_t id = pci_read_config_byte(bus, dev, func, cap_ptr);
        if (id == cap_id) {
            return cap_ptr;
        }

        cap_ptr = pci_read_config_byte(bus, dev, func, cap_ptr + 1);
    }

    return 0;
}

uint16_t pci_find_ext_capability(uint8_t bus, uint8_t dev, uint8_t func, uint16_t cap_id)
{
    uint8_t pcie_cap = pci_find_capability(bus, dev, func, PCI_CAP_ID_PCIE);
    if (!pcie_cap) return 0;

    uint16_t dev_ctrl_reg = pci_read_config_word(bus, dev, func, pcie_cap + 0x02);
    uint8_t dev_type = (dev_ctrl_reg >> 4) & 0x0F;

    if (dev_type == PCI_EXP_TYPE_ROOT_PORT ||
        dev_type == PCI_EXP_TYPE_RC_EC) {
        return 0;
    }

    uint32_t ecap_offset = 0x100;

    while (ecap_offset >= 0x100 && ecap_offset <= 0xFFF) {
        uint32_t header = pci_read_config_dword(bus, dev, func, ecap_offset);
        uint16_t ecap_id = header & 0xFFFF;
        uint16_t next = (header >> 20) & 0xFFF;

        if (ecap_id == cap_id) {
            return (uint16_t)ecap_offset;
        }

        if (next == 0) break;
        ecap_offset = next;
    }

    return 0;
}

int pci_enable_msi(uint8_t bus, uint8_t dev, uint8_t func, uint32_t vector)
{
    uint8_t cap_offset = pci_find_capability(bus, dev, func, PCI_CAP_ID_MSI);
    if (!cap_offset) return -1;

    uint16_t msi_ctrl = pci_read_config_word(bus, dev, func, cap_offset + 0x02);

    msi_ctrl |= PCI_MSI_CTRL_ENABLE;

    uint32_t addr_lo = 0xFEE00000 | ((uint8_t)(vector >> 8) << 12);
    pci_write_config_dword(bus, dev, func, cap_offset + 0x04, addr_lo);

    uint32_t data = vector & 0xFF;
    data |= (vector & 0x7F);

    if (msi_ctrl & PCI_MSI_CTRL_64BIT) {
        pci_write_config_dword(bus, dev, func, cap_offset + 0x08, 0);
        pci_write_config_dword(bus, dev, func, cap_offset + 0x0C, data);
    } else {
        pci_write_config_dword(bus, dev, func, cap_offset + 0x08, data);
    }

    uint16_t cmd = pci_read_config_word(bus, dev, func, 0x04);
    cmd |= PCI_COMMAND_INTX_DISABLE;
    pci_write_config_word(bus, dev, func, 0x04, cmd);

    msi_ctrl |= PCI_MSI_CTRL_ENABLE;
    pci_write_config_word(bus, dev, func, cap_offset + 0x02, msi_ctrl);

    return 0;
}

int pci_enable_msix(uint8_t bus, uint8_t dev, uint8_t func, pci_msix_entry_t* entries, uint16_t count)
{
    uint8_t cap_offset = pci_find_capability(bus, dev, func, PCI_CAP_ID_MSIX);
    if (!cap_offset) return -1;
    if (!entries || count == 0) return -1;

    uint16_t msix_ctrl = pci_read_config_word(bus, dev, func, cap_offset + 0x02);
    uint16_t table_size = (msix_ctrl & PCI_MSIX_CTRL_TBL_SIZE_MASK) + 1;

    if (count > table_size) count = table_size;

    uint32_t table_bir_offset = pci_read_config_dword(bus, dev, func, cap_offset + 0x04);
    uint8_t table_bir = table_bir_offset & PCI_MSIX_FLAGS_BIR_MASK;
    uint32_t table_offset = table_bir_offset & PCI_MSIX_FLAGS_OFFSET_MASK;

    uint32_t bar_val = pci_read_config_dword(bus, dev, func, 0x10 + table_bir * 4);
    void* table_base = (void*)(uintptr_t)(bar_val & ~0xF) + table_offset;

    pci_msix_entry_t* msix_table = (pci_msix_entry_t*)table_base;

    for (uint16_t i = 0; i < count; i++) {
        msix_table[i].msg_addr_lo = 0xFEE00000 | ((entries[i].msg_data >> 8) << 12);
        msix_table[i].msg_addr_hi = 0;
        msix_table[i].msg_data = entries[i].msg_data & 0xFF;
        msix_table[i].vector_control = 0;
    }

    msix_ctrl &= ~PCI_MSIX_CTRL_MASKALL;
    msix_ctrl |= PCI_MSIX_CTRL_ENABLE;
    pci_write_config_word(bus, dev, func, cap_offset + 0x02, msix_ctrl);

    uint16_t cmd = pci_read_config_word(bus, dev, func, 0x04);
    cmd |= PCI_COMMAND_INTX_DISABLE;
    pci_write_config_word(bus, dev, func, 0x04, cmd);

    return (int)count;
}

void pci_disable_msix(uint8_t bus, uint8_t dev, uint8_t func)
{
    uint8_t cap_offset = pci_find_capability(bus, dev, func, PCI_CAP_ID_MSIX);
    if (!cap_offset) return;

    uint16_t msix_ctrl = pci_read_config_word(bus, dev, func, cap_offset + 0x02);
    msix_ctrl &= ~PCI_MSIX_CTRL_ENABLE;
    msix_ctrl |= PCI_MSIX_CTRL_MASKALL;
    pci_write_config_word(bus, dev, func, cap_offset + 0x02, msix_ctrl);
}

void pci_set_bus_master(uint8_t bus, uint8_t dev, uint8_t func, int enable)
{
    uint16_t cmd = pci_read_config_word(bus, dev, func, 0x04);

    if (enable) {
        cmd |= PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_IO_SPACE;
    } else {
        cmd &= ~PCI_COMMAND_BUS_MASTER;
    }

    pci_write_config_word(bus, dev, func, 0x04, cmd);
}

int pci_request_irq(uint8_t bus, uint8_t dev, uint8_t func)
{
    uint8_t irq_pin = pci_read_config_byte(bus, dev, func, 0x3D);

    if (irq_pin == 0 || irq_pin > 4) return -1;

    uint8_t slot = dev;
    uint8_t pin_idx = irq_pin - 1;

    uint8_t irq_line = pci_read_config_byte(bus, dev, func, 0x3C);

    if (irq_line != 0 && irq_line != 0xFF) {
        return (int)irq_line;
    }

    uint8_t irq = 0;
    for (int i = 0; i < PCI_IRQ_ROUTES_MAX; i++) {
        if (pci_irq_routes[i][pin_idx] != 0) {
            irq = pci_irq_routes[i][pin_idx];
            break;
        }
    }

    if (irq == 0) {
        static const uint8_t default_irq_map[4] = { 11, 9, 11, 9 };
        irq = default_irq_map[pin_idx];
    }

    pci_write_config_byte(bus, dev, func, 0x3C, irq);

    return (int)irq;
}

uint32_t pci_read_bar(uint8_t bus, uint8_t dev, uint8_t func, int bar_num)
{
    if (bar_num < 0 || bar_num >= PCI_MAX_BARS) return 0;
    return pci_read_config_dword(bus, dev, func, 0x10 + bar_num * 4);
}

uint32_t pci_get_bar_size(uint8_t bus, uint8_t dev, uint8_t func, int bar_num)
{
    if (bar_num < 0 || bar_num >= PCI_MAX_BARS) return 0;
    int bar_offset = 0x10 + bar_num * 4;

    uint32_t orig_val = pci_read_config_dword(bus, dev, func, bar_offset);
    pci_write_config_dword(bus, dev, func, bar_offset, 0xFFFFFFFF);
    uint32_t size_val = pci_read_config_dword(bus, dev, func, bar_offset);
    pci_write_config_dword(bus, dev, func, bar_offset, orig_val);

    if (size_val == 0xFFFFFFFF || size_val == 0) return 0;

    uint32_t size;
    if (size_val & 0x01) {
        size = ~(size_val & 0xFFFFFFFC) + 1;
    } else {
        size = ~(size_val & 0xFFFFFFF0) + 1;
    }

    return size;
}
