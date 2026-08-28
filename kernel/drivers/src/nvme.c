#include "nvme.h"
#include <arch/memory.h>
#include <string.h>

static volatile uint32_t* nvme_sq_doorbell(nvme_ctrl_t* ctrl, uint16_t qid)
{
    return (volatile uint32_t*)((uintptr_t)ctrl->regs + 0x1000 + (2 * qid) * ctrl->doorbell_stride * 4);
}

static volatile uint32_t* nvme_cq_doorbell(nvme_ctrl_t* ctrl, uint16_t qid)
{
    return (volatile uint32_t*)((uintptr_t)ctrl->regs + 0x1000 + (2 * qid + 1) * ctrl->doorbell_stride * 4);
}

static void nvme_wait_ready(nvme_ctrl_t* ctrl, int wait_for, uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < timeout_ms * 1000; i++) {
        uint32_t csts = ctrl->regs[NVME_REG_CSTS / 4];
        if (wait_for) {
            if (csts & NVME_CSTS_RDY) return;
        } else {
            if (!(csts & NVME_CSTS_RDY)) return;
        }
        for (volatile int d = 0; d < 1000; d++);
    }
}

static uint16_t nvme_next_cmd_id(nvme_ctrl_t* ctrl)
{
    return ctrl->cmd_id_counter++;
}

static int nvme_submit_admin(nvme_ctrl_t* ctrl, nvme_sqe_t* cmd)
{
    if (!ctrl || !cmd) return -1;
    nvme_queue_t* q = &ctrl->admin_q;

    spinlock_acquire(&q->lock);
    uint32_t tail = q->sq_tail;
    q->sq_entries[tail] = *cmd;
    tail = (tail + 1) % q->depth;
    q->sq_tail = tail;
    *nvme_sq_doorbell(ctrl, 0) = tail;
    spinlock_release(&q->lock);
    return 0;
}

static nvme_cqe_t nvme_wait_cq(nvme_ctrl_t* ctrl, nvme_queue_t* q, uint16_t cmd_id)
{
    nvme_cqe_t empty;
    memset(&empty, 0, sizeof(empty));

    for (int i = 0; i < 10000000; i++) {
        nvme_cqe_t* cqe = &q->cq_entries[q->cq_head];
        if (cqe->command_id == cmd_id && (cqe->status & 0x1)) {
            nvme_cqe_t result = *cqe;
            uint32_t new_head = (q->cq_head + 1) % q->depth;
            q->cq_head = new_head;
            *nvme_cq_doorbell(ctrl, q->cq_id) = new_head;
            return result;
        }
    }
    return empty;
}

int nvme_admin_cmd(nvme_ctrl_t* ctrl, nvme_sqe_t* cmd, nvme_cqe_t* result)
{
    if (!ctrl || !cmd) return -1;
    cmd->command_id = nvme_next_cmd_id(ctrl);
    int ret = nvme_submit_admin(ctrl, cmd);
    if (ret != 0) return ret;

    nvme_cqe_t cqe = nvme_wait_cq(ctrl, &ctrl->admin_q, cmd->command_id);
    if (result) *result = cqe;
    return (cqe->status >> 1) & 0xFF;
}

int nvme_identify_ctrl(nvme_ctrl_t* ctrl, void* buf)
{
    if (!ctrl || !buf) return -1;

    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CMD_IDENTIFY;
    cmd.nsid = 0;
    cmd.prp1 = (uint64_t)(uintptr_t)buf;
    cmd.cdw10 = 1;

    nvme_cqe_t result;
    return nvme_admin_cmd(ctrl, &cmd, &result);
}

int nvme_identify_ns(nvme_ctrl_t* ctrl, uint32_t nsid, void* buf)
{
    if (!ctrl || !buf) return -1;

    nvme_sqe_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CMD_IDENTIFY;
    cmd.nsid = nsid;
    cmd.prp1 = (uint64_t)(uintptr_t)buf;
    cmd.cdw10 = 0;

    nvme_cqe_t result;
    return nvme_admin_cmd(ctrl, &cmd, &result);
}

