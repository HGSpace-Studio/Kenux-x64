#ifndef KERNEL_DRIVERS_USB_OHCI_EHCI_UHCI_H
#define KERNEL_DRIVERS_USB_OHCI_EHCI_UHCI_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define UHCI_PCI_CLASS     0x0C
#define UHCI_PCI_SUBCLASS  0x03
#define UHCI_PCI_PROGIF    0x00

#define UHCI_CMD           0x00
#define UHCI_STS           0x02
#define UHCI_FRNUM         0x06
#define UHCI_FLBASE        0x08
#define UHCI_SOF           0x0C
#define UHCI_PORTSC1       0x10
#define UHCI_PORTSC2       0x12

#define UHCI_CMD_RS        0x0001
#define UHCI_CMD_HCRESET   0x0002
#define UHCI_CMD_EGSM      0x0008
#define UHCI_CMD_FGR       0x0010

#define UHCI_STS_HCHALTED  0x0020
#define UHCI_STS_HCPE      0x0004
#define UHCI_STS_USBPI     0x0001

typedef struct {
    uint32_t link;
    uint32_t status;
    uint32_t token;
    uint32_t buffer;
} __attribute__((packed, aligned(16))) uhci_qh_t;

typedef struct {
    uint32_t link;
    uint32_t status;
    uint32_t token;
    uint32_t buffer;
} __attribute__((packed, aligned(16))) uhci_td_t;

typedef struct {
    pci_device_t*    pci_dev;
    uint16_t         io_base;
    uint32_t*        frame_list;
    uint32_t         frame_list_phys;
    uhci_qh_t*       qh_pool;
    uhci_td_t*       td_pool;
    int              qh_count;
    int              td_count;
    int              port_count;
    int              running;
    spinlock_t       lock;
} uhci_hc_t;

int  uhci_init(uhci_hc_t* hc, pci_device_t* pci_dev);
void uhci_shutdown(uhci_hc_t* hc);
int  uhci_reset(uhci_hc_t* hc);
int  uhci_start(uhci_hc_t* hc);
int  uhci_stop(uhci_hc_t* hc);
void uhci_irq_handler(int irq, void* ctx);

#define OHCI_PCI_CLASS     0x0C
#define OHCI_PCI_SUBCLASS  0x03
#define OHCI_PCI_PROGIF    0x10

#define OHCI_REVISION      0x00
#define OHCI_CONTROL       0x04
#define OHCI_CMD_STATUS    0x08
#define OHCI_INT_STATUS    0x0C
#define OHCI_INT_ENABLE    0x10
#define OHCI_INT_DISABLE   0x14
#define OHCI_HCCA          0x18
#define OHCI_PERIOD_ED     0x1C
#define OHCI_CTRL_ED       0x20
#define OHCI_BULK_ED       0x24
#define OHCI_CTRL_HEAD_ED  0x28
#define OHCI_BULK_HEAD_ED  0x2C
#define OHCI_DONE_HEAD     0x30
#define OHCI_FM_INTERVAL   0x34
#define OHCI_FM_REMAINING  0x38
#define OHCI_FM_NUMBER     0x3C
#define OHCI_PERIOD_START  0x40
#define OHCI_LS_THRESHOLD  0x44
#define OHCI_RH_DESC_A     0x48
#define OHCI_RH_DESC_B     0x4C
#define OHCI_RH_STATUS     0x50
#define OHCI_RH_PORT0      0x54

#define OHCI_CTRL_CBSR     0x00000030
#define OHCI_CTRL_PLE      0x00000004
#define OHCI_CTRL_IE       0x00000008
#define OHCI_CTRL_CLE      0x00000010
#define OHCI_CTRL_BLE      0x00000020
#define OHCI_CTRL_HCFS     0x000000C0
#define OHCI_CTRL_HCFS_RESET   0x00000000
#define OHCI_CTRL_HCFS_RESUME 0x00000040
#define OHCI_CTRL_HCFS_OPER   0x00000080
#define OHCI_CTRL_HCFS_SUSP   0x000000C0

typedef struct {
    uint32_t control;
    uint32_t tail_ptr;
    uint32_t head_ptr;
    uint32_t next_ed;
} ohci_ed_t;

typedef struct {
    uint32_t info;
    uint32_t cur_buf;
    uint32_t next_td;
    uint32_t buf_end;
} ohci_td_t;

typedef struct {
    uint32_t int_table[32];
    uint16_t frame_num;
    uint16_t pad1;
    uint32_t done_head;
    uint8_t  reserved[116];
} __attribute__((packed, aligned(256))) ohci_hcca_t;

