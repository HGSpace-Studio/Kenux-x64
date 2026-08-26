

#ifndef ARCH_X86_64_NVME_H
#define ARCH_X86_64_NVME_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define NVME_PCI_CLASS       0x01
#define NVME_PCI_SUBCLASS    0x08
#define NVME_PCI_PROG_IF     0x02

#define NVME_BAR_TYPE_MASK       0x00000001UL
#define NVME_BAR_TYPE_MEMORY     0x00000000UL
#define NVME_BAR_TYPE_IO         0x00000001UL
#define NVME_BAR_MEM_TYPE_MASK   0x00000006UL
#define NVME_BAR_MEM_32BIT       0x00000000UL
#define NVME_BAR_MEM_64BIT       0x00000004UL
#define NVME_BAR_MEM_PREFETCH    0x00000008UL

#define NVME_PCI_BAR0_OFFSET     0x10
#define NVME_PCI_BAR1_OFFSET     0x14
#define NVME_PCI_COMMAND_OFFSET  0x04
#define NVME_PCI_COMMAND_BME     (1 << 2)
#define NVME_PCI_COMMAND_MSE     (1 << 1)

#define NVME_REG_CAP        0x0000
#define NVME_REG_VS         0x0008
#define NVME_REG_INTMS      0x000C
#define NVME_REG_INTMC      0x0010
#define NVME_REG_CC         0x0014
#define NVME_REG_CSTS       0x0018
#define NVME_REG_NSSR       0x001C
#define NVME_REG_AQA        0x0024
#define NVME_REG_ASQ        0x0028
#define NVME_REG_ACQ        0x0030
#define NVME_REG_CQSEL      0x0024
#define NVME_REG_CQH        0x0038
#define NVME_REG_CQT        0x003C
#define NVME_REG_SQSEL      0x0024
#define NVME_REG_SQH        0x0040
#define NVME_REG_SQT        0x0044
#define NVME_REG_DBBS       0x1000
#define NVME_REG_DBBL       0x1000

#define NVME_CAP_MPSMIN_SHIFT      48
#define NVME_CAP_MPSMIN_MASK       (0xFULL << 48)
#define NVME_CAP_MPSMAX_SHIFT      52
#define NVME_CAP_MPSMAX_MASK       (0xFULL << 52)
#define NVME_CAP_NSSRS_SHIFT       36
#define NVME_CAP_NSSRS_MASK        (1ULL  << 36)
#define NVME_CAP_CSS_SHIFT         37
#define NVME_CAP_CSS_MASK          (0xFFULL << 37)
#define NVME_CAP_CSS_NVM           (1ULL << 37)
#define NVME_CAP_MQES_SHIFT        0
#define NVME_CAP_MQES_MASK         0xFFFF
#define NVME_CAP_CQR_SHIFT         16
#define NVME_CAP_CQR_MASK          (1ULL << 16)
#define NVME_CAP_TO_SHIFT          24
#define NVME_CAP_TO_MASK           (0xFFULL << 24)
#define NVME_CAP_DSTRD_SHIFT       32
#define NVME_CAP_DSTRD_MASK        (0xFULL << 32)
#define NVME_CAP_NSSRC_SHIFT       37
#define NVME_CAP_NSSRC_MASK        (1ULL << 37)
#define NVME_CAP_AMS_SHIFT         17
#define NVME_CAP_AMS_MASK          (0x3ULL << 17)
#define NVME_CAP_AMS_WRR           (0ULL << 17)
#define NVME_CAP_AMS_VQ            (1ULL << 17)
#define NVME_CAP_AMS_WRR_NS        (2ULL << 17)

