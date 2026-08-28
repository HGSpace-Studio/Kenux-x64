#include "usb_ohci_ehci_uhci.h"
#include <arch/io.h>
#include <arch/memory.h>
#include <string.h>

int uhci_init(uhci_hc_t* hc, pci_device_t* pci_dev)
{
    if (!hc || !pci_dev) return -1;
    memset(hc, 0, sizeof(uhci_hc_t));
    spin_init(&hc->lock);
    hc->pci_dev = pci_dev;

    pci_enable_device(pci_dev);
    hc->io_base = (uint16_t)pci_dev->bar[4];
    if (hc->io_base == 0) return -2;

    int ret = uhci_reset(hc);
    if (ret != 0) return -3;

    hc->frame_list = (uint32_t*)memory_alloc_aligned(1024 * sizeof(uint32_t), 4096);
    if (!hc->frame_list) return -4;
    hc->frame_list_phys = (uint32_t)(uintptr_t)hc->frame_list;
    for (int i = 0; i < 1024; i++) hc->frame_list[i] = 1;

    hc->qh_pool = (uhci_qh_t*)memory_alloc_aligned(64 * sizeof(uhci_qh_t), 16);
    hc->td_pool = (uhci_td_t*)memory_alloc_aligned(128 * sizeof(uhci_td_t), 16);
    if (!hc->qh_pool || !hc->td_pool) return -5;
    hc->qh_count = 64;
    hc->td_count = 128;

    outw(hc->io_base + UHCI_FLBASE, hc->frame_list_phys);
    outw(hc->io_base + UHCI_SOF, 0x40);

    hc->port_count = 2;
    return 0;
}

void uhci_shutdown(uhci_hc_t* hc)
{
    if (!hc) return;
    uhci_stop(hc);
}

int uhci_reset(uhci_hc_t* hc)
{
    if (!hc) return -1;
    outw(hc->io_base + UHCI_CMD, UHCI_CMD_HCRESET);
    for (int i = 0; i < 1000000; i++) {
        if (!(inw(hc->io_base + UHCI_CMD) & UHCI_CMD_HCRESET)) return 0;
    }
    return -2;
}

int uhci_start(uhci_hc_t* hc)
{
    if (!hc) return -1;
    outw(hc->io_base + UHCI_CMD, UHCI_CMD_RS | UHCI_CMD_EGSM | UHCI_CMD_FGR);
    for (int i = 0; i < 1000000; i++) {
        if (!(inw(hc->io_base + UHCI_STS) & UHCI_STS_HCHALTED)) {
            hc->running = 1;
            return 0;
        }
    }
    return -2;
}

int uhci_stop(uhci_hc_t* hc)
{
    if (!hc) return -1;
    outw(hc->io_base + UHCI_CMD, 0);
    for (int i = 0; i < 1000000; i++) {
        if (inw(hc->io_base + UHCI_STS) & UHCI_STS_HCHALTED) {
            hc->running = 0;
            return 0;
        }
    }
    return -2;
}

void uhci_irq_handler(int irq, void* ctx)
{
    uhci_hc_t* hc = (uhci_hc_t*)ctx;
    if (!hc) return;
    uint16_t status = inw(hc->io_base + UHCI_STS);
    outw(hc->io_base + UHCI_STS, status);
    (void)irq;
}

int ohci_init(ohci_hc_t* hc, pci_device_t* pci_dev)
{
    if (!hc || !pci_dev) return -1;
    memset(hc, 0, sizeof(ohci_hc_t));
    spin_init(&hc->lock);
    hc->pci_dev = pci_dev;

    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    hc->mmio = (volatile uint32_t*)pci_map_bar(pci_dev, 0);
    if (!hc->mmio) return -2;

    hc->hcca = (ohci_hcca_t*)memory_alloc_aligned(sizeof(ohci_hcca_t), 256);
    if (!hc->hcca) return -3;
    hc->hcca_phys = (uint32_t)(uintptr_t)hc->hcca;
    memset(hc->hcca, 0, sizeof(ohci_hcca_t));

    int ret = ohci_reset(hc);
    if (ret != 0) return -4;

    uint32_t rha = hc->mmio[OHCI_RH_DESC_A / 4];
    hc->port_count = (int)(rha & 0xFF);

    return 0;
}

