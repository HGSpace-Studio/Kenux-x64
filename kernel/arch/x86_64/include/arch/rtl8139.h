#ifndef ARCH_X86_64_RTL8139_H
#define ARCH_X86_64_RTL8139_H

#include <arch/types.h>
#include <arch/pci.h>
#include <arch/spinlock.h>

#define RTL8139_VENDOR_ID    0x10EC
#define RTL8139_DEVICE_ID    0x8139

#define RTL8139_IDR0         0x00
#define RTL8139_MAR0         0x08
#define RTL8139_TXSTAT0      0x10
#define RTL8139_TXADDR0      0x20
#define RTL8139_RXBUF        0x30
#define RTL8139_EARLYRXBCNT  0x34
#define RTL8139_CMD          0x37
#define RTL8139_IMR          0x3C
#define RTL8139_ISR          0x3E
#define RTL8139_TXCFG        0x40
#define RTL8139_RXCFG        0x44
#define RTL8139_MPC          0x4C
#define RTL8139_CFG9346      0x50
#define RTL8139_CONFIG0      0x51
#define RTL8139_CONFIG1      0x52
#define RTL8139_CONFIG2      0x53
#define RTL8139_CONFIG3      0x54
#define RTL8139_CONFIG4      0x55
#define RTL8139_CONFIG5      0x56
#define RTL8139_MEDIASTAT    0x58
#define RTL8139_MII          0x5A
#define RTL8139_HLTCLK       0x5C
#define RTL8139_ERBCNT       0x5D
#define RTL8139_CSCR         0x5C
#define RTL8139_MIIREG       0x5E
#define RTL8139_MIIADDR      0x5F
#define RTL8139_PCSCR        0x60
#define RTL8139_PHYAR        0x60
#define RTL8139_CABLE_DETECT 0x64
#define RTL8139_RXMISS       0x6C
#define RTL8139_TXCOLCNT     0x6E

#define RTL8139_CMD_RESET       0x10
#define RTL8139_CMD_RX_ENABLE   0x08
#define RTL8139_CMD_TX_ENABLE   0x04
#define RTL8139_CMD_RX_BUF_EMPTY 0x01

#define RTL8139_IMR_ROK     0x0001
#define RTL8139_IMR_RER     0x0002
#define RTL8139_IMR_TOK     0x0004
#define RTL8139_IMR_TER     0x0008
#define RTL8139_IMR_RXOVW   0x0010
#define RTL8139_IMR_PUN     0x0020
#define RTL8139_IMR_FOVW    0x0040
#define RTL8139_IMR_LENCHG  0x2000
#define RTL8139_IMR_TIMEOUT 0x4000
#define RTL8139_IMR_SERR    0x8000

#define RTL8139_TXCFG_MAX_DMA_2048  0x00700000
#define RTL8139_TXCFG_MAX_DMA_1024  0x00600000
#define RTL8139_TXCFG_IFG96         0x02000000

#define RTL8139_RXCFG_MAX_DMA_2048  0x00070000
#define RTL8139_RXCFG_MAX_DMA_1024  0x00060000
#define RTL8139_RXCFG_ACCEPT_BROADCAST  0x00000008
#define RTL8139_RXCFG_ACCEPT_MULTICAST  0x00000010
#define RTL8139_RXCFG_ACCEPT_PHYS_MATCH 0x00000040
#define RTL8139_RXCFG_ACCEPT_ALL   0x00000080
#define RTL8139_RXCFG_WRAP         0x00000080
#define RTL8139_RXCFG_BUF_LEN_64K  0x00001800

#define RTL8139_RX_BUFFER_SIZE  65536
#define RTL8139_TX_BUFFER_SIZE  4096
#define RTL8139_TX_DESC_COUNT   4

#define RTL8139_RX_HEADER_LEN  4

typedef struct {
    uint8_t   bus;
    uint8_t   device;
    uint8_t   function;
    uint32_t  io_base;
    uint8_t   irq;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint8_t   mac[6];
    void*     rx_buffer;
    uint64_t  rx_buffer_phys;
    uint32_t  rx_offset;
    void*     tx_buffers[RTL8139_TX_DESC_COUNT];
    uint64_t  tx_phys[RTL8139_TX_DESC_COUNT];
    uint32_t  tx_current;
    spinlock_t lock;
    int       initialized;
} rtl8139_t;

void rtl8139_init(void);
rtl8139_t* rtl8139_get_controller(uint8_t index);
int rtl8139_send(rtl8139_t* dev, const void* data, uint32_t len);
int rtl8139_recv(rtl8139_t* dev, void* buf, uint32_t* len);
void rtl8139_irq_handler(void);
uint8_t rtl8139_get_count(void);

#endif