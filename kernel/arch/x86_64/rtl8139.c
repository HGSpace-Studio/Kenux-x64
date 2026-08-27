#include <arch/rtl8139.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <string.h>

#define RTL8139_MAX_CONTROLLERS 4

static rtl8139_t rtl8139_devices[RTL8139_MAX_CONTROLLERS];
static uint8_t rtl8139_count = 0;

static inline uint8_t rtl8139_inb(rtl8139_t* dev, uint16_t reg)
{
    return inb(dev->io_base + reg);
}

static inline uint16_t rtl8139_inw(rtl8139_t* dev, uint16_t reg)
{
    return inw(dev->io_base + reg);
}

static inline uint32_t rtl8139_inl(rtl8139_t* dev, uint16_t reg)
{
    return inl(dev->io_base + reg);
}

static inline void rtl8139_outb(rtl8139_t* dev, uint16_t reg, uint8_t val)
{
    outb(dev->io_base + reg, val);
}

static inline void rtl8139_outw(rtl8139_t* dev, uint16_t reg, uint16_t val)
{
    outw(dev->io_base + reg, val);
}

static inline void rtl8139_outl(rtl8139_t* dev, uint16_t reg, uint32_t val)
{
    outl(dev->io_base + reg, val);
}

void rtl8139_init(void)
{
    memset(rtl8139_devices, 0, sizeof(rtl8139_devices));
    rtl8139_count = 0;

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t id = pci_read_config(b, d, f, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (vendor != RTL8139_VENDOR_ID || device != RTL8139_DEVICE_ID) continue;
                if (rtl8139_count >= RTL8139_MAX_CONTROLLERS) return;

                rtl8139_t* dev = &rtl8139_devices[rtl8139_count];
                dev->bus = (uint8_t)b;
                dev->device = (uint8_t)d;
                dev->function = (uint8_t)f;
                dev->vendor_id = vendor;
                dev->device_id = device;
                dev->io_base = pci_read_config(b, d, f, 0x10) & 0xFFFFFFFC;
                dev->irq = pci_read_config(b, d, f, 0x3C) & 0xFF;
                spin_init(&dev->lock);

                uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                cmd |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER;
                pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                rtl8139_outb(dev, RTL8139_CMD, RTL8139_CMD_RESET);
                for (volatile int i = 0; i < 1000000; i++);
                while (rtl8139_inb(dev, RTL8139_CMD) & RTL8139_CMD_RESET);

                for (int i = 0; i < 6; i++) {
                    dev->mac[i] = rtl8139_inb(dev, RTL8139_IDR0 + i);
                }

                dev->rx_buffer = memory_alloc(RTL8139_RX_BUFFER_SIZE + 4096);
                if (!dev->rx_buffer) continue;
                dev->rx_buffer_phys = (uint64_t)(uintptr_t)dev->rx_buffer;
                dev->rx_offset = 0;

                for (int i = 0; i < RTL8139_TX_DESC_COUNT; i++) {
                    dev->tx_buffers[i] = memory_alloc(RTL8139_TX_BUFFER_SIZE);
                    dev->tx_phys[i] = (uint64_t)(uintptr_t)dev->tx_buffers[i];
                }
                dev->tx_current = 0;

                memset(dev->rx_buffer, 0, RTL8139_RX_BUFFER_SIZE + 4096);
                rtl8139_outl(dev, RTL8139_RXBUF, (uint32_t)dev->rx_buffer_phys);

                for (int i = 0; i < RTL8139_TX_DESC_COUNT; i++) {
                    rtl8139_outl(dev, RTL8139_TXADDR0 + i * 4, (uint32_t)dev->tx_phys[i]);
                }

                rtl8139_outl(dev, RTL8139_TXCFG,
                             RTL8139_TXCFG_MAX_DMA_2048 | RTL8139_TXCFG_IFG96);
                rtl8139_outl(dev, RTL8139_RXCFG,
                             RTL8139_RXCFG_MAX_DMA_2048 |
                             RTL8139_RXCFG_ACCEPT_BROADCAST |
                             RTL8139_RXCFG_ACCEPT_PHYS_MATCH |
                             RTL8139_RXCFG_BUF_LEN_64K);

                rtl8139_outw(dev, RTL8139_IMR,
                             RTL8139_IMR_ROK | RTL8139_IMR_TOK |
                             RTL8139_IMR_RER | RTL8139_IMR_TER |
                             RTL8139_IMR_RXOVW | RTL8139_IMR_FOVW);

                rtl8139_outb(dev, RTL8139_CMD,
                             RTL8139_CMD_RX_ENABLE | RTL8139_CMD_TX_ENABLE);

                dev->initialized = 1;
                rtl8139_count++;
            }
        }
    }
}

