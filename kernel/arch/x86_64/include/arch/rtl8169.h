#ifndef ARCH_X86_64_RTL8169_H
#define ARCH_X86_64_RTL8169_H

#include <arch/types.h>
#include <arch/pci.h>
#include <arch/spinlock.h>

#define RTL8169_VENDOR_ID    0x10EC
#define RTL8169_DEV_8169     0x8169
#define RTL8169_DEV_8167     0x8167
#define RTL8169_DEV_8168     0x8168
#define RTL8169_DEV_8111     0x8111

#define RTL8169_IDR0         0x00
#define RTL8169_MAR0         0x08
#define RTL8169_TXDESC_START_ADDR 0x20
#define RTL8169_TXDESC_START_ADDR_HI 0x24
#define RTL8169_RXDESC_START_ADDR 0x28
#define RTL8169_RXDESC_START_ADDR_HI 0x2C
#define RTL8169_CMD          0x37
#define RTL8169_IMR          0x3C
#define RTL8169_ISR          0x3E
#define RTL8169_TXCFG        0x40
#define RTL8169_RXCFG        0x44
#define RTL8169_MPC          0x4C
#define RTL8169_CFG9346      0x50
#define RTL8169_RMS          0x51
#define RTL8169_CPLUS_CMD    0xE0
#define RTL8169_RX_MAX_PKT   0xDA
#define RTL8169_INTRMITIGATE 0xE2
#define RTL8169_TNPDS        0x08
#define RTL8169_THPDS        0x10
#define RTL8169_RDSAR        0xE4

#define RTL8169_CMD_RESET       0x10
#define RTL8169_CMD_RX_ENABLE   0x08
#define RTL8169_CMD_TX_ENABLE   0x04
#define RTL8169_CMD_RX_BUF_EMPTY 0x01

#define RTL8169_IMR_ROK     0x0001
#define RTL8169_IMR_RER     0x0002
#define RTL8169_IMR_TOK     0x0004
#define RTL8169_IMR_TER     0x0008
#define RTL8169_IMR_RXOVW   0x0010
#define RTL8169_IMR_PUN     0x0020
#define RTL8169_IMR_FOVW    0x0040
#define RTL8169_IMR_LINKCHG 0x2000
#define RTL8169_IMR_TIMEOUT 0x4000
#define RTL8169_IMR_SERR    0x8000

#define RTL8169_TX_DESC_OWN   0x80000000
#define RTL8169_TX_DESC_EOR   0x40000000
#define RTL8169_TX_DESC_FS    0x20000000
#define RTL8169_TX_DESC_LS    0x10000000
#define RTL8169_TX_DESC_LEN_M 0x0000FFFF

#define RTL8169_RX_DESC_OWN   0x80000000
#define RTL8169_RX_DESC_EOR   0x40000000
#define RTL8169_RX_DESC_FS    0x20000000
#define RTL8169_RX_DESC_LS    0x10000000
#define RTL8169_RX_DESC_LEN_M 0x00003FFF

#define RTL8169_TX_DESC_COUNT  64
#define RTL8169_RX_DESC_COUNT  64
#define RTL8169_MAX_PKT_SIZE   2048

typedef struct {
    uint32_t cmd;
    uint32_t vlan;
    uint64_t addr;
} __attribute__((packed)) rtl8169_tx_desc_t;

typedef struct {
    uint32_t cmd;
    uint32_t vlan;
    uint64_t addr;
} __attribute__((packed)) rtl8169_rx_desc_t;

typedef struct {
    uint8_t   bus;
    uint8_t   device;
    uint8_t   function;
    uint32_t  io_base;
    uint8_t   irq;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint8_t   mac[6];
    rtl8169_tx_desc_t* tx_desc;
    uint64_t  tx_desc_phys;
    rtl8169_rx_desc_t* rx_desc;
    uint64_t  rx_desc_phys;
    void*     tx_buffers[RTL8169_TX_DESC_COUNT];
    uint64_t  tx_phys[RTL8169_TX_DESC_COUNT];
    void*     rx_buffers[RTL8169_RX_DESC_COUNT];
    uint64_t  rx_phys[RTL8169_RX_DESC_COUNT];
    uint32_t  tx_current;
    uint32_t  rx_current;
    spinlock_t lock;
    int       initialized;
} rtl8169_t;

void rtl8169_init(void);
rtl8169_t* rtl8169_get_controller(uint8_t index);
int rtl8169_send(rtl8169_t* dev, const void* data, uint32_t len);
int rtl8169_recv(rtl8169_t* dev, void* buf, uint32_t* len);
void rtl8169_irq_handler(void);
uint8_t rtl8169_get_count(void);

#endif