int nvme_create_io_queue(nvme_ctrl_t* ctrl, int qid, int depth)
{
    if (!ctrl || qid <= 0 || qid >= NVME_MAX_QUEUES) return -1;

    nvme_queue_t* q = &ctrl->io_queues[qid];
    memset(q, 0, sizeof(nvme_queue_t));
    spin_init(&q->lock);
    q->depth = depth;
    q->sq_id = qid;
    q->cq_id = qid;

    q->sq_entries = (nvme_sqe_t*)memory_alloc_aligned(depth * sizeof(nvme_sqe_t), 4096);
    q->cq_entries = (nvme_cqe_t*)memory_alloc_aligned(depth * sizeof(nvme_cqe_t), 4096);
    if (!q->sq_entries || !q->cq_entries) return -2;

    q->sq_phys = (uint64_t)(uintptr_t)q->sq_entries;
    q->cq_phys = (uint64_t)(uintptr_t)q->cq_entries;

    nvme_sqe_t cmd;
    nvme_cqe_t result;

    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CMD_CREATE_CQ;
    cmd.prp1 = q->cq_phys;
    cmd.cdw10 = (uint32_t)((qid << 16) | (depth - 1));
    cmd.cdw11 = 0x03;
    int ret = nvme_admin_cmd(ctrl, &cmd, &result);
    if (ret != 0) return -3;

    memset(&cmd, 0, sizeof(cmd));
    cmd.opcode = NVME_ADMIN_CMD_CREATE_SQ;
    cmd.prp1 = q->sq_phys;
    cmd.cdw10 = (uint32_t)((qid << 16) | (depth - 1));
    cmd.cdw11 = (uint32_t)((qid << 16) | 0x01);
    ret = nvme_admin_cmd(ctrl, &cmd, &result);
    if (ret != 0) return -4;

    ctrl->io_queue_count++;
    return 0;
}

int nvme_read(nvme_ctrl_t* ctrl, uint32_t nsid, uint64_t lba, void* buf, uint32_t count)
{
    if (!ctrl || !buf || ctrl->io_queue_count == 0) return -1;

    nvme_queue_t* q = &ctrl->io_queues[1];
    spinlock_acquire(&q->lock);

    uint32_t tail = q->sq_tail;
    nvme_sqe_t* cmd = &q->sq_entries[tail];
    memset(cmd, 0, sizeof(nvme_sqe_t));
    cmd->opcode = NVME_CMD_READ;
    cmd->nsid = nsid;
    cmd->command_id = nvme_next_cmd_id(ctrl);
    cmd->prp1 = (uint64_t)(uintptr_t)buf;
    cmd->cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
    cmd->cdw11 = (uint32_t)((lba >> 32) & 0xFFFFFFFF);
    cmd->cdw12 = (count - 1) & 0xFFFF;

    tail = (tail + 1) % q->depth;
    q->sq_tail = tail;
    *nvme_sq_doorbell(ctrl, q->sq_id) = tail;

    nvme_cqe_t cqe = nvme_wait_cq(ctrl, q, cmd->command_id);
    spinlock_release(&q->lock);

    uint16_t sc = (cqe.status >> 1) & 0xFF;
    return sc == 0 ? (int)(count * 512) : -(int)sc;
}

