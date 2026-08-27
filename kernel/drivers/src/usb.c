#include "usb.h"
#include <arch/memory.h>
#include <string.h>

int usb_control_msg(usb_device_t* dev, uint8_t request_type, uint8_t request,
                     uint16_t value, uint16_t index, void* data, uint16_t length)
{
    if (!dev) return -1;
    usb_hc_ops_t* ops = (usb_hc_ops_t*)dev->hc_priv;
    if (!ops || !ops->control) return -2;
    return ops->control(dev, request_type, request, value, index, data, length);
}

int usb_get_descriptor(usb_device_t* dev, uint8_t type, uint8_t index,
                        uint16_t lang, void* buf, uint16_t size)
{
    if (!dev || !buf) return -1;
    return usb_control_msg(dev, 0x80, USB_REQ_GET_DESCRIPTOR,
                           (uint16_t)((type << 8) | index), lang, buf, size);
}

int usb_set_address(usb_device_t* dev, uint8_t addr)
{
    if (!dev) return -1;
    int result = usb_control_msg(dev, 0x00, USB_REQ_SET_ADDRESS, addr, 0, NULL, 0);
    if (result == 0) dev->address = addr;
    return result;
}

int usb_set_configuration(usb_device_t* dev, uint8_t config)
{
    if (!dev) return -1;
    int result = usb_control_msg(dev, 0x00, USB_REQ_SET_CONFIG, config, 0, NULL, 0);
    if (result == 0) dev->configured = 1;
    return result;
}

int usb_clear_feature(usb_device_t* dev, uint8_t feature, uint16_t index)
{
    if (!dev) return -1;
    return usb_control_msg(dev, 0x02, USB_REQ_CLEAR_FEATURE, feature, index, NULL, 0);
}

int usb_set_feature(usb_device_t* dev, uint8_t feature, uint16_t index)
{
    if (!dev) return -1;
    return usb_control_msg(dev, 0x02, USB_REQ_SET_FEATURE, feature, index, NULL, 0);
}

int usb_bulk_transfer(usb_device_t* dev, int endpoint, void* data, uint32_t length, int direction)
{
    if (!dev || !data) return -1;
    usb_hc_ops_t* ops = (usb_hc_ops_t*)dev->hc_priv;
    if (!ops || !ops->bulk) return -2;
    return ops->bulk(dev, endpoint, data, length, direction);
}

int usb_interrupt_transfer(usb_device_t* dev, int endpoint, void* data, uint32_t length)
{
    if (!dev || !data) return -1;
    usb_hc_ops_t* ops = (usb_hc_ops_t*)dev->hc_priv;
    if (!ops || !ops->interrupt) return -2;
    return ops->interrupt(dev, endpoint, data, length);
}

static int xhci_control(usb_device_t* dev, uint8_t request_type, uint8_t request,
                         uint16_t value, uint16_t index, void* data, uint16_t length)
{
    (void)dev; (void)request_type; (void)request;
    (void)value; (void)index; (void)data; (void)length;
    return -1;
}

static int xhci_bulk(usb_device_t* dev, int endpoint, void* data, uint32_t length, int direction)
{
    (void)dev; (void)endpoint; (void)data; (void)length; (void)direction;
    return -1;
}

static int xhci_interrupt_transfer(usb_device_t* dev, int endpoint, void* data, uint32_t length)
{
    (void)dev; (void)endpoint; (void)data; (void)length;
    return -1;
}

static usb_hc_ops_t xhci_ops = {
    .control = xhci_control,
    .bulk = xhci_bulk,
    .interrupt = xhci_interrupt_transfer,
    .isochronous = NULL,
};

static void xhci_ring_init(xhci_ring_t* ring, uint32_t size)
{
    if (!ring) return;
    memset(ring, 0, sizeof(xhci_ring_t));
    spin_init(&ring->lock);
    ring->ring_size = size;
    ring->trbs = (xhci_trb_t*)memory_alloc_aligned(size * sizeof(xhci_trb_t), 4096);
    ring->phys = (uint64_t)(uintptr_t)ring->trbs;
    ring->cycle = 1;
    ring->enqueue_idx = 0;
    ring->dequeue_idx = 0;
}

int xhci_reset(xhci_hc_t* hc)
{
    if (!hc) return -1;
    volatile uint32_t* cmd = (volatile uint32_t*)(hc->op_regs + XHCI_USBCMD);
    *cmd |= XHCI_CMD_HCRST;
    for (int i = 0; i < 1000000; i++) {
        if (!(*cmd & XHCI_CMD_HCRST)) return 0;
    }
    return -2;
}

