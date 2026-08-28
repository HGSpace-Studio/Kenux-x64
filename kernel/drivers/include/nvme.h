#ifndef KERNEL_DRIVERS_NVME_H
#define KERNEL_DRIVERS_NVME_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define NVME_PCI_CLASS    0x01
#define NVME_PCI_SUBCLASS 0x08

#define NVME_REG_CAP      0x0000
#define NVME_REG_VS       0x0008
#define NVME_REG_INTMS    0x000C
#define NVME_REG_INTMC    0x0010
#define NVME_REG_CC       0x0014
#define NVME_REG_CSTS     0x001C
#define NVME_REG_NSSR     0x0020
#define NVME_REG_AQA      0x0024
#define NVME_REG_ASQ      0x0028
#define NVME_REG_ACQ      0x0030

#define NVME_CAP_MQES_SHIFT   0
#define NVME_CAP_CQR_SHIFT    16
#define NVME_CAP_AMS_SHIFT    17
#define NVME_CAP_TO_SHIFT     24
#define NVME_CAP_DSTRD_SHIFT  32
#define NVME_CAP_NSSRS_SHIFT  36
#define NVME_CAP_CSS_SHIFT    37
#define NVME_CAP_BPS_SHIFT    56
#define NVME_CAP_MPSMIN_SHIFT 56
#define NVME_CAP_MPSMAX_SHIFT 52

#define NVME_CC_EN       (1 << 0)
#define NVME_CC_CSS_SHIFT 4
#define NVME_CC_MPS_SHIFT 7
#define NVME_CC_AMS_SHIFT 11
#define NVME_CC_SHN_SHIFT 14
#define NVME_CC_IOSQES_SHIFT 16
#define NVME_CC_IOCQES_SHIFT 20

#define NVME_CSTS_RDY    (1 << 0)
#define NVME_CSTS_CFS    (1 << 1)
#define NVME_CSTS_SHST_SHIFT 2
#define NVME_CSTS_NSSRO  (1 << 4)

#define NVME_SQ_ENTRY_SIZE  64
#define NVME_CQ_ENTRY_SIZE  16

#define NVME_CMD_FLUSH         0x00
#define NVME_CMD_WRITE         0x01
#define NVME_CMD_READ          0x02
#define NVME_CMD_WRITE_UNCOR   0x04
#define NVME_CMD_COMPARE       0x05
#define NVME_CMD_WRITE_ZEROES  0x08
#define NVME_CMD_DSM           0x09
#define NVME_CMD_RESV_REG      0x0D
#define NVME_CMD_RESV_REPORT   0x0E
#define NVME_CMD_RESV_ACQUIRE  0x11
#define NVME_CMD_RESV_RELEASE  0x15

#define NVME_ADMIN_CMD_DELETE_SQ    0x00
#define NVME_ADMIN_CMD_CREATE_SQ    0x01
#define NVME_ADMIN_CMD_DELETE_CQ    0x04
#define NVME_ADMIN_CMD_CREATE_CQ    0x05
#define NVME_ADMIN_CMD_IDENTIFY     0x06
#define NVME_ADMIN_CMD_ABORT        0x08
#define NVME_ADMIN_CMD_SET_FEATURES 0x09
#define NVME_ADMIN_CMD_GET_FEATURES 0x0A
#define NVME_ADMIN_CMD_ASYNC_EVENT  0x0C

#define NVME_SC_SUCCESS           0x0000
#define NVME_SC_INVALID_OPCODE    0x0001
#define NVME_SC_INVALID_FIELD     0x0002
#define NVME_SC_DATA_XFER_ERROR   0x0003
#define NVME_SC_INTERNAL_ERROR    0x0004
#define NVME_SC_ABORT_REQ         0x0007
#define NVME_SC_ABORT_LIMIT       0x0008
#define NVME_SC_ABORT_FAILED      0x0009
#define NVME_SC_ABORT_MISSING     0x000A
#define NVME_SC_DNR               0x4000

#define NVME_MAX_QUEUES      64
#define NVME_QUEUE_DEPTH     256
#define NVME_MAX_NAMESPACES  256

typedef struct {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t command_id;
    uint32_t nsid;
    uint32_t cdw2;
    uint32_t cdw3;
    uint64_t metadata;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sqe_t;

typedef struct {
    uint32_t result;
    uint32_t result1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t command_id;
    uint16_t status;
} __attribute__((packed)) nvme_cqe_t;

typedef struct {
    nvme_sqe_t*  sq_entries;
    nvme_cqe_t*  cq_entries;
    uint32_t     sq_tail;
    uint32_t     cq_head;
    uint16_t     sq_id;
    uint16_t     cq_id;
    uint32_t     depth;
    uint64_t     sq_phys;
    uint64_t     cq_phys;
    spinlock_t   lock;
} nvme_queue_t;

typedef struct {
    uint32_t nsid;
    uint64_t nsze;
    uint64_t ncap;
    uint32_t flbas;
    uint32_t dps;
    uint32_t nmic;
    uint64_t eui64;
    uint8_t  ns_guid[16];
    uint32_t lbads;
    uint32_t ms;
    uint32_t sector_size;
    uint64_t sector_count;
} nvme_namespace_t;

typedef struct {
    pci_device_t*       pci_dev;
    volatile uint32_t*  regs;
    uint64_t            cap;
    uint32_t            vs;
    uint32_t            doorbell_stride;
    int                 max_queue_entries;
    int                 max_queues;
    int                 mps_min;
    int                 mps_max;
    nvme_queue_t        admin_q;
    nvme_queue_t        io_queues[NVME_MAX_QUEUES];
    int                 io_queue_count;
    nvme_namespace_t    namespaces[NVME_MAX_NAMESPACES];
    int                 ns_count;
    uint16_t            cmd_id_counter;
    spinlock_t          lock;
} nvme_ctrl_t;

int  nvme_init(nvme_ctrl_t* ctrl, pci_device_t* pci_dev);
void nvme_shutdown(nvme_ctrl_t* ctrl);
int  nvme_admin_cmd(nvme_ctrl_t* ctrl, nvme_sqe_t* cmd, nvme_cqe_t* result);
int  nvme_identify_ctrl(nvme_ctrl_t* ctrl, void* buf);
int  nvme_identify_ns(nvme_ctrl_t* ctrl, uint32_t nsid, void* buf);
int  nvme_create_io_queue(nvme_ctrl_t* ctrl, int qid, int depth);
int  nvme_read(nvme_ctrl_t* ctrl, uint32_t nsid, uint64_t lba, void* buf, uint32_t count);
int  nvme_write(nvme_ctrl_t* ctrl, uint32_t nsid, uint64_t lba, const void* buf, uint32_t count);
int  nvme_flush(nvme_ctrl_t* ctrl, uint32_t nsid);
void nvme_irq_handler(int irq, void* ctx);

#endif