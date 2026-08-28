#ifndef ARCH_X86_64_E1000_H
#define ARCH_X86_64_E1000_H

#include <arch/types.h>
#include <arch/pci.h>
#include <arch/spinlock.h>

#define E1000_VENDOR_ID     0x8086
#define E1000_DEV_82540EM   0x100E
#define E1000_DEV_82543GC   0x1004
#define E1000_DEV_82545EM   0x100F
#define E1000_DEV_82545GM   0x1026
#define E1000_DEV_82546EB   0x1027
#define E1000_DEV_82547GI   0x1075
#define E1000_DEV_82541ER   0x1078
#define E1000_DEV_82541GI   0x1076
#define E1000_DEV_82574L    0x10D3
#define E1000_DEV_82577LM   0x10F0
#define E1000_DEV_82579V    0x10CB
#define E1000_DEV_I217_LM   0x153A
#define E1000_DEV_I217_V    0x153B
#define E1000_DEV_I219_V    0x1565
#define E1000_DEV_I219_LM   0x1568

#define E1000_CTRL          0x00000
#define E1000_STATUS        0x00008
#define E1000_EECD          0x00010
#define E1000_EERD          0x00014
#define E1000_CTRL_EXT      0x00018
#define E1000_FLA           0x0001C
#define E1000_MDIC          0x00020
#define E1000_SCTL          0x00024
#define E1000_FEXT          0x00028
#define E1000_FEXTNVM       0x00030
#define E1000_FCAL          0x00028
#define E1000_FCAH          0x0002C
#define E1000_FCT           0x00030
#define E1000_VET           0x00038
#define E1000_ICR           0x000C0
#define E1000_ITR           0x000C4
#define E1000_ICS           0x000C8
#define E1000_IMS           0x000D0
#define E1000_IMC           0x000D8
#define E1000_IAM           0x000E0
#define E1000_RCTL          0x00100
#define E1000_FCTTV         0x00170
#define E1000_TXCW          0x00178
#define E1000_RXCW          0x00180
#define E1000_TCTL          0x00400
#define E1000_TCTL_EXT      0x00404
#define E1000_TIPG          0x00410
#define E1000_AIT           0x00458
#define E1000_LEDCTL        0x00E00

#define E1000_TDBAL         0x03800
#define E1000_TDBAH         0x03804
#define E1000_TDLEN         0x03808
#define E1000_TDH           0x03810
#define E1000_TDT           0x03818
#define E1000_TIDV          0x03820
#define E1000_TXDCTL        0x03828
#define E1000_TADV          0x0382C

#define E1000_RDBAL         0x02800
#define E1000_RDBAH         0x02804
#define E1000_RDLEN         0x02808
#define E1000_RDH           0x02810
#define E1000_RDT           0x02818
#define E1000_RDTR          0x02820
#define E1000_RXDCTL        0x02828
#define E1000_RADV          0x0282C
#define E1000_RSRPD         0x02C00

#define E1000_CTRL_FD       0x00000001
#define E1000_CTRL_GIO_MASTER 0x00000002
#define E1000_CTRL_LRST     0x00000008
#define E1000_CTRL_TME      0x00000010
#define E1000_CTRL_ASDE     0x00000020
#define E1000_CTRL_SLU      0x00000040
#define E1000_CTRL_RFE      0x00000080
#define E1000_CTRL_VME      0x00000100
#define E1000_CTRL_PHY_RST  0x00000800
#define E1000_CTRL_FRCSPD   0x00000800
#define E1000_CTRL_FRCDPLX  0x00001000
#define E1000_CTRL_SPD_100  0x00000000
#define E1000_CTRL_SPD_1000 0x00002000
#define E1000_CTRL_RST      0x04000000

#define E1000_RCTL_EN       0x00000002
#define E1000_RCTL_SBP      0x00000004
#define E1000_RCTL_UPE      0x00000008
#define E1000_RCTL_MPE      0x00000010
#define E1000_RCTL_LPE      0x00000020
#define E1000_RCTL_LBM_NO   0x00000000
#define E1000_RCTL_LBM_MAC  0x00000040
#define E1000_RCTL_RDMTS_HALF 0x00000000
#define E1000_RCTL_RDMTS_QUARTER 0x00000100
#define E1000_RCTL_RDMTS_EIGHTH 0x00000200
#define E1000_RCTL_MO_SHIFT 12
#define E1000_RCTL_BAM      0x00008000
#define E1000_RCTL_BSIZE_2048 0x00000000
#define E1000_RCTL_BSIZE_1024 0x00010000
#define E1000_RCTL_BSIZE_512  0x00020000
#define E1000_RCTL_BSIZE_256  0x00030000
#define E1000_RCTL_VME      0x00040000
#define E1000_RCTL_DTYPE    0x00000000
#define E1000_RCTL_SECRC    0x04000000

#define E1000_TCTL_EN       0x00000002
#define E1000_TCTL_PSP      0x00000008
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TCTL_SWXOFF   0x00400000
#define E1000_TCTL_RTLC     0x01000000

#define E1000_TX_DESC_DD    0x00000001
#define E1000_TX_DESC_EOP   0x00000002
#define E1000_TX_DESC_IFCS  0x00000004
#define E1000_TX_DESC_RS    0x00000008
#define E1000_TX_DESC_DEXT  0x20000000
#define E1000_TX_DESC_VLE   0x40000000
#define E1000_TX_DESC_DTYPE 0x80000000

#define E1000_RX_DESC_DD    0x00000001
#define E1000_RX_DESC_EOP   0x00000002
#define E1000_RX_DESC_IXSM  0x00000004
#define E1000_RX_DESC_VP    0x00000008
#define E1000_RX_DESC_TCPCS 0x00000010
#define E1000_RX_DESC_IPCS  0x00000020
#define E1000_RX_DESC_PTYPE 0x00000000
#define E1000_RX_DESC_DEXT  0x20000000

#define E1000_IMS_RXDMT0    0x00000010
#define E1000_IMS_RXT0      0x00000040
#define E1000_IMS_TXDW      0x00000001
#define E1000_IMS_TXQE      0x00000002
#define E1000_IMS_LSC       0x00000004
#define E1000_IMS_RXSEQ     0x00000008

#define E1000_TX_DESC_COUNT  256
#define E1000_RX_DESC_COUNT  256
#define E1000_MAX_PKT_SIZE   2048

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    uint8_t   bus;
    uint8_t   device;
    uint8_t   function;
    uint64_t  mmio_base;
    uint8_t   irq;
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint8_t   mac[6];
    int       has_eeprom;
    e1000_tx_desc_t* tx_desc;
    uint64_t  tx_desc_phys;
    e1000_rx_desc_t* rx_desc;
    uint64_t  rx_desc_phys;
    void*     tx_buffers[E1000_TX_DESC_COUNT];
    uint64_t  tx_phys[E1000_TX_DESC_COUNT];
    void*     rx_buffers[E1000_RX_DESC_COUNT];
    uint64_t  rx_phys[E1000_RX_DESC_COUNT];
    uint32_t  tx_current;
    uint32_t  rx_current;
    spinlock_t lock;
    int       initialized;
} e1000_t;

void e1000_init(void);
e1000_t* e1000_get_controller(uint8_t index);
int e1000_send(e1000_t* dev, const void* data, uint32_t len);
int e1000_recv(e1000_t* dev, void* buf, uint32_t* len);
void e1000_irq_handler(void);
uint8_t e1000_get_count(void);

#endif