void ohci_shutdown(ohci_hc_t* hc)
{
    if (!hc) return;
    ohci_stop(hc);
}

int ohci_reset(ohci_hc_t* hc)
{
    if (!hc) return -1;
    hc->mmio[OHCI_CONTROL / 4] = OHCI_CTRL_HCFS_RESET;
    for (int i = 0; i < 1000000; i++) {
        uint32_t ctrl = hc->mmio[OHCI_CONTROL / 4];
        if ((ctrl & OHCI_CTRL_HCFS) == OHCI_CTRL_HCFS_RESET) break;
    }
    hc->mmio[OHCI_HCCA / 4] = hc->hcca_phys;
    hc->mmio[OHCI_INT_DISABLE / 4] = 0xFFFFFFFF;
    hc->mmio[OHCI_CONTROL / 4] = OHCI_CTRL_HCFS_SUSP;
    return 0;
}

int ohci_start(ohci_hc_t* hc)
{
    if (!hc) return -1;
    hc->mmio[OHCI_CONTROL / 4] = OHCI_CTRL_HCFS_OPER | OHCI_CTRL_PLE | OHCI_CTRL_CLE | OHCI_CTRL_BLE;
    for (int i = 0; i < 1000000; i++) {
        uint32_t ctrl = hc->mmio[OHCI_CONTROL / 4];
        if ((ctrl & OHCI_CTRL_HCFS) == OHCI_CTRL_HCFS_OPER) {
            hc->running = 1;
            return 0;
        }
    }
    return -2;
}

int ohci_stop(ohci_hc_t* hc)
{
    if (!hc) return -1;
    hc->mmio[OHCI_CONTROL / 4] = OHCI_CTRL_HCFS_SUSP;
    hc->running = 0;
    return 0;
}

void ohci_irq_handler(int irq, void* ctx)
{
    ohci_hc_t* hc = (ohci_hc_t*)ctx;
    if (!hc) return;
    uint32_t status = hc->mmio[OHCI_INT_STATUS / 4];
    hc->mmio[OHCI_INT_STATUS / 4] = status;
    (void)irq;
}

int ehci_init(ehci_hc_t* hc, pci_device_t* pci_dev)
{
    if (!hc || !pci_dev) return -1;
    memset(hc, 0, sizeof(ehci_hc_t));
    spin_init(&hc->lock);
    hc->pci_dev = pci_dev;

    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    hc->mmio = (volatile uint8_t*)pci_map_bar(pci_dev, 0);
    if (!hc->mmio) return -2;

    hc->cap_length = hc->mmio[EHCI_CAPLENGTH];
    hc->hci_version = *(volatile uint16_t*)(hc->mmio + EHCI_HCIVERSION);
    hc->hcs_params = *(volatile uint32_t*)(hc->mmio + EHCI_HCSPARAMS);
    hc->hcc_params = *(volatile uint32_t*)(hc->mmio + EHCI_HCCPARAMS);

    hc->op_regs = (volatile uint32_t*)(hc->mmio + hc->cap_length);
    hc->port_count = (int)((hc->hcs_params >> 0) & 0xF);
    hc->companion_count = (int)((hc->hcs_params >> 8) & 0xF);

    int ret = ehci_reset(hc);
    if (ret != 0) return -3;

    hc->periodic_list = (uint32_t*)memory_alloc_aligned(1024 * sizeof(uint32_t), 4096);
    if (!hc->periodic_list) return -4;
    hc->periodic_list_phys = (uint32_t)(uintptr_t)hc->periodic_list;
    for (int i = 0; i < 1024; i++) hc->periodic_list[i] = 1;

    hc->async_qh = (ehci_qh_t*)memory_alloc_aligned(sizeof(ehci_qh_t), 32);
    if (!hc->async_qh) return -5;
    hc->async_qh_phys = (uint32_t)(uintptr_t)hc->async_qh;
    memset(hc->async_qh, 0, sizeof(ehci_qh_t));
    hc->async_qh->link = hc->async_qh_phys | 0x02;
    hc->async_qh->charac = 0x00000000;

    hc->op_regs[EHCI_PERIODICLIST / 4] = hc->periodic_list_phys;
    hc->op_regs[EHCI_ASYNCLIST / 4] = hc->async_qh_phys;

    return 0;
}