#define NVME_CC_EN_SHIFT       0
#define NVME_CC_EN_MASK        (1UL << 0)
#define NVME_CC_EN_DISABLE     0
#define NVME_CC_EN_ENABLE      1
#define NVME_CC_IOCQES_SHIFT   20
#define NVME_CC_IOCQES_MASK    (0xFUL << 20)
#define NVME_CC_IOSQES_SHIFT   16
#define NVME_CC_IOSQES_MASK    (0xFUL << 16)
#define NVME_CC_SHN_SHIFT      1
#define NVME_CC_SHN_MASK       (0x3UL << 1)
#define NVME_CC_SHN_NONE       (0UL << 1)
#define NVME_CC_SHN_NORMAL     (1UL << 1)
#define NVME_CC_SHN_ABRUPT     (2UL << 1)
#define NVME_CC_AMS_SHIFT      11
#define NVME_CC_AMS_MASK       (0x7UL << 11)
#define NVME_CC_MPS_SHIFT      7
#define NVME_CC_MPS_MASK       (0xFUL << 7)
#define NVME_CC_CSS_SHIFT      4
#define NVME_CC_CSS_MASK       (0x7UL << 4)
#define NVME_CC_CSS_NVM        0
#define NVME_CC_ABC_SHIFT      5
#define NVME_CC_ABC_MASK       (0xFFUL << 5)

#define NVME_CSTS_RDY_SHIFT    0
#define NVME_CSTS_RDY_MASK     (1UL << 0)
#define NVME_CSTS_RDY_NOT      0
#define NVME_CSTS_RDY_READY    1
#define NVME_CSTS_CFS_SHIFT    1
#define NVME_CSTS_CFS_MASK     (1UL << 1)
#define NVME_CSTS_SHST_SHIFT   2
#define NVME_CSTS_SHST_MASK    (0x3UL << 2)
#define NVME_CSTS_SHST_NORMAL  (0UL << 2)
#define NVME_CSTS_SHST_OCCUR   (1UL << 2)
#define NVME_CSTS_SHST_COMP    (2UL << 2)

#define NVME_OPC_FLUSH             0x00
#define NVME_OPC_WRITE             0x01
#define NVME_OPC_READ              0x02
#define NVME_OPC_WRITE_UNCOR       0x04
#define NVME_OPC_COMPARE           0x05
#define NVME_OPC_WRITE_ZEROES      0x08
#define NVME_OPC_DATASET_MANAGEMENT 0x09
#define NVME_OPC_RESERVATION_REG   0x0D
#define NVME_OPC_RESERVATION_REPORT 0x0E
#define NVME_OPC_RESERVATION_ACQ   0x11
#define NVME_OPC_RESERVATION_REL   0x15
#define NVME_OPC_IDENTIFY          0x06
#define NVME_OPC_ABORT             0x08
#define NVME_OPC_SET_FEATURES      0x09
#define NVME_OPC_GET_FEATURES      0x0A
#define NVME_OPC_ASYNC_EVENT_REQ   0x0C
#define NVME_OPC_CREATE_IO_SQ      0x01
#define NVME_OPC_CREATE_IO_CQ      0x05
#define NVME_OPC_DELETE_IO_SQ      0x00
#define NVME_OPC_DELETE_IO_CQ      0x04
#define NVME_OPC_GET_LOG_PAGE       0x02

#define NVME_IDENTIFY_CNS_NS        0x00
#define NVME_IDENTIFY_CNS_CTRL      0x01
#define NVME_IDENTIFY_CNS_NS_LIST   0x02
#define NVME_IDENTIFY_CNS_DESC_NS   0x03

