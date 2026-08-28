#include <arch/rtl8169.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <string.h>

#define RTL8169_MAX_CONTROLLERS 4

static rtl8169_t rtl8169_devices[RTL8169_MAX_CONTROLLERS];
static uint8_t rtl8169_count = 0;

static inline uint8_t rtl8169_inb(rtl8169_t* dev, uint16_t reg) { return inb(dev->io_base + reg); }
static inline uint16_t rtl8169_inw(rtl8169_t* dev, uint16_t reg) { return inw(dev->io_base + reg); }
static inline uint32_t rtl8169_inl(rtl8169_t* dev, uint16_t reg) { return inl(dev->io_base + reg); }
static inline void rtl8169_outb(rtl8169_t* dev, uint16_t reg, uint8_t val) { outb(dev->io_base + reg, val); }
static inline void rtl8169_outw(rtl8169_t* dev, uint16_t reg, uint16_t val) { outw(dev->io_base + reg, val); }
static inline void rtl8169_outl(rtl8169_t* dev, uint16_t reg, uint32_t val) { outl(dev->io_base + reg, val); }

static int rtl8169_match_device(uint16_t vendor, uint16_t device)
{
    if (vendor != RTL8169_VENDOR_ID) return 0;
    switch (device) {
    case RTL8169_DEV_8169:
    case RTL8169_DEV_8167:
    case RTL8169_DEV_8168:
    case RTL8169_DEV_8111:
        return 1;
    default:
        return 0;
    }
}

void rtl8169_init(void)
{
    memset(rtl8169_devices, 0, sizeof(rtl8169_devices));
    rtl8169_count = 0;

    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                uint32_t id = pci_read_config(b, d, f, 0x00);
                uint16_t vendor = id & 0xFFFF;
                uint16_t device = (id >> 16) & 0xFFFF;

                if (!rtl8169_match_device(vendor, device)) continue;
                if (rtl8169_count >= RTL8169_MAX_CONTROLLERS) return;

                rtl8169_t* dev = &rtl8169_devices[rtl8169_count];
                dev->bus = (uint8_t)b;
                dev->device = (uint8_t)d;
                dev->function = (uint8_t)f;
                dev->vendor_id = vendor;
                dev->device_id = device;
                dev->io_base = pci_read_config(b, d, f, 0x10) & 0xFFFFFFFC;
                dev->irq = pci_read_config(b, d, f, 0x3C) & 0xFF;
                spin_init(&dev->lock);

                uint16_t cmd = pci_read_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04);
                cmd |= PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE;
                pci_write_config_word((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x04, cmd);

                rtl8169_outb(dev, RTL8169_CMD, RTL8169_CMD_RESET);
                for (volatile int i = 0; i < 1000000; i++);
                while (rtl8169_inb(dev, RTL8169_CMD) & RTL8169_CMD_RESET);

                for (int i = 0; i < 6; i++) {
                    dev->mac[i] = rtl8169_inb(dev, RTL8169_IDR0 + i);
                }

                uint32_t tx_desc_size = RTL8169_TX_DESC_COUNT * sizeof(rtl8169_tx_desc_t);
                dev->tx_desc = (rtl8169_tx_desc_t*)memory_alloc(tx_desc_size + 4096);
                if (!dev->tx_desc) continue;
                dev->tx_desc_phys = (uint64_t)(uintptr_t)dev->tx_desc;
                memset(dev->tx_desc, 0, tx_desc_size);

                uint32_t rx_desc_size = RTL8169_RX_DESC_COUNT * sizeof(rtl8169_rx_desc_t);
                dev->rx_desc = (rtl8169_rx_desc_t*)memory_alloc(rx_desc_size + 4096);
                if (!dev->rx_desc) {
                    memory_free(dev->tx_desc);
                    continue;
                }
                dev->rx_desc_phys = (uint64_t)(uintptr_t)dev->rx_desc;
                memset(dev->rx_desc, 0, rx_desc_size);

                for (int i = 0; i < RTL8169_TX_DESC_COUNT; i++) {
                    dev->tx_buffers[i] = memory_alloc(RTL8169_MAX_PKT_SIZE);
                    dev->tx_phys[i] = (uint64_t)(uintptr_t)dev->tx_buffers[i];
                    dev->tx_desc[i].addr = dev->tx_phys[i];
                    dev->tx_desc[i].cmd = 0;
                }

                for (int i = 0; i < RTL8169_RX_DESC_COUNT; i++) {
                    dev->rx_buffers[i] = memory_alloc(RTL8169_MAX_PKT_SIZE);
                    dev->rx_phys[i] = (uint64_t)(uintptr_t)dev->rx_buffers[i];
                    dev->rx_desc[i].addr = dev->rx_phys[i];
                    dev->rx_desc[i].cmd = RTL8169_RX_DESC_OWN | RTL8169_MAX_PKT_SIZE;
                    if (i == RTL8169_RX_DESC_COUNT - 1) {
                        dev->rx_desc[i].cmd |= RTL8169_RX_DESC_EOR;
                    }
                }

                dev->tx_desc[RTL8169_TX_DESC_COUNT - 1].cmd = RTL8169_TX_DESC_EOR;
                dev->tx_current = 0;
                dev->rx_current = 0;

                rtl8169_outl(dev, RTL8169_TXDESC_START_ADDR, (uint32_t)dev->tx_desc_phys);
                rtl8169_outl(dev, RTL8169_TXDESC_START_ADDR_HI, (uint32_t)(dev->tx_desc_phys >> 32));
                rtl8169_outl(dev, RTL8169_RXDESC_START_ADDR, (uint32_t)dev->rx_desc_phys);
                rtl8169_outl(dev, RTL8169_RXDESC_START_ADDR_HI, (uint32_t)(dev->rx_desc_phys >> 32));

                rtl8169_outl(dev, RTL8169_TXCFG, 0x03070700);
                rtl8169_outl(dev, RTL8169_RXCFG, 0x00007E00);

                rtl8169_outw(dev, RTL8169_INTRMITIGATE, 0x0000);
                rtl8169_outw(dev, RTL8169_IMR,
                             RTL8169_IMR_ROK | RTL8169_IMR_TOK |
                             RTL8169_IMR_RER | RTL8169_IMR_TER |
                             RTL8169_IMR_RXOVW | RTL8169_IMR_FOVW |
                             RTL8169_IMR_LINKCHG);

                rtl8169_outb(dev, RTL8169_CMD,
                             RTL8169_CMD_RX_ENABLE | RTL8169_CMD_TX_ENABLE);

                dev->initialized = 1;
                rtl8169_count++;
            }
        }
    }
}