int nvme_write(nvme_ctrl_t* ctrl, uint32_t nsid, uint64_t lba, const void* buf, uint32_t count)
{
    if (!ctrl || !buf || ctrl->io_queue_count == 0) return -1;

    nvme_queue_t* q = &ctrl->io_queues[1];
    spinlock_acquire(&q->lock);

    uint32_t tail = q->sq_tail;
    nvme_sqe_t* cmd = &q->sq_entries[tail];
    memset(cmd, 0, sizeof(nvme_sqe_t));
    cmd->opcode = NVME_CMD_WRITE;
    cmd->nsid = nsid;
    cmd->command_id = nvme_next_cmd_id(ctrl);
    cmd->prp1 = (uint64_t)(uintptr_t)buf;
    cmd->cdw10 = (uint32_t)(lba & 0xFFFFFFFF);
    cmd->cdw11 = (uint32_t)((lba >> 32) & 0xFFFFFFFF);
    cmd->cdw12 = (count - 1) & 0xFFFF;

    tail = (tail + 1) % q->depth;
    q->sq_tail = tail;
    *nvme_sq_doorbell(ctrl, q->sq_id) = tail;

    nvme_cqe_t cqe = nvme_wait_cq(ctrl, q, cmd->command_id);
    spinlock_release(&q->lock);

    uint16_t sc = (cqe.status >> 1) & 0xFF;
    return sc == 0 ? (int)(count * 512) : -(int)sc;
}

int nvme_flush(nvme_ctrl_t* ctrl, uint32_t nsid)
{
    if (!ctrl || ctrl->io_queue_count == 0) return -1;

    nvme_queue_t* q = &ctrl->io_queues[1];
    spinlock_acquire(&q->lock);

    uint32_t tail = q->sq_tail;
    nvme_sqe_t* cmd = &q->sq_entries[tail];
    memset(cmd, 0, sizeof(nvme_sqe_t));
    cmd->opcode = NVME_CMD_FLUSH;
    cmd->nsid = nsid;
    cmd->command_id = nvme_next_cmd_id(ctrl);

    tail = (tail + 1) % q->depth;
    q->sq_tail = tail;
    *nvme_sq_doorbell(ctrl, q->sq_id) = tail;

    nvme_cqe_t cqe = nvme_wait_cq(ctrl, q, cmd->command_id);
    spinlock_release(&q->lock);

    return (int)((cqe.status >> 1) & 0xFF);
}

