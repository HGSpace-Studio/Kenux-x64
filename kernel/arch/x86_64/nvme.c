

#include <arch/nvme.h>
#include <arch/pci.h>
#include <arch/memory.h>
#include <arch/spinlock.h>
#include <string.h>

static int nvme_wait_csts_rdy(nvme_controller_t *ctrl, int ready, uint32_t timeout_us);
static int nvme_submit_admin_cmd(nvme_controller_t *ctrl, nvme_cmd_t *cmd);
static int nvme_poll_admin_cq(nvme_controller_t *ctrl, nvme_cqe_t *cqe);
static int nvme_submit_cmd(nvme_sq_t *sq, nvme_cmd_t *cmd);
static int nvme_poll_cq(nvme_cq_t *cq, nvme_cqe_t *cqe);
static void nvme_ring_sq_doorbell(nvme_controller_t *ctrl, nvme_sq_t *sq);
static void nvme_ring_cq_doorbell(nvme_controller_t *ctrl, nvme_cq_t *cq);

nvme_controller_t nvme_ctrl;

#define NVME_TIMEOUT_RESET      5000000
#define NVME_TIMEOUT_READY      2000000
#define NVME_TIMEOUT_CMD        1000000
#define NVME_TIMEOUT_SHUTDOWN   10000000
#define NVME_TIMEOUT_POLL_US   10

static void nvme_delay_us(uint32_t us)
{

    volatile uint32_t count;
    for (volatile uint32_t i = 0; i < us; i++) {

        for (count = 0; count < 1000; count++) {
            __asm__ volatile ("nop");
        }
    }
}

static void *nvme_alloc_queue(size_t size, uint64_t *phys_addr)
{

    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t paddr = memory_alloc_physical(pages);

    if (paddr == 0) {
        return NULL;
    }

    if (phys_addr) {
        *phys_addr = paddr;
    }

    return (void *)(uintptr_t)paddr;
}

static void nvme_free_queue(void *addr, size_t size)
{
    if (!addr) return;
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    memory_free_physical((uint64_t)(uintptr_t)addr, pages);
}