int xhci_stop(xhci_hc_t* hc)
{
    if (!hc) return -1;
    volatile uint32_t* cmd = (volatile uint32_t*)(hc->op_regs + XHCI_USBCMD);
    *cmd &= ~XHCI_CMD_RUN;
    volatile uint32_t* sts = (volatile uint32_t*)(hc->op_regs + XHCI_USBSTS);
    for (int i = 0; i < 1000000; i++) {
        if (*sts & XHCI_STS_HCH) return 0;
    }
    return -2;
}

int xhci_start(xhci_hc_t* hc)
{
    if (!hc) return -1;
    volatile uint32_t* cmd = (volatile uint32_t*)(hc->op_regs + XHCI_USBCMD);
    *cmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    volatile uint32_t* sts = (volatile uint32_t*)(hc->op_regs + XHCI_USBSTS);
    for (int i = 0; i < 1000000; i++) {
        if (!(*sts & XHCI_STS_HCH)) return 0;
    }
    return -2;
}

int xhci_init(xhci_hc_t* hc, pci_device_t* pci_dev)
{
    if (!hc || !pci_dev) return -1;

    memset(hc, 0, sizeof(xhci_hc_t));
    spin_init(&hc->lock);
    hc->pci_dev = pci_dev;

    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    hc->mmio = (volatile uint8_t*)pci_map_bar(pci_dev, 0);
    if (!hc->mmio) return -2;

    hc->cap_length = hc->mmio[XHCI_CAPLENGTH];
    hc->hci_version = *(volatile uint16_t*)(hc->mmio + XHCI_HCIVERSION);
    hc->hcs_params1 = *(volatile uint32_t*)(hc->mmio + XHCI_HCSPARAMS1);
    hc->hcs_params2 = *(volatile uint32_t*)(hc->mmio + XHCI_HCSPARAMS2);
    hc->hcs_params3 = *(volatile uint32_t*)(hc->mmio + XHCI_HCSPARAMS3);
    hc->hcc_params1 = *(volatile uint32_t*)(hc->mmio + XHCI_HCCPARAMS1);
    hc->dboff = *(volatile uint32_t*)(hc->mmio + XHCI_DBOFF);
    hc->hcc_params2 = *(volatile uint32_t*)(hc->mmio + XHCI_HCCPARAMS2);

    hc->max_slots = (int)(hc->hcs_params1 & 0xFF);
    hc->max_ports = (int)((hc->hcs_params1 >> 24) & 0xFF);
    hc->max_intrs = (int)((hc->hcs_params1 >> 8) & 0x1FF);
    hc->max_scratchpad = (int)(((hc->hcs_params2 >> 16) & 0x1F) * ((hc->hcs_params2 >> 27) & 0x1F));
    hc->context_size = (hc->hcc_params1 & 0x04) ? 64 : 32;

    hc->op_regs = (volatile uint32_t*)(hc->mmio + hc->cap_length);
    hc->doorbells = (volatile uint32_t*)(hc->mmio + hc->dboff);

    xhci_stop(hc);
    int result = xhci_reset(hc);
    if (result != 0) return -3;

    xhci_ring_init(&hc->cmd_ring, 256);

    volatile uint32_t* crcr = (volatile uint32_t*)(hc->op_regs + XHCI_CRCR_LOW / 4);
    crcr[0] = (uint32_t)hc->cmd_ring.phys | hc->cmd_ring.cycle;
    crcr[1] = (uint32_t)(hc->cmd_ring.phys >> 32);

    hc->dcbaa = (xhci_slot_ctx_entry_t*)memory_alloc_aligned(
        (hc->max_slots + 1) * sizeof(xhci_slot_ctx_entry_t), 4096);
    if (!hc->dcbaa) return -4;
    memset(hc->dcbaa, 0, (hc->max_slots + 1) * sizeof(xhci_slot_ctx_entry_t));

    volatile uint32_t* dcbaap = (volatile uint32_t*)(hc->op_regs + XHCI_DCBAAP_LOW / 4);
    dcbaap[0] = (uint32_t)(uintptr_t)hc->dcbaa;
    dcbaap[1] = (uint32_t)((uintptr_t)hc->dcbaa >> 32);

    volatile uint32_t* config = (volatile uint32_t*)(hc->op_regs + XHCI_CONFIG / 4);
    *config = (uint32_t)hc->max_slots;

    result = xhci_start(hc);
    if (result != 0) return -5;

    return 0;
}

void xhci_shutdown(xhci_hc_t* hc)
{
    if (!hc) return;
    xhci_stop(hc);
}

void xhci_irq_handler(int irq, void* ctx)
{
    xhci_hc_t* hc = (xhci_hc_t*)ctx;
    if (!hc) return;
    volatile uint32_t* sts = (volatile uint32_t*)(hc->op_regs + XHCI_USBSTS);
    *sts |= XHCI_STS_EINT | XHCI_STS_PCD;
    (void)irq;
}