typedef struct {
    pci_device_t*       pci_dev;
    volatile uint32_t*  mmio;
    ohci_hcca_t*        hcca;
    uint32_t            hcca_phys;
    int                 port_count;
    int                 running;
    spinlock_t          lock;
} ohci_hc_t;

int  ohci_init(ohci_hc_t* hc, pci_device_t* pci_dev);
void ohci_shutdown(ohci_hc_t* hc);
int  ohci_reset(ohci_hc_t* hc);
int  ohci_start(ohci_hc_t* hc);
int  ohci_stop(ohci_hc_t* hc);
void ohci_irq_handler(int irq, void* ctx);

#define EHCI_PCI_CLASS     0x0C
#define EHCI_PCI_SUBCLASS  0x03
#define EHCI_PCI_PROGIF    0x20

#define EHCI_CAPLENGTH     0x00
#define EHCI_HCIVERSION   0x02
#define EHCI_HCSPARAMS     0x04
#define EHCI_HCCPARAMS     0x08

#define EHCI_USBCMD        0x00
#define EHCI_USBSTS        0x04
#define EHCI_USBINTR       0x08
#define EHCI_FRINDEX       0x0C
#define EHCI_CTRLDSSEG     0x10
#define EHCI_PERIODICLIST  0x14
#define EHCI_ASYNCLIST     0x18
#define EHCI_CONFIGFLAG    0x40
#define EHCI_PORTSC_BASE   0x44

#define EHCI_CMD_RUN       0x00000001
#define EHCI_CMD_HCRESET   0x00000002
#define EHCI_CMD_ASE       0x00000020
#define EHCI_CMD_PSE       0x00000010
#define EHCI_CMD_ITP       0x00000008

#define EHCI_STS_HCHALTED  0x00001000
#define EHCI_STS_ASE       0x00002000
#define EHCI_STS_PSE       0x00004000
#define EHCI_STS_INT       0x00000001

typedef struct {
    uint32_t link;
    uint32_t charac;
    uint32_t cap;
    uint32_t cur_qtd;
    uint32_t next_qtd;
    uint32_t alt_qtd;
    uint32_t token;
    uint32_t buf[5];
} __attribute__((packed, aligned(32))) ehci_qh_t;

typedef struct {
    uint32_t next_qtd;
    uint32_t alt_qtd;
    uint32_t token;
    uint32_t buf[5];
} __attribute__((packed, aligned(32))) ehci_qtd_t;

typedef struct {
    pci_device_t*       pci_dev;
    volatile uint8_t*   mmio;
    uint32_t            cap_length;
    uint16_t            hci_version;
    uint32_t            hcs_params;
    uint32_t            hcc_params;
    volatile uint32_t*  op_regs;
    uint32_t*           periodic_list;
    uint32_t            periodic_list_phys;
    ehci_qh_t*          async_qh;
    uint32_t            async_qh_phys;
    int                 port_count;
    int                 companion_count;
    int                 running;
    spinlock_t          lock;
} ehci_hc_t;

int  ehci_init(ehci_hc_t* hc, pci_device_t* pci_dev);
void ehci_shutdown(ehci_hc_t* hc);
int  ehci_reset(ehci_hc_t* hc);
int  ehci_start(ehci_hc_t* hc);
int  ehci_stop(ehci_hc_t* hc);
void ehci_irq_handler(int irq, void* ctx);

#define PCIE_CFG_BASE      0xE0000000
#define PCIE_CFG_SIZE      0x10000000
#define PCIE_BUS_SHIFT     20
#define PCIE_DEV_SHIFT     15
#define PCIE_FUNC_SHIFT    12
#define PCIE_REG_SHIFT     2

#define PCIE_CAP_EXP       0x10

typedef struct {
    volatile uint32_t* cfg_base;
    uint32_t           cfg_size;
    int                start_bus;
    int                end_bus;
    spinlock_t         lock;
} pcie_bus_t;

int   pcie_init(pcie_bus_t* bus, uint64_t cfg_base, uint32_t cfg_size, int start_bus, int end_bus);
uint32_t pcie_config_read(pcie_bus_t* bus, uint8_t bus_num, uint8_t dev, uint8_t func, uint16_t offset);
void  pcie_config_write(pcie_bus_t* bus, uint8_t bus_num, uint8_t dev, uint8_t func, uint16_t offset, uint32_t value);
int   pcie_find_capability(pcie_bus_t* bus, uint8_t bus_num, uint8_t dev, uint8_t func, uint8_t cap_id);

#endif