static int nvme_wait_csts_rdy(nvme_controller_t *ctrl, int ready, uint32_t timeout_us)
{
    uint32_t elapsed = 0;

    while (elapsed < timeout_us) {
        uint32_t csts = nvme_read_reg32(ctrl->reg_base, NVME_REG_CSTS);

        if (csts & NVME_CSTS_CFS_MASK) {
            return -2;
        }

        int rdy = (csts & NVME_CSTS_RDY_MASK) ? 1 : 0;
        if (rdy == ready) {
            return 0;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    return -1;
}

static void nvme_ring_sq_doorbell(nvme_controller_t *ctrl, nvme_sq_t *sq)
{
    uint32_t db_offset = 0x1000 + (2 * (uint32_t)sq->sqid) * (4 << ctrl->dstrd);

    nvme_wmb();

    nvme_write_reg32(ctrl->reg_base, db_offset, sq->tail);
}

static void nvme_ring_cq_doorbell(nvme_controller_t *ctrl, nvme_cq_t *cq)
{
    uint32_t db_offset = 0x1000 + (2 * (uint32_t)cq->cqid + 1) * (4 << ctrl->dstrd);

    nvme_wmb();

    nvme_write_reg32(ctrl->reg_base, db_offset, cq->head);
}

static int nvme_submit_cmd(nvme_sq_t *sq, nvme_cmd_t *cmd)
{
    spin_lock(&sq->lock);

    uint16_t next_tail = (sq->tail + 1) % sq->depth;
    if (next_tail == sq->head) {
        spin_unlock(&sq->lock);
        return -1;
    }

    sq->base[sq->tail] = *cmd;

    sq->tail = next_tail;

    spin_unlock(&sq->lock);
    return 0;
}

static int nvme_poll_cq(nvme_cq_t *cq, nvme_cqe_t *cqe)
{
    spin_lock(&cq->lock);

    if (cq->head == cq->tail) {
        spin_unlock(&cq->lock);
        return -1;
    }

    nvme_rmb();

    nvme_cqe_t *entry = &cq->base[cq->head];
    uint16_t entry_phase = (entry->status >> 15) & 0x1;

    if (entry_phase != cq->phase) {
        spin_unlock(&cq->lock);
        return -1;
    }

    if (cqe) {
        *cqe = *entry;
    }

    cq->head = (cq->head + 1) % cq->depth;

    if (cq->head == 0) {
        cq->phase = cq->phase ? 0 : 1;
    }

    spin_unlock(&cq->lock);
    return 0;
}

static int nvme_submit_admin_cmd(nvme_controller_t *ctrl, nvme_cmd_t *cmd)
{
    nvme_cqe_t cqe;
    uint32_t elapsed = 0;

    cmd->cid = (uint16_t)(ctrl->admin_cid & 0xFFFF);
    ctrl->admin_cid++;

    if (nvme_submit_cmd(&ctrl->admin_sq, cmd) != 0) {
        return -3;
    }

    nvme_ring_sq_doorbell(ctrl, &ctrl->admin_sq);

    while (elapsed < NVME_TIMEOUT_CMD) {
        if (nvme_poll_cq(&ctrl->admin_cq, &cqe) == 0) {

            nvme_ring_cq_doorbell(ctrl, &ctrl->admin_cq);

            if (NVME_CQE_SC(cqe.status) != NVME_SC_SUCCESS) {
                return -4;
            }
            return 0;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    return -1;
}

static int nvme_poll_admin_cq(nvme_controller_t *ctrl, nvme_cqe_t *cqe)
{
    uint32_t elapsed = 0;

    while (elapsed < NVME_TIMEOUT_CMD) {
        if (nvme_poll_cq(&ctrl->admin_cq, cqe) == 0) {
            nvme_ring_cq_doorbell(ctrl, &ctrl->admin_cq);
            return 0;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    return -1;
}

int nvme_probe(void)
{
    int found = 0;

    for (int b = 0; b < PCI_MAX_BUSSES && found == 0; b++) {
        for (int d = 0; d < PCI_MAX_DEVICES && found == 0; d++) {
            for (int f = 0; f < PCI_MAX_FUNCTIONS && found == 0; f++) {

                uint32_t vendor_device = pci_read_config(b, d, f, 0x00);
                uint16_t vid = vendor_device & 0xFFFF;
                if (vid == 0xFFFF || vid == 0x0000) {
                    continue;
                }

                uint32_t class_reg = pci_read_config(b, d, f, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass   = (class_reg >> 16) & 0xFF;
                uint8_t prog_if    = (class_reg >> 8) & 0xFF;

                if (class_code == NVME_PCI_CLASS &&
                    subclass == NVME_PCI_SUBCLASS &&
                    prog_if == NVME_PCI_PROG_IF) {

                    nvme_ctrl.bus = b;
                    nvme_ctrl.device = d;
                    nvme_ctrl.function = f;
                    nvme_ctrl.vendor_id = vid;
                    nvme_ctrl.device_id = (vendor_device >> 16) & 0xFFFF;

                    uint32_t bar0_lo = pci_read_config(b, d, f, NVME_PCI_BAR0_OFFSET);
                    uint32_t bar0_hi = pci_read_config(b, d, f, NVME_PCI_BAR1_OFFSET);

                    if (bar0_lo & NVME_BAR_TYPE_IO) {

                        continue;
                    }

                    if (bar0_lo & NVME_BAR_MEM_64BIT) {

                        nvme_ctrl.reg_base_phys = ((uint64_t)bar0_hi << 32) |
                                                   (bar0_lo & 0xFFFFFFF0UL);
                    } else {

                        nvme_ctrl.reg_base_phys = bar0_lo & 0xFFFFFFF0UL;
                    }

                    uint16_t cmd_reg = pci_read_config(b, d, f, NVME_PCI_COMMAND_OFFSET);
                    cmd_reg |= NVME_PCI_COMMAND_BME | NVME_PCI_COMMAND_MSE;
                    pci_write_config(b, d, f, NVME_PCI_COMMAND_OFFSET, cmd_reg);

                    nvme_ctrl.reg_base = (void *)(uintptr_t)nvme_ctrl.reg_base_phys;
                    nvme_ctrl.reg_size = PAGE_SIZE;

                    nvme_ctrl.state = NVME_CTRL_STATE_RESET;
                    nvme_ctrl.admin_cid = 1;
                    nvme_ctrl.io_cid = 1;
                    nvme_ctrl.io_queues_created = 0;
                    nvme_ctrl.active_namespaces = 0;
                    memset(nvme_ctrl.namespaces, 0, sizeof(nvme_ctrl.namespaces));

                    found = 1;
                }
            }
        }
    }

    return found;
}

int nvme_init(nvme_controller_t *ctrl)
{
    if (!ctrl || !ctrl->reg_base) {
        return -1;
    }

    ctrl->state = NVME_CTRL_STATE_INITING;

    ctrl->cap = nvme_read_reg64(ctrl->reg_base, NVME_REG_CAP);
    ctrl->version = nvme_read_reg32(ctrl->reg_base, NVME_REG_VS);

    ctrl->mqes   = (ctrl->cap & NVME_CAP_MQES_MASK) + 1;
    ctrl->mpsmax = (ctrl->cap >> NVME_CAP_MPSMAX_SHIFT) & 0xF;
    ctrl->mpsmin = (ctrl->cap >> NVME_CAP_MPSMIN_SHIFT) & 0xF;
    ctrl->dstrd  = (ctrl->cap >> NVME_CAP_DSTRD_SHIFT) & 0xF;

    uint32_t mps = 0;
    if (mps > ctrl->mpsmax) {
        mps = ctrl->mpsmax;
    }
    ctrl->mps = (1UL << (mps + 12));

    uint32_t cc = nvme_read_reg32(ctrl->reg_base, NVME_REG_CC);
    if (cc & NVME_CC_EN_MASK) {

        cc &= ~NVME_CC_EN_MASK;
        nvme_write_reg32(ctrl->reg_base, NVME_REG_CC, cc);
        nvme_wmb();

        int ret = nvme_wait_csts_rdy(ctrl, 0, NVME_TIMEOUT_RESET);
        if (ret < 0) {
            if (ret == -2) {
                ctrl->state = NVME_CTRL_STATE_FAILED;
                return -2;
            }
            return -3;
        }
    }

    uint32_t admin_sq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cmd_t);
    uint32_t admin_cq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cqe_t);

    ctrl->admin_sq.depth = NVME_ADMIN_QUEUE_DEPTH;
    ctrl->admin_sq.sqid  = 0;
    ctrl->admin_sq.head  = 0;
    ctrl->admin_sq.tail  = 0;

    ctrl->admin_cq.depth = NVME_ADMIN_QUEUE_DEPTH;
    ctrl->admin_cq.cqid  = 0;
    ctrl->admin_cq.head  = 0;
    ctrl->admin_cq.tail  = 0;
    ctrl->admin_cq.phase = 1;

    ctrl->admin_sq.base = (nvme_cmd_t *)nvme_alloc_queue(
        admin_sq_size, &ctrl->admin_sq.base_phys);
    if (!ctrl->admin_sq.base) {
        ctrl->state = NVME_CTRL_STATE_FAILED;
        return -4;
    }
    memset(ctrl->admin_sq.base, 0, admin_sq_size);

    ctrl->admin_cq.base = (nvme_cqe_t *)nvme_alloc_queue(
        admin_cq_size, &ctrl->admin_cq.base_phys);
    if (!ctrl->admin_cq.base) {
        nvme_free_queue(ctrl->admin_sq.base, admin_sq_size);
        ctrl->state = NVME_CTRL_STATE_FAILED;
        return -5;
    }
    memset(ctrl->admin_cq.base, 0, admin_cq_size);

    spin_init(&ctrl->admin_sq.lock);
    spin_init(&ctrl->admin_cq.lock);

    uint32_t aqa = ((NVME_ADMIN_QUEUE_DEPTH - 1) << 16) |
                   (NVME_ADMIN_QUEUE_DEPTH - 1);
    nvme_write_reg32(ctrl->reg_base, NVME_REG_AQA, aqa);

    nvme_write_reg64(ctrl->reg_base, NVME_REG_ASQ, ctrl->admin_sq.base_phys);
    nvme_write_reg64(ctrl->reg_base, NVME_REG_ACQ, ctrl->admin_cq.base_phys);

    cc = 0;
    cc |= (mps << NVME_CC_MPS_SHIFT);
    cc |= (NVME_CC_CSS_NVM << NVME_CC_CSS_SHIFT);

    cc |= (0 << NVME_CC_IOSQES_SHIFT);
    cc |= (0 << NVME_CC_IOCQES_SHIFT);

    nvme_write_reg32(ctrl->reg_base, NVME_REG_CC, cc);
    nvme_wmb();

    cc |= NVME_CC_EN_ENABLE;
    nvme_write_reg32(ctrl->reg_base, NVME_REG_CC, cc);
    nvme_wmb();

    int ret = nvme_wait_csts_rdy(ctrl, 1, NVME_TIMEOUT_READY);
    if (ret < 0) {
        if (ret == -2) {
            ctrl->state = NVME_CTRL_STATE_FAILED;
            return -6;
        }
        return -7;
    }

    ret = nvme_identify_controller(ctrl);
    if (ret != 0) {
        ctrl->state = NVME_CTRL_STATE_FAILED;
        return -8;
    }

    ctrl->active_namespaces = 0;
    for (uint32_t nsid = 1; nsid <= NVME_MAX_NAMESPACES; nsid++) {
        nvme_ns_t ns;
        if (nvme_identify_namespace(ctrl, nsid, &ns) == 0) {
            if (ns.active && ns.nsze > 0) {
                ctrl->namespaces[nsid - 1] = ns;
                ctrl->active_namespaces++;
            }
        }
    }

    ctrl->state = NVME_CTRL_STATE_READY;
    return 0;
}

int nvme_identify_controller(nvme_controller_t *ctrl)
{

    uint64_t ident_phys;
    nvme_identify_t *ident_buf = (nvme_identify_t *)nvme_alloc_queue(
        sizeof(nvme_identify_t), &ident_phys);
    if (!ident_buf) {
        return -1;
    }
    memset(ident_buf, 0, sizeof(nvme_identify_t));

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_OPC_IDENTIFY;
    cmd.nsid   = 0;
    cmd.prp1   = ident_phys;
    cmd.cdw10  = NVME_IDENTIFY_CNS_CTRL;

    int ret = nvme_submit_admin_cmd(ctrl, &cmd);

    if (ret == 0) {

        memcpy(&ctrl->identify_data, ident_buf, sizeof(nvme_identify_t));
    }

    nvme_free_queue(ident_buf, sizeof(nvme_identify_t));
    return ret;
}

int nvme_identify_namespace(nvme_controller_t *ctrl, uint32_t ns_id, nvme_ns_t *ns)
{
    if (!ns) return -1;
    if (ns_id == 0 || ns_id > NVME_MAX_NAMESPACES) return -2;

    uint64_t ident_phys;
    nvme_ns_identify_t *ident_buf = (nvme_ns_identify_t *)nvme_alloc_queue(
        sizeof(nvme_ns_identify_t), &ident_phys);
    if (!ident_buf) {
        return -3;
    }
    memset(ident_buf, 0, sizeof(nvme_ns_identify_t));

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_OPC_IDENTIFY;
    cmd.nsid   = ns_id;
    cmd.prp1   = ident_phys;
    cmd.cdw10  = NVME_IDENTIFY_CNS_NS;

    int ret = nvme_submit_admin_cmd(ctrl, &cmd);

    if (ret == 0) {

        memset(ns, 0, sizeof(nvme_ns_t));
        ns->nsid  = ns_id;
        ns->nsze  = ident_buf->nsze;
        ns->ncap  = ident_buf->ncap;
        ns->nlbaf = ident_buf->nlbaf + 1;
        ns->flbas = NVME_FLBAS_FORMAT(ident_buf->flbas);
        uint8_t fmt_idx = ns->flbas;
        if (fmt_idx <= ident_buf->nlbaf) {

            uint8_t *lba_fmt_base = (uint8_t *)ident_buf + 128;
            nvme_lba_format_t *fmt = (nvme_lba_format_t *)
                (lba_fmt_base + fmt_idx * sizeof(nvme_lba_format_t));
            ns->lbads = fmt->lbads;
        }

        ns->block_size = (uint32_t)NVME_LBA_SIZE(ns->lbads);
        ns->total_size = ns->nsze * ns->block_size;
        ns->active = 1;
    } else {
        memset(ns, 0, sizeof(nvme_ns_t));
        ns->active = 0;
    }

    nvme_free_queue(ident_buf, sizeof(nvme_ns_identify_t));
    return ret;
}

int nvme_create_io_queues(nvme_controller_t *ctrl)
{
    if (ctrl->io_queues_created) {
        return 0;
    }

    uint32_t io_sq_size = NVME_IO_QUEUE_DEPTH * sizeof(nvme_cmd_t);
    uint32_t io_cq_size = NVME_IO_QUEUE_DEPTH * sizeof(nvme_cqe_t);

    ctrl->io_sq.depth = NVME_IO_QUEUE_DEPTH;
    ctrl->io_sq.sqid  = NVME_IO_SQ_ID;
    ctrl->io_sq.head  = 0;
    ctrl->io_sq.tail  = 0;

    ctrl->io_sq.base = (nvme_cmd_t *)nvme_alloc_queue(
        io_sq_size, &ctrl->io_sq.base_phys);
    if (!ctrl->io_sq.base) {
        return -1;
    }
    memset(ctrl->io_sq.base, 0, io_sq_size);
    spin_init(&ctrl->io_sq.lock);

    ctrl->io_cq.depth = NVME_IO_QUEUE_DEPTH;
    ctrl->io_cq.cqid  = NVME_IO_CQ_ID;
    ctrl->io_cq.head  = 0;
    ctrl->io_cq.tail  = 0;
    ctrl->io_cq.phase = 1;

    ctrl->io_cq.base = (nvme_cqe_t *)nvme_alloc_queue(
        io_cq_size, &ctrl->io_cq.base_phys);
    if (!ctrl->io_cq.base) {
        nvme_free_queue(ctrl->io_sq.base, io_sq_size);
        return -2;
    }
    memset(ctrl->io_cq.base, 0, io_cq_size);
    spin_init(&ctrl->io_cq.lock);

    nvme_cmd_t cq_cmd;
    memset(&cq_cmd, 0, sizeof(cq_cmd));

    cq_cmd.opcode = NVME_OPC_CREATE_IO_CQ;
    cq_cmd.prp1   = ctrl->io_cq.base_phys;

    cq_cmd.cdw10  = ((NVME_IO_QUEUE_DEPTH - 1) << 16) | NVME_IO_CQ_ID;

    cq_cmd.cdw11  = (1 << 0);

    int ret = nvme_submit_admin_cmd(ctrl, &cq_cmd);
    if (ret != 0) {
        nvme_free_queue(ctrl->io_cq.base, io_cq_size);
        nvme_free_queue(ctrl->io_sq.base, io_sq_size);
        return -3;
    }

    nvme_cmd_t sq_cmd;
    memset(&sq_cmd, 0, sizeof(sq_cmd));

    sq_cmd.opcode = NVME_OPC_CREATE_IO_SQ;
    sq_cmd.prp1   = ctrl->io_sq.base_phys;

    sq_cmd.cdw10  = ((NVME_IO_QUEUE_DEPTH - 1) << 16) | NVME_IO_SQ_ID;

    sq_cmd.cdw11  = (1 << 0) | ((uint32_t)NVME_IO_CQ_ID << 16);

    ret = nvme_submit_admin_cmd(ctrl, &sq_cmd);
    if (ret != 0) {
        nvme_free_queue(ctrl->io_cq.base, io_cq_size);
        nvme_free_queue(ctrl->io_sq.base, io_sq_size);
        return -4;
    }

    uint32_t cc = nvme_read_reg32(ctrl->reg_base, NVME_REG_CC);
    cc &= ~NVME_CC_IOSQES_MASK;
    cc &= ~NVME_CC_IOCQES_MASK;
    cc |= (0 << NVME_CC_IOSQES_SHIFT);
    cc |= (0 << NVME_CC_IOCQES_SHIFT);
    nvme_write_reg32(ctrl->reg_base, NVME_REG_CC, cc);
    nvme_wmb();

    ctrl->io_queues_created = 1;
    return 0;
}

int nvme_read(nvme_controller_t *ctrl, uint32_t ns_id, uint64_t lba,
              uint32_t count, void *buffer)
{
    if (!ctrl || !buffer) return -1;
    if (count == 0) return 0;

    if (!ctrl->io_queues_created) {
        int ret = nvme_create_io_queues(ctrl);
        if (ret != 0) return -2;
    }

    if (ns_id < 1 || ns_id > NVME_MAX_NAMESPACES ||
        !ctrl->namespaces[ns_id - 1].active) {
        return -3;
    }

    uint64_t buf_phys = (uint64_t)(uintptr_t)buffer;

    if (buf_phys & (ctrl->mps - 1)) {
        return -4;
    }

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_OPC_READ;
    cmd.nsid   = ns_id;
    cmd.prp1   = buf_phys;
    cmd.cdw10  = (uint32_t)(lba & 0xFFFFFFFF);
    cmd.cdw11  = (uint32_t)(lba >> 32);
    cmd.cdw12  = count - 1;

    cmd.cid = (uint16_t)(ctrl->io_cid & 0xFFFF);
    ctrl->io_cid++;

    if (nvme_submit_cmd(&ctrl->io_sq, &cmd) != 0) {
        return -5;
    }

    nvme_ring_sq_doorbell(ctrl, &ctrl->io_sq);

    nvme_cqe_t cqe;
    uint32_t elapsed = 0;

    while (elapsed < NVME_TIMEOUT_CMD) {
        if (nvme_poll_cq(&ctrl->io_cq, &cqe) == 0) {
            nvme_ring_cq_doorbell(ctrl, &ctrl->io_cq);

            if (NVME_CQE_SC(cqe.status) != NVME_SC_SUCCESS) {
                return -6;
            }
            return 0;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    return -7;
}

int nvme_write(nvme_controller_t *ctrl, uint32_t ns_id, uint64_t lba,
               uint32_t count, const void *buffer)
{
    if (!ctrl || !buffer) return -1;
    if (count == 0) return 0;

    if (!ctrl->io_queues_created) {
        int ret = nvme_create_io_queues(ctrl);
        if (ret != 0) return -2;
    }

    if (ns_id < 1 || ns_id > NVME_MAX_NAMESPACES ||
        !ctrl->namespaces[ns_id - 1].active) {
        return -3;
    }

    uint64_t buf_phys = (uint64_t)(uintptr_t)buffer;

    if (buf_phys & (ctrl->mps - 1)) {
        return -4;
    }

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_OPC_WRITE;
    cmd.nsid   = ns_id;
    cmd.prp1   = buf_phys;
    cmd.cdw10  = (uint32_t)(lba & 0xFFFFFFFF);
    cmd.cdw11  = (uint32_t)(lba >> 32);
    cmd.cdw12  = count - 1;

    cmd.cid = (uint16_t)(ctrl->io_cid & 0xFFFF);
    ctrl->io_cid++;

    if (nvme_submit_cmd(&ctrl->io_sq, &cmd) != 0) {
        return -5;
    }

    nvme_ring_sq_doorbell(ctrl, &ctrl->io_sq);

    nvme_cqe_t cqe;
    uint32_t elapsed = 0;

    while (elapsed < NVME_TIMEOUT_CMD) {
        if (nvme_poll_cq(&ctrl->io_cq, &cqe) == 0) {
            nvme_ring_cq_doorbell(ctrl, &ctrl->io_cq);

            if (NVME_CQE_SC(cqe.status) != NVME_SC_SUCCESS) {
                return -6;
            }
            return 0;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    return -7;
}

int nvme_flush(nvme_controller_t *ctrl, uint32_t ns_id)
{
    if (!ctrl) return -1;

    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.opcode = NVME_OPC_FLUSH;
    cmd.nsid   = ns_id;

    cmd.cid = (uint16_t)(ctrl->io_cid & 0xFFFF);
    ctrl->io_cid++;

    if (!ctrl->io_queues_created) {
        int ret = nvme_create_io_queues(ctrl);
        if (ret != 0) return -2;
    }

    if (nvme_submit_cmd(&ctrl->io_sq, &cmd) != 0) {
        return -3;
    }

    nvme_ring_sq_doorbell(ctrl, &ctrl->io_sq);

    nvme_cqe_t cqe;
    uint32_t elapsed = 0;

    while (elapsed < NVME_TIMEOUT_CMD) {
        if (nvme_poll_cq(&ctrl->io_cq, &cqe) == 0) {
            nvme_ring_cq_doorbell(ctrl, &ctrl->io_cq);

            if (NVME_CQE_SC(cqe.status) != NVME_SC_SUCCESS) {
                return -4;
            }
            return 0;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    return -5;
}

int nvme_shutdown(nvme_controller_t *ctrl)
{
    if (!ctrl || !ctrl->reg_base) {
        return -1;
    }

    ctrl->state = NVME_CTRL_STATE_SHUTDOWN;

    uint32_t cc = nvme_read_reg32(ctrl->reg_base, NVME_REG_CC);
    cc &= ~NVME_CC_SHN_MASK;
    cc |= NVME_CC_SHN_NORMAL;
    nvme_write_reg32(ctrl->reg_base, NVME_REG_CC, cc);
    nvme_wmb();

    uint32_t elapsed = 0;
    while (elapsed < NVME_TIMEOUT_SHUTDOWN) {
        uint32_t csts = nvme_read_reg32(ctrl->reg_base, NVME_REG_CSTS);
        uint32_t shst = csts & NVME_CSTS_SHST_MASK;

        if (shst == NVME_CSTS_SHST_COMP) {

            break;
        }

        if (csts & NVME_CSTS_CFS_MASK) {
            ctrl->state = NVME_CTRL_STATE_FAILED;
            return -2;
        }

        nvme_delay_us(NVME_TIMEOUT_POLL_US);
        elapsed += NVME_TIMEOUT_POLL_US;
    }

    if (elapsed >= NVME_TIMEOUT_SHUTDOWN) {
        return -3;
    }

    cc = nvme_read_reg32(ctrl->reg_base, NVME_REG_CC);
    cc &= ~NVME_CC_EN_MASK;
    nvme_write_reg32(ctrl->reg_base, NVME_REG_CC, cc);
    nvme_wmb();

    ctrl->state = NVME_CTRL_STATE_RESET;

    if (ctrl->io_queues_created) {
        uint32_t io_sq_size = NVME_IO_QUEUE_DEPTH * sizeof(nvme_cmd_t);
        uint32_t io_cq_size = NVME_IO_QUEUE_DEPTH * sizeof(nvme_cqe_t);
        nvme_free_queue(ctrl->io_sq.base, io_sq_size);
        nvme_free_queue(ctrl->io_cq.base, io_cq_size);
        ctrl->io_queues_created = 0;
    }

    uint32_t admin_sq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cmd_t);
    uint32_t admin_cq_size = NVME_ADMIN_QUEUE_DEPTH * sizeof(nvme_cqe_t);
    nvme_free_queue(ctrl->admin_sq.base, admin_sq_size);
    nvme_free_queue(ctrl->admin_cq.base, admin_cq_size);

    return 0;
}

int nvme_get_info(nvme_controller_t *ctrl)
{
    if (!ctrl) return -1;

    uint32_t vs = ctrl->version;
    uint16_t major = (vs >> 16) & 0xFFFF;
    uint16_t minor = (vs >> 8) & 0xFF;
    uint8_t  tertiary = vs & 0xFF;

    (void)major;
    (void)minor;
    (void)tertiary;

    return 0;
}

nvme_ns_t *nvme_get_namespace(nvme_controller_t *ctrl, uint32_t ns_id)
{
    if (!ctrl || ns_id < 1 || ns_id > NVME_MAX_NAMESPACES) {
        return NULL;
    }

    if (ctrl->namespaces[ns_id - 1].active) {
        return &ctrl->namespaces[ns_id - 1];
    }

    return NULL;
}

int nvme_driver_init(void)
{

    int found = nvme_probe();
    if (found == 0) {

        return -1;
    }

    int ret = nvme_init(&nvme_ctrl);
    if (ret != 0) {
        return ret;
    }

    nvme_get_info(&nvme_ctrl);

    return 0;
}