rtl8169_t* rtl8169_get_controller(uint8_t index)
{
    if (index >= rtl8169_count) return NULL;
    return &rtl8169_devices[index];
}

int rtl8169_send(rtl8169_t* dev, const void* data, uint32_t len)
{
    if (!dev || !dev->initialized || !data || len == 0 || len > RTL8169_MAX_PKT_SIZE)
        return -1;

    spinlock_acquire(&dev->lock);

    uint32_t idx = dev->tx_current;
    rtl8169_tx_desc_t* desc = &dev->tx_desc[idx];

    if (desc->cmd & RTL8169_TX_DESC_OWN) {
        spinlock_release(&dev->lock);
        return -2;
    }

    memcpy(dev->tx_buffers[idx], data, len);
    if (len < 60) {
        memset((uint8_t*)dev->tx_buffers[idx] + len, 0, 60 - len);
        len = 60;
    }

    uint32_t cmd = RTL8169_TX_DESC_OWN | RTL8169_TX_DESC_FS | RTL8169_TX_DESC_LS | len;
    if (idx == RTL8169_TX_DESC_COUNT - 1) cmd |= RTL8169_TX_DESC_EOR;
    desc->cmd = cmd;

    dev->tx_current = (idx + 1) % RTL8169_TX_DESC_COUNT;
    spinlock_release(&dev->lock);
    return (int)len;
}

int rtl8169_recv(rtl8169_t* dev, void* buf, uint32_t* len)
{
    if (!dev || !dev->initialized || !buf || !len) return -1;

    spinlock_acquire(&dev->lock);

    uint32_t idx = dev->rx_current;
    rtl8169_rx_desc_t* desc = &dev->rx_desc[idx];

    if (desc->cmd & RTL8169_RX_DESC_OWN) {
        spinlock_release(&dev->lock);
        return -2;
    }

    if ((desc->cmd & (RTL8169_RX_DESC_FS | RTL8169_RX_DESC_LS)) !=
        (RTL8169_RX_DESC_FS | RTL8169_RX_DESC_LS)) {
        uint32_t new_cmd = RTL8169_RX_DESC_OWN | RTL8169_MAX_PKT_SIZE;
        if (idx == RTL8169_RX_DESC_COUNT - 1) new_cmd |= RTL8169_RX_DESC_EOR;
        desc->cmd = new_cmd;
        dev->rx_current = (idx + 1) % RTL8169_RX_DESC_COUNT;
        spinlock_release(&dev->lock);
        return -3;
    }

    uint32_t pkt_len = (desc->cmd & RTL8169_RX_DESC_LEN_M) - 4;
    if (pkt_len > 0 && pkt_len <= RTL8169_MAX_PKT_SIZE) {
        memcpy(buf, dev->rx_buffers[idx], pkt_len);
        *len = pkt_len;
    }

    uint32_t new_cmd = RTL8169_RX_DESC_OWN | RTL8169_MAX_PKT_SIZE;
    if (idx == RTL8169_RX_DESC_COUNT - 1) new_cmd |= RTL8169_RX_DESC_EOR;
    desc->cmd = new_cmd;

    dev->rx_current = (idx + 1) % RTL8169_RX_DESC_COUNT;
    spinlock_release(&dev->lock);
    return 0;
}

void rtl8169_irq_handler(void)
{
    for (uint8_t i = 0; i < rtl8169_count; i++) {
        rtl8169_t* dev = &rtl8169_devices[i];
        uint16_t isr = rtl8169_inw(dev, RTL8169_ISR);
        if (isr == 0) continue;
        rtl8169_outw(dev, RTL8169_ISR, isr);
    }
}

uint8_t rtl8169_get_count(void)
{
    return rtl8169_count;
}