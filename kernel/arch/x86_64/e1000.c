#include <arch/e1000.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <string.h>

#define E1000_MAX_CONTROLLERS 4

static e1000_t e1000_devices[E1000_MAX_CONTROLLERS];
static uint8_t e1000_count = 0;

static inline uint32_t e1000_read32(e1000_t* dev, uint32_t reg)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->mmio_base + reg);
    return *ptr;
}

static inline void e1000_write32(e1000_t* dev, uint32_t reg, uint32_t val)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->mmio_base + reg);
    *ptr = val;
}

static int e1000_match_device(uint16_t vendor, uint16_t device)
{
    if (vendor != E1000_VENDOR_ID) return 0;
    switch (device) {
    case E1000_DEV_82540EM:
    case E1000_DEV_82543GC:
    case E1000_DEV_82545EM:
    case E1000_DEV_82545GM:
    case E1000_DEV_82546EB:
    case E1000_DEV_82547GI:
    case E1000_DEV_82541ER:
    case E1000_DEV_82541GI:
    case E1000_DEV_82574L:
    case E1000_DEV_82577LM:
    case E1000_DEV_82579V:
    case E1000_DEV_I217_LM:
    case E1000_DEV_I217_V:
    case E1000_DEV_I219_V:
    case E1000_DEV_I219_LM:
        return 1;
    default:
        return 0;
    }
}

static uint16_t e1000_eeprom_read(e1000_t* dev, uint8_t addr)
{
    e1000_write32(dev, E1000_EERD, ((uint32_t)addr << 8) | 0x01);
    for (volatile int i = 0; i < 100000; i++) {
        uint32_t data = e1000_read32(dev, E1000_EERD);
        if (data & 0x10) return (uint16_t)(data >> 16);
    }
    return 0;
}

static void e1000_read_mac(e1000_t* dev)
{
    if (dev->has_eeprom) {
        for (int i = 0; i < 3; i++) {
            uint16_t word = e1000_eeprom_read(dev, (uint8_t)i);
            dev->mac[i * 2] = (uint8_t)(word & 0xFF);
            dev->mac[i * 2 + 1] = (uint8_t)(word >> 8);
        }
    } else {
        for (int i = 0; i < 6; i++) {
            dev->mac[i] = (uint8_t)e1000_read32(dev, 0x5400 + i * 4);
        }
    }
}

void e1000_init(void)
{
    memset(e1000_devices, 0, sizeof(e1000_devices));
    e1000_count = 0;

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t id = pci_read_config(b, d, f, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (!e1000_match_device(vendor, device)) continue;
                if (e1000_count >= E1000_MAX_CONTROLLERS) return;

                e1000_t* dev = &e1000_devices[e1000_count];
                dev->bus = (uint8_t)b;
                dev->device = (uint8_t)d;
                dev->function = (uint8_t)f;
                dev->vendor_id = vendor;
                dev->device_id = device;
                dev->irq = pci_read_config(b, d, f, 0x3C) & 0xFF;
                spin_init(&dev->lock);

                uint32_t bar = pci_read_config(b, d, f, 0x10);
                dev->mmio_base = (uint64_t)(bar & 0xFFFFFFF0);
                uint32_t bar_hi = pci_read_config(b, d, f, 0x14);
                dev->mmio_base |= ((uint64_t)bar_hi << 32);

                uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
                pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                uint32_t eecd = e1000_read32(dev, E1000_EECD);
                dev->has_eeprom = (eecd & 0x10) ? 1 : 0;

                e1000_read_mac(dev);

                uint32_t ctrl = e1000_read32(dev, E1000_CTRL);
                e1000_write32(dev, E1000_CTRL, ctrl | E1000_CTRL_RST);
                for (volatile int i = 0; i < 1000000; i++);

                e1000_write32(dev, E1000_CTRL,
                              E1000_CTRL_SLU | E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPLX);

                dev->tx_desc = (e1000_tx_desc_t*)memory_alloc(
                    E1000_TX_DESC_COUNT * sizeof(e1000_tx_desc_t) + 4096);
                dev->tx_desc_phys = (uint64_t)(uintptr_t)dev->tx_desc;
                dev->rx_desc = (e1000_rx_desc_t*)memory_alloc(
                    E1000_RX_DESC_COUNT * sizeof(e1000_rx_desc_t) + 4096);
                dev->rx_desc_phys = (uint64_t)(uintptr_t)dev->rx_desc;
                if (!dev->tx_desc || !dev->rx_desc) continue;

                memset(dev->tx_desc, 0, E1000_TX_DESC_COUNT * sizeof(e1000_tx_desc_t));
                memset(dev->rx_desc, 0, E1000_RX_DESC_COUNT * sizeof(e1000_rx_desc_t));

                for (int i = 0; i < E1000_TX_DESC_COUNT; i++) {
                    dev->tx_buffers[i] = memory_alloc(E1000_MAX_PKT_SIZE);
                    dev->tx_phys[i] = (uint64_t)(uintptr_t)dev->tx_buffers[i];
                    dev->tx_desc[i].addr = dev->tx_phys[i];
                }
                for (int i = 0; i < E1000_RX_DESC_COUNT; i++) {
                    dev->rx_buffers[i] = memory_alloc(E1000_MAX_PKT_SIZE);
                    dev->rx_phys[i] = (uint64_t)(uintptr_t)dev->rx_buffers[i];
                    dev->rx_desc[i].addr = dev->rx_phys[i];
                    dev->rx_desc[i].status = 0;
                }

                e1000_write32(dev, E1000_TDBAL, (uint32_t)dev->tx_desc_phys);
                e1000_write32(dev, E1000_TDBAH, (uint32_t)(dev->tx_desc_phys >> 32));
                e1000_write32(dev, E1000_TDLEN, E1000_TX_DESC_COUNT * sizeof(e1000_tx_desc_t));
                e1000_write32(dev, E1000_TDH, 0);
                e1000_write32(dev, E1000_TDT, 0);

                e1000_write32(dev, E1000_RDBAL, (uint32_t)dev->rx_desc_phys);
                e1000_write32(dev, E1000_RDBAH, (uint32_t)(dev->rx_desc_phys >> 32));
                e1000_write32(dev, E1000_RDLEN, E1000_RX_DESC_COUNT * sizeof(e1000_rx_desc_t));
                e1000_write32(dev, E1000_RDH, 0);
                e1000_write32(dev, E1000_RDT, E1000_RX_DESC_COUNT - 1);

                e1000_write32(dev, E1000_RCTL,
                              E1000_RCTL_EN | E1000_RCTL_SBP |
                              E1000_RCTL_BAM | E1000_RCTL_LPE |
                              E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC);

                e1000_write32(dev, E1000_TCTL,
                              E1000_TCTL_EN | E1000_TCTL_PSP |
                              (0x10 << E1000_TCTL_CT_SHIFT) |
                              (0x40 << E1000_TCTL_COLD_SHIFT));

                e1000_write32(dev, E1000_TIPG, 0x0060200A);

                e1000_write32(dev, E1000_IMS,
                              E1000_IMS_RXDMT0 | E1000_IMS_RXT0 |
                              E1000_IMS_TXDW | E1000_IMS_LSC);

                dev->tx_current = 0;
                dev->rx_current = 0;
                dev->initialized = 1;
                e1000_count++;
            }
        }
    }
}