typedef struct {

    uint8_t  opcode;
    uint8_t  fuse;
    uint16_t cid;

    uint32_t nsid;

    uint32_t cdw2;
    uint32_t cdw3;

    uint64_t prp1;

    uint64_t prp2;

    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_cmd_t;

typedef struct {
    uint32_t cdw0;
    uint32_t rsvd1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cqe_t;

#define NVME_CQE_SC(status)     ((status) & 0xFF)
#define NVME_CQE_SCT(status)    (((status) >> 8) & 0x7F)
#define NVME_CQE_DNR(status)    (((status) >> 15) & 0x1)

#define NVME_SCT_GENERIC         0x00
#define NVME_SCT_CMD_SPEC        0x01
#define NVME_SCT_INTEGRITY_ERR   0x02
#define NVME_SCT_VENDOR_SPEC     0x07

#define NVME_SC_SUCCESS          0x00
#define NVME_SC_INVALID_OPCODE   0x01
#define NVME_SC_INVALID_FIELD    0x02
#define NVME_SC_CMDID_CONFLICT   0x03
#define NVME_SC_DATA_XFER_ERR    0x04
#define NVME_SC_POWER_LOSS       0x05
#define NVME_SC_INTERNAL_ERR     0x06
#define NVME_SC_ABORT_REQ        0x07
#define NVME_SC_ABORT_QUEUE      0x08
#define NVME_SC_FUSED_ERR        0x09
#define NVME_SC_FUSED_MISSING    0x0A
#define NVME_SC_INVALID_NS       0x0B
#define NVME_SC_CMD_SEQ_ERR      0x0C
#define NVME_SC_SGL_INV_DESC     0x0D
#define NVME_SC_NUM_SGL_DESC_EXC 0x0E
#define NVME_SC_SGL_INV_OFFSET   0x0F
#define NVME_SC_SGL_SUBCONT_INV  0x10
#define NVME_SC_SGL_INV_DATA_LEN 0x11
#define NVME_SC_NS_WRITE_PROTECT 0x20
#define NVME_SC_TRANSIENT_ERR    0x21
#define NVME_SC_INVALID_FMT      0x22
#define NVME_SC_INV_ACCESS_CNTL  0x23
#define NVME_SC_INV_NS_SEC       0x24
#define NVME_SC_READONLY_NS      0x25

typedef struct {

    uint16_t vid;
    uint16_t ssvid;

    uint8_t  sn[20];

    uint8_t  mn[40];

    uint8_t  fr[8];

    uint8_t  rab;

    uint8_t  ieee[3];

    uint8_t  cmic;

    uint8_t  mdts;

    uint16_t cntlid;

    uint32_t ver;

    uint32_t rtd3r;

    uint32_t rtd3e;

    uint32_t oacs;

    uint32_t oncs;

    uint32_t fuses;

    uint32_t fna;

    uint32_t vwc;

    uint16_t awun;

    uint16_t awupf;

    uint8_t  nvscc;

    uint8_t  reserved1;

    uint16_t acwc;

    uint32_t sgls;

    uint32_t mnan;

    uint8_t  reserved2[128];

    uint8_t  sqes;

    uint8_t  cqes;

    uint8_t  reserved3[4096 - 258];
} __attribute__((packed)) nvme_identify_t;

#define NVME_SQES_MIN(val)   ((val) & 0x0F)
#define NVME_SQES_MAX(val)   (((val) >> 4) & 0x0F)
#define NVME_CQES_MIN(val)   ((val) & 0x0F)
#define NVME_CQES_MAX(val)   (((val) >> 4) & 0x0F)

typedef struct {

    uint64_t nsze;

    uint64_t ncap;

    uint64_t nuse;

    uint8_t  nsfeat;

    uint8_t  nlbaf;

    uint8_t  flbas;

    uint8_t  mc;

    uint8_t  dpc;

    uint8_t  dps;

    uint8_t  nmic;

    uint8_t  rescap;

    uint8_t  fpi;

    uint8_t  dlfeat;

    uint16_t nawun;

    uint16_t nawupf;

    uint16_t nacwu;

    uint16_t nacs;

    uint16_t nsan;

    uint16_t nsd;

    uint16_t nsc;

    uint16_t nso;

    uint16_t nsss;

    uint16_t nmc;

    uint16_t noiob;

    uint64_t nvmcap[2];

    uint8_t  vs[4096 - 128 - 72];
} __attribute__((packed)) nvme_ns_identify_t;

typedef struct {

    uint16_t ms;
    uint8_t  lbads;
    uint8_t  rp;
    uint8_t  reserved[13];
} __attribute__((packed)) nvme_lba_format_t;

#define NVME_FLBAS_FORMAT(flbas)  ((flbas) & 0x0F)
#define NVME_LBA_SIZE(lbads)      (1ULL << (lbads))

typedef struct {
    uint32_t nsid;
    uint64_t nsze;
    uint64_t ncap;
    uint8_t  nlbaf;
    uint8_t  flbas;
    uint8_t  lbads;
    uint32_t block_size;
    uint64_t total_size;
    int      active;
} nvme_ns_t;

#define NVME_ADMIN_QUEUE_DEPTH    32
#define NVME_IO_QUEUE_DEPTH       64
#define NVME_MIN_QUEUE_DEPTH       2
#define NVME_MAX_QUEUE_DEPTH       4096

typedef struct {
    nvme_cmd_t *base;
    uint64_t    base_phys;
    uint32_t    depth;
    uint16_t    sqid;
    volatile uint16_t head;
    volatile uint16_t tail;
    uint32_t    db_offset;
    spinlock_t  lock;
} nvme_sq_t;

typedef struct {
    nvme_cqe_t *base;
    uint64_t    base_phys;
    uint32_t    depth;
    uint16_t    cqid;
    volatile uint16_t head;
    volatile uint16_t tail;
    uint32_t    db_offset;
    uint16_t    phase;
    spinlock_t  lock;
    int         irq_vector;
} nvme_cq_t;

#define NVME_MAX_NAMESPACES     256
#define NVME_MAX_IO_QUEUES      1
#define NVME_IO_SQ_ID           1
#define NVME_IO_CQ_ID           1

#define NVME_CTRL_STATE_RESET        0
#define NVME_CTRL_STATE_INITING      1
#define NVME_CTRL_STATE_READY        2
#define NVME_CTRL_STATE_FAILED       3
#define NVME_CTRL_STATE_SHUTDOWN     4

typedef struct {
    uint16_t sqid;
    uint16_t cqid;
    uint16_t qsize;
    uint16_t sq_flags;
    uint64_t prp1;
    uint16_t cqid_for_create;
} nvme_create_sq_param_t;

typedef struct {
    uint16_t cqid;
    uint16_t qsize;
    uint16_t cq_flags;
    uint32_t irq_vector;
    uint64_t prp1;
} nvme_create_cq_param_t;

typedef struct {

    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;

    void    *reg_base;
    uint64_t reg_base_phys;
    uint64_t reg_size;

    uint64_t cap;
    uint32_t version;
    uint32_t mps;
    uint32_t mpsmax;
    uint32_t mpsmin;
    uint32_t dstrd;
    uint32_t mqes;

    nvme_sq_t admin_sq;
    nvme_cq_t admin_cq;

    nvme_sq_t io_sq;
    nvme_cq_t io_cq;
    int      io_queues_created;

    nvme_ns_t namespaces[NVME_MAX_NAMESPACES];
    uint32_t active_namespaces;

    nvme_identify_t identify_data;

    volatile uint32_t state;
    uint32_t admin_cid;
    uint32_t io_cid;
} nvme_controller_t;

#define nvme_mb()    __sync_synchronize()

#define nvme_wmb()   __sync_synchronize()

#define nvme_rmb()   __sync_synchronize()

static inline uint32_t nvme_read_reg32(volatile void *base, uint32_t offset)
{
    return *(volatile uint32_t *)((uint8_t *)base + offset);
}

static inline void nvme_write_reg32(volatile void *base, uint32_t offset, uint32_t val)
{
    *(volatile uint32_t *)((uint8_t *)base + offset) = val;
}

static inline uint64_t nvme_read_reg64(volatile void *base, uint32_t offset)
{
    uint64_t lo = (uint64_t)nvme_read_reg32(base, offset);
    uint64_t hi = (uint64_t)nvme_read_reg32(base, offset + 4);
    return (hi << 32) | lo;
}

static inline void nvme_write_reg64(volatile void *base, uint32_t offset, uint64_t val)
{
    nvme_write_reg32(base, offset,     (uint32_t)(val & 0xFFFFFFFF));
    nvme_write_reg32(base, offset + 4, (uint32_t)(val >> 32));
}

int nvme_probe(void);

int nvme_init(nvme_controller_t *ctrl);

int nvme_identify_controller(nvme_controller_t *ctrl);

int nvme_identify_namespace(nvme_controller_t *ctrl, uint32_t ns_id, nvme_ns_t *ns);

int nvme_create_io_queues(nvme_controller_t *ctrl);

int nvme_read(nvme_controller_t *ctrl, uint32_t ns_id, uint64_t lba,
              uint32_t count, void *buffer);

int nvme_write(nvme_controller_t *ctrl, uint32_t ns_id, uint64_t lba,
               uint32_t count, const void *buffer);

int nvme_flush(nvme_controller_t *ctrl, uint32_t ns_id);

int nvme_shutdown(nvme_controller_t *ctrl);

int nvme_get_info(nvme_controller_t *ctrl);

nvme_ns_t *nvme_get_namespace(nvme_controller_t *ctrl, uint32_t ns_id);

#endif