rtl8139_t* rtl8139_get_controller(uint8_t index)
{
    if (index >= rtl8139_count) return NULL;
    return &rtl8139_devices[index];
}

int rtl8139_send(rtl8139_t* dev, const void* data, uint32_t len)
{
    if (!dev || !dev->initialized || !data || len == 0 || len > RTL8139_TX_BUFFER_SIZE)
        return -1;

    spinlock_acquire(&dev->lock);

    uint32_t tx_idx = dev->tx_current;
    uint32_t status = rtl8139_inl(dev, RTL8139_TXSTAT0 + tx_idx * 4);
    if (status & 0x01) {
        spinlock_release(&dev->lock);
        return -2;
    }

    memcpy(dev->tx_buffers[tx_idx], data, len);
    if (len < 60) {
        memset((uint8_t*)dev->tx_buffers[tx_idx] + len, 0, 60 - len);
        len = 60;
    }

    rtl8139_outl(dev, RTL8139_TXSTAT0 + tx_idx * 4, len | 0x00400000);

    dev->tx_current = (tx_idx + 1) % RTL8139_TX_DESC_COUNT;
    spinlock_release(&dev->lock);
    return (int)len;
}

int rtl8139_recv(rtl8139_t* dev, void* buf, uint32_t* len)
{
    if (!dev || !dev->initialized || !buf || !len) return -1;

    spinlock_acquire(&dev->lock);

    uint8_t* rx = (uint8_t*)dev->rx_buffer;
    uint32_t offset = dev->rx_offset;
    uint32_t header = *(uint32_t*)(rx + offset);

    uint32_t rx_len = (header >> 16) & 0xFFFF;
    uint32_t rx_status = header & 0xFFFF;

    if (rx_len == 0 || rx_len > RTL8139_RX_BUFFER_SIZE) {
        rtl8139_outw(dev, RTL8139_CMD, RTL8139_CMD_RX_BUF_EMPTY);
        dev->rx_offset = 0;
        spinlock_release(&dev->lock);
        return -2;
    }

    if (rx_status & 0x01) {
        uint32_t pkt_len = rx_len - RTL8139_RX_HEADER_LEN - 4;
        if (pkt_len > 0 && pkt_len <= 2048) {
            memcpy(buf, rx + offset + RTL8139_RX_HEADER_LEN, pkt_len);
            *len = pkt_len;
            spinlock_release(&dev->lock);
            return 0;
        }
    }

    offset += (rx_len + 3) & ~3U;
    offset %= RTL8139_RX_BUFFER_SIZE;
    dev->rx_offset = offset;

    if (offset > RTL8139_RX_BUFFER_SIZE - 2048) {
        dev->rx_offset = 0;
    }

    rtl8139_outw(dev, RTL8139_CMD, RTL8139_CMD_RX_BUF_EMPTY);
    spinlock_release(&dev->lock);
    return -3;
}

void rtl8139_irq_handler(void)
{
    for (uint8_t i = 0; i < rtl8139_count; i++) {
        rtl8139_t* dev = &rtl8139_devices[i];
        uint16_t isr = rtl8139_inw(dev, RTL8139_ISR);
        if (isr == 0) continue;
        rtl8139_outw(dev, RTL8139_ISR, isr);
    }
}

uint8_t rtl8139_get_count(void)
{
    return rtl8139_count;
}