int nvme_init(nvme_ctrl_t* ctrl, pci_device_t* pci_dev)
{
    if (!ctrl || !pci_dev) return -1;

    memset(ctrl, 0, sizeof(nvme_ctrl_t));
    spin_init(&ctrl->lock);
    ctrl->pci_dev = pci_dev;

    pci_enable_device(pci_dev);
    pci_set_master(pci_dev);

    ctrl->regs = (volatile uint32_t*)pci_map_bar(pci_dev, 0);
    if (!ctrl->regs) return -2;

    ctrl->cap = (uint64_t)ctrl->regs[NVME_REG_CAP / 4] |
                ((uint64_t)ctrl->regs[NVME_REG_CAP / 4 + 1] << 32);
    ctrl->vs = ctrl->regs[NVME_REG_VS / 4];

    ctrl->max_queue_entries = (int)((ctrl->cap >> NVME_CAP_MQES_SHIFT) & 0xFFFF) + 1;
    ctrl->doorbell_stride = (int)((ctrl->cap >> NVME_CAP_DSTRD_SHIFT) & 0xF);
    ctrl->mps_min = (int)((ctrl->cap >> NVME_CAP_MPSMIN_SHIFT) & 0xF);
    ctrl->mps_max = (int)((ctrl->cap >> NVME_CAP_MPSMAX_SHIFT) & 0xF);

    uint32_t cc = ctrl->regs[NVME_REG_CC / 4];
    if (cc & NVME_CC_EN) {
        ctrl->regs[NVME_REG_CC / 4] = cc & ~NVME_CC_EN;
        nvme_wait_ready(ctrl, 0, 5000);
    }

    cc = 0;
    cc |= (0 << NVME_CC_CSS_SHIFT);
    cc |= (0 << NVME_CC_MPS_SHIFT);
    cc |= (6 << NVME_CC_IOSQES_SHIFT);
    cc |= (4 << NVME_CC_IOCQES_SHIFT);
    ctrl->regs[NVME_REG_CC / 4] = cc;

    nvme_queue_t* admin_q = &ctrl->admin_q;
    memset(admin_q, 0, sizeof(nvme_queue_t));
    spin_init(&admin_q->lock);
    admin_q->depth = NVME_QUEUE_DEPTH;
    admin_q->sq_id = 0;
    admin_q->cq_id = 0;
    admin_q->sq_entries = (nvme_sqe_t*)memory_alloc_aligned(NVME_QUEUE_DEPTH * sizeof(nvme_sqe_t), 4096);
    admin_q->cq_entries = (nvme_cqe_t*)memory_alloc_aligned(NVME_QUEUE_DEPTH * sizeof(nvme_cqe_t), 4096);
    if (!admin_q->sq_entries || !admin_q->cq_entries) return -3;

    admin_q->sq_phys = (uint64_t)(uintptr_t)admin_q->sq_entries;
    admin_q->cq_phys = (uint64_t)(uintptr_t)admin_q->cq_entries;

    ctrl->regs[NVME_REG_AQA / 4] = ((NVME_QUEUE_DEPTH - 1) << 16) | (NVME_QUEUE_DEPTH - 1);
    ctrl->regs[NVME_REG_ASQ / 4] = (uint32_t)admin_q->sq_phys;
    *((volatile uint32_t*)((uintptr_t)ctrl->regs + NVME_REG_ASQ + 4)) = (uint32_t)(admin_q->sq_phys >> 32);
    ctrl->regs[NVME_REG_ACQ / 4] = (uint32_t)admin_q->cq_phys;
    *((volatile uint32_t*)((uintptr_t)ctrl->regs + NVME_REG_ACQ + 4)) = (uint32_t)(admin_q->cq_phys >> 32);

    cc = ctrl->regs[NVME_REG_CC / 4];
    cc |= NVME_CC_EN;
    ctrl->regs[NVME_REG_CC / 4] = cc;

    nvme_wait_ready(ctrl, 1, 5000);

    uint8_t identify_buf[4096];
    int result = nvme_identify_ctrl(ctrl, identify_buf);
    if (result != 0) return -4;

    uint32_t nn = *(uint32_t*)(identify_buf + 252);
    if (nn > NVME_MAX_NAMESPACES) nn = NVME_MAX_NAMESPACES;

    for (uint32_t i = 1; i <= nn; i++) {
        result = nvme_identify_ns(ctrl, i, identify_buf);
        if (result != 0) continue;

        nvme_namespace_t* ns = &ctrl->namespaces[ctrl->ns_count];
        ns->nsid = i;
        ns->nsze = *(uint64_t*)(identify_buf + 0);
        ns->ncap = *(uint64_t*)(identify_buf + 8);
        ns->flbas = *(uint32_t*)(identify_buf + 24);
        ns->lbads = (ns->flbas >> 16) & 0xFF;
        ns->sector_size = 1 << ns->lbads;
        ns->sector_count = ns->nsze;
        ns->dps = *(uint32_t*)(identify_buf + 28);
        ns->nmic = *(uint32_t*)(identify_buf + 32);
        ns->eui64 = *(uint64_t*)(identify_buf + 120);
        memcpy(ns->ns_guid, identify_buf + 104, 16);

        if (ns->ncap > 0) ctrl->ns_count++;
    }

    if (ctrl->ns_count > 0) {
        nvme_create_io_queue(ctrl, 1, NVME_QUEUE_DEPTH);
    }

    return 0;
}

void nvme_shutdown(nvme_ctrl_t* ctrl)
{
    if (!ctrl) return;
    uint32_t cc = ctrl->regs[NVME_REG_CC / 4];
    cc |= (2 << NVME_CC_SHN_SHIFT);
    ctrl->regs[NVME_REG_CC / 4] = cc;
    nvme_wait_ready(ctrl, 0, 5000);
}

void nvme_irq_handler(int irq, void* ctx)
{
    (void)irq; (void)ctx;
}