void ehci_shutdown(ehci_hc_t* hc)
{
    if (!hc) return;
    ehci_stop(hc);
}

int ehci_reset(ehci_hc_t* hc)
{
    if (!hc) return -1;
    hc->op_regs[EHCI_USBCMD / 4] = EHCI_CMD_HCRESET;
    for (int i = 0; i < 1000000; i++) {
        if (!(hc->op_regs[EHCI_USBCMD / 4] & EHCI_CMD_HCRESET)) return 0;
    }
    return -2;
}

int ehci_start(ehci_hc_t* hc)
{
    if (!hc) return -1;
    hc->op_regs[EHCI_CONFIGFLAG / 4] = 1;
    hc->op_regs[EHCI_USBCMD / 4] = EHCI_CMD_RUN | EHCI_CMD_ASE | EHCI_CMD_PSE | EHCI_CMD_ITP;
    for (int i = 0; i < 1000000; i++) {
        uint32_t sts = hc->op_regs[EHCI_USBSTS / 4];
        if (!(sts & EHCI_STS_HCHALTED)) {
            hc->running = 1;
            return 0;
        }
    }
    return -2;
}

int ehci_stop(ehci_hc_t* hc)
{
    if (!hc) return -1;
    hc->op_regs[EHCI_USBCMD / 4] = 0;
    for (int i = 0; i < 1000000; i++) {
        if (hc->op_regs[EHCI_USBSTS / 4] & EHCI_STS_HCHALTED) {
            hc->running = 0;
            return 0;
        }
    }
    return -2;
}

void ehci_irq_handler(int irq, void* ctx)
{
    ehci_hc_t* hc = (ehci_hc_t*)ctx;
    if (!hc) return;
    uint32_t sts = hc->op_regs[EHCI_USBSTS / 4];
    hc->op_regs[EHCI_USBSTS / 4] = sts;
    (void)irq;
}

int pcie_init(pcie_bus_t* bus, uint64_t cfg_base, uint32_t cfg_size, int start_bus, int end_bus)
{
    if (!bus) return -1;
    memset(bus, 0, sizeof(pcie_bus_t));
    spin_init(&bus->lock);
    bus->cfg_base = (volatile uint32_t*)(uintptr_t)cfg_base;
    bus->cfg_size = cfg_size;
    bus->start_bus = start_bus;
    bus->end_bus = end_bus;
    return 0;
}

uint32_t pcie_config_read(pcie_bus_t* bus, uint8_t bus_num, uint8_t dev, uint8_t func, uint16_t offset)
{
    if (!bus || !bus->cfg_base) return 0xFFFFFFFF;
    uint32_t addr = ((uint32_t)bus_num << PCIE_BUS_SHIFT) |
                    ((uint32_t)dev << PCIE_DEV_SHIFT) |
                    ((uint32_t)func << PCIE_FUNC_SHIFT) |
                    (offset & 0xFFC);
    if (addr >= bus->cfg_size) return 0xFFFFFFFF;
    return bus->cfg_base[addr / 4];
}

void pcie_config_write(pcie_bus_t* bus, uint8_t bus_num, uint8_t dev, uint8_t func, uint16_t offset, uint32_t value)
{
    if (!bus || !bus->cfg_base) return;
    uint32_t addr = ((uint32_t)bus_num << PCIE_BUS_SHIFT) |
                    ((uint32_t)dev << PCIE_DEV_SHIFT) |
                    ((uint32_t)func << PCIE_FUNC_SHIFT) |
                    (offset & 0xFFC);
    if (addr >= bus->cfg_size) return;
    bus->cfg_base[addr / 4] = value;
}

int pcie_find_capability(pcie_bus_t* bus, uint8_t bus_num, uint8_t dev, uint8_t func, uint8_t cap_id)
{
    if (!bus) return -1;
    uint32_t caps = pcie_config_read(bus, bus_num, dev, func, 0x34);
    uint8_t ptr = (uint8_t)(caps & 0xFF);

    while (ptr != 0 && ptr != 0xFF) {
        uint32_t cap = pcie_config_read(bus, bus_num, dev, func, ptr);
        if ((cap & 0xFF) == cap_id) return ptr;
        ptr = (uint8_t)((cap >> 8) & 0xFF);
    }
    return -2;
}