e1000_t* e1000_get_controller(uint8_t index)
{
    if (index >= e1000_count) return NULL;
    return &e1000_devices[index];
}

int e1000_send(e1000_t* dev, const void* data, uint32_t len)
{
    if (!dev || !dev->initialized || !data || len == 0 || len > E1000_MAX_PKT_SIZE)
        return -1;

    spinlock_acquire(&dev->lock);

    uint32_t idx = dev->tx_current;
    e1000_tx_desc_t* desc = &dev->tx_desc[idx];

    if (desc->status & E1000_TX_DESC_DD) {
        desc->status &= ~E1000_TX_DESC_DD;
    }

    memcpy(dev->tx_buffers[idx], data, len);
    if (len < 60) {
        memset((uint8_t*)dev->tx_buffers[idx] + len, 0, 60 - len);
        len = 60;
    }

    desc->length = (uint16_t)len;
    desc->cmd = E1000_TX_DESC_EOP | E1000_TX_DESC_IFCS | E1000_TX_DESC_RS;

    uint32_t tdt = e1000_read32(dev, E1000_TDT);
    tdt = (tdt + 1) % E1000_TX_DESC_COUNT;
    e1000_write32(dev, E1000_TDT, tdt);

    dev->tx_current = (idx + 1) % E1000_TX_DESC_COUNT;
    spinlock_release(&dev->lock);
    return (int)len;
}

int e1000_recv(e1000_t* dev, void* buf, uint32_t* len)
{
    if (!dev || !dev->initialized || !buf || !len) return -1;

    spinlock_acquire(&dev->lock);

    uint32_t idx = dev->rx_current;
    e1000_rx_desc_t* desc = &dev->rx_desc[idx];

    if (!(desc->status & E1000_RX_DESC_DD)) {
        spinlock_release(&dev->lock);
        return -2;
    }

    if (desc->status & E1000_RX_DESC_EOP) {
        uint32_t pkt_len = desc->length;
        if (pkt_len > 0 && pkt_len <= E1000_MAX_PKT_SIZE) {
            memcpy(buf, dev->rx_buffers[idx], pkt_len);
            *len = pkt_len;
        }

        desc->status = 0;
        uint32_t rdt = e1000_read32(dev, E1000_RDT);
        rdt = (rdt + 1) % E1000_RX_DESC_COUNT;
        e1000_write32(dev, E1000_RDT, rdt);

        dev->rx_current = (idx + 1) % E1000_RX_DESC_COUNT;
        spinlock_release(&dev->lock);
        return 0;
    }

    desc->status = 0;
    dev->rx_current = (idx + 1) % E1000_RX_DESC_COUNT;
    spinlock_release(&dev->lock);
    return -3;
}

void e1000_irq_handler(void)
{
    for (uint8_t i = 0; i < e1000_count; i++) {
        e1000_t* dev = &e1000_devices[i];
        uint32_t icr = e1000_read32(dev, E1000_ICR);
        if (icr == 0) continue;
        e1000_write32(dev, E1000_IMC, 0xFFFFFFFF);
        e1000_write32(dev, E1000_IMS,
                      E1000_IMS_RXDMT0 | E1000_IMS_RXT0 |
                      E1000_IMS_TXDW | E1000_IMS_LSC);
    }
}

uint8_t e1000_get_count(void)
{
    return e1000_count;
}