#ifndef KERNEL_DRIVERS_AHCI_H
#define KERNEL_DRIVERS_AHCI_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define AHCI_PCI_CLASS    0x01
#define AHCI_PCI_SUBCLASS 0x06

#define AHCI_HBA_CAP      0x00
#define AHCI_HBA_GHC      0x04
#define AHCI_HBA_IS       0x08
#define AHCI_HBA_PI       0x0C
#define AHCI_HBA_VS       0x10
#define AHCI_HBA_CCCC     0x14
#define AHCI_HBA_CCCP     0x18
#define AHCI_HBA_EM_LOC   0x1C
#define AHCI_HBA_EM_CTL   0x20
#define AHCI_HBA_CAP2     0x24
#define AHCI_HBA_BOHC     0x28

#define AHCI_GHC_AE       0x80000000
#define AHCI_GHC_MRSM     0x00000008
#define AHCI_GHC_IE       0x00000002
#define AHCI_GHC_HR       0x00000001

#define AHCI_PX_CLB       0x00
#define AHCI_PX_CLBU      0x04
#define AHCI_PX_FB        0x08
#define AHCI_PX_FBU       0x0C
#define AHCI_PX_IS        0x10
#define AHCI_PX_IE        0x14
#define AHCI_PX_CMD       0x18
#define AHCI_PX_RES1      0x1C
#define AHCI_PX_TFD       0x20
#define AHCI_PX_SIG       0x24
#define AHCI_PX_SSTS      0x28
#define AHCI_PX_SCTL      0x2C
#define AHCI_PX_SERR      0x30
#define AHCI_PX_SACT      0x34
#define AHCI_PX_CI        0x38
#define AHCI_PX_SNTF      0x3C
#define AHCI_PX_FBS       0x40
#define AHCI_PX_RES2      0x44
#define AHCI_PX_DEVCTL    0x48
#define AHCI_PX_DEVADDR   0x4C

#define AHCI_CMD_ST       0x0001
#define AHCI_CMD_SUD      0x0002
#define AHCI_CMD_POD      0x0004
#define AHCI_CMD_CLO      0x0008
#define AHCI_CMD_FRE      0x0010
#define AHCI_CMD_CCS_SHIFT 5
#define AHCI_CMD_MPSS     0x0020
#define AHCI_CMD_FR       0x0040
#define AHCI_CMD_CR       0x0080
#define AHCI_CMD_AATA     0x0100
#define AHCI_CMD_ALPE     0x0200
#define AHCI_CMD_ASP      0x0400
#define AHCI_CMD_ICC_SHIFT 12
#define AHCI_CMD_ACTIVE   0x1000

#define AHCI_TFD_STS_ERR  0x01
#define AHCI_TFD_STS_DRQ  0x08
#define AHCI_TFD_STS_BSY  0x80

#define AHCI_IS_DHRS      0x00000001
#define AHCI_IS_PSS       0x00000002
#define AHCI_IS_DSS       0x00000004
#define AHCI_IS_SDBS      0x00000008
#define AHCI_IS_UFS       0x00000010
#define AHCI_IS_DPS       0x00000020
#define AHCI_IS_PCS       0x00000040
#define AHCI_IS_DMPS      0x00000080
#define AHCI_IS_PRCS      0x00010000

#define AHCI_CMD_HDR_READ  0x0020
#define AHCI_CMD_HDR_WRITE 0x0040
#define AHCI_CMD_HDR_PREFETCH 0x0080
#define AHCI_CMD_HDR_RESET 0x0100
#define AHCI_CMD_HDR_BIST  0x0200
#define AHCI_CMD_HDR_CLR_BUSY 0x0400

#define AHCI_MAX_PORTS     32
#define AHCI_MAX_SLOTS     32
#define AHCI_CMD_TBL_SIZE  256
#define AHCI_PRDT_MAX      8

typedef struct {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t prdbc;
    uint64_t cmd_table_addr;
} __attribute__((packed)) ahci_cmd_header_t;

typedef struct {
    uint8_t  cfis[64];
    uint8_t  acmd[64];
    uint8_t  res[96];
} ahci_cmd_table_t;

typedef struct {
    uint64_t data_base;
    uint32_t data_byte_count;
    uint32_t reserved;
} __attribute__((packed)) ahci_prdt_entry_t;

typedef struct {
    int          present;
    int          port_num;
    uint32_t     sig;
    uint64_t     sector_count;
    uint32_t     sector_size;
    volatile uint32_t* port_regs;
    ahci_cmd_header_t* cmd_headers;
    ahci_cmd_table_t*  cmd_tables;
    ahci_prdt_entry_t* prdt;
    void*       dma_buf;
    uint32_t    dma_buf_phys;
    int         cmd_slot;
    spinlock_t  lock;
} ahci_port_t;

typedef struct {
    pci_device_t*    pci_dev;
    volatile uint32_t* hba_regs;
    uint32_t         cap;
    uint32_t         cap2;
    uint32_t         port_impl;
    int              n_ports;
    int              n_cmd_slots;
    int              is_64bit;
    ahci_port_t      ports[AHCI_MAX_PORTS];
    spinlock_t       lock;
} ahci_hba_t;

int  ahci_init(ahci_hba_t* hba, pci_device_t* pci_dev);
void ahci_shutdown(ahci_hba_t* hba);
int  ahci_port_start(ahci_hba_t* hba, int port);
int  ahci_port_stop(ahci_hba_t* hba, int port);
int  ahci_port_cmd_slot(ahci_port_t* port);
int  ahci_read(ahci_port_t* port, uint64_t lba, void* buf, uint32_t count);
int  ahci_write(ahci_port_t* port, uint64_t lba, const void* buf, uint32_t count);
int  ahci_identify(ahci_port_t* port, uint16_t* identify);
void ahci_irq_handler(int irq, void* ctx);

#endif