#include <arch/tpm.h>
#include <arch/memory.h>
#include <arch/io.h>
#include <string.h>

static tpm_t tpm_device;

static inline uint8_t tpm_read8(tpm_t* dev, uint32_t reg)
{
    volatile uint8_t* ptr = (volatile uint8_t*)(dev->base_addr + reg);
    return *ptr;
}

static inline void tpm_write8(tpm_t* dev, uint32_t reg, uint8_t val)
{
    volatile uint8_t* ptr = (volatile uint8_t*)(dev->base_addr + reg);
    *ptr = val;
}

static inline uint32_t tpm_read32(tpm_t* dev, uint32_t reg)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->base_addr + reg);
    return *ptr;
}

static inline void tpm_write32(tpm_t* dev, uint32_t reg, uint32_t val)
{
    volatile uint32_t* ptr = (volatile uint32_t*)(dev->base_addr + reg);
    *ptr = val;
}

static int tpm_fifo_wait_status(tpm_t* dev, uint8_t mask, uint8_t value, int timeout_ms)
{
    for (volatile int i = 0; i < timeout_ms * 10000; i++) {
        uint8_t sts = tpm_read8(dev, TPM_FIFO_STS + dev->locality * 0x1000);
        if ((sts & mask) == value) return 0;
    }
    return -1;
}

static int tpm_fifo_send(tpm_t* dev, const uint8_t* data, uint32_t size)
{
    if (!dev || !data || size == 0 || size > TPM_MAX_BUFFER_SIZE) return -1;

    uint32_t base = dev->locality * 0x1000;
    uint8_t access = tpm_read8(dev, TPM_FIFO_ACCESS + base);
    if (!(access & TPM_ACCESS_ACTIVE_LOCALITY)) {
        tpm_write8(dev, TPM_FIFO_ACCESS + base, TPM_ACCESS_REQUEST_USE);
        for (volatile int i = 0; i < 1000000; i++) {
            access = tpm_read8(dev, TPM_FIFO_ACCESS + base);
            if (access & TPM_ACCESS_ACTIVE_LOCALITY) break;
        }
        if (!(access & TPM_ACCESS_ACTIVE_LOCALITY)) return -2;
    }

    if (tpm_fifo_wait_status(dev, TPM_STS_CMD_READY, TPM_STS_CMD_READY, 100) != 0) {
        tpm_write8(dev, TPM_FIFO_STS + base, TPM_STS_CMD_READY);
        if (tpm_fifo_wait_status(dev, TPM_STS_CMD_READY, TPM_STS_CMD_READY, 100) != 0)
            return -3;
    }

    uint32_t burst_count = 0;
    for (uint32_t offset = 0; offset < size; ) {
        uint8_t sts = tpm_read8(dev, TPM_FIFO_STS + base);
        burst_count = (uint32_t)((sts >> 8) & 0xFF);
        if (burst_count == 0) {
            for (volatile int i = 0; i < 10000; i++) {
                sts = tpm_read8(dev, TPM_FIFO_STS + base);
                burst_count = (uint32_t)((sts >> 8) & 0xFF);
                if (burst_count > 0) break;
            }
            if (burst_count == 0) return -4;
        }

        uint32_t count = burst_count;
        if (count > size - offset) count = size - offset;

        for (uint32_t i = 0; i < count; i++) {
            tpm_write8(dev, TPM_FIFO_DATA_FIFO + base, data[offset + i]);
        }
        offset += count;
    }

    tpm_write8(dev, TPM_FIFO_STS + base, TPM_STS_TPM_GO);

    return 0;
}

static int tpm_fifo_recv(tpm_t* dev, uint8_t* data, uint32_t* size)
{
    if (!dev || !data || !size) return -1;

    uint32_t base = dev->locality * 0x1000;

    if (tpm_fifo_wait_status(dev, TPM_STS_DATA_AVAIL, TPM_STS_DATA_AVAIL, 5000) != 0)
        return -2;

    uint32_t offset = 0;
    uint32_t max_size = *size;

    while (1) {
        uint8_t sts = tpm_read8(dev, TPM_FIFO_STS + base);
        if (!(sts & TPM_STS_DATA_AVAIL)) break;

        uint32_t burst_count = (uint32_t)((sts >> 8) & 0xFF);
        if (burst_count == 0) {
            for (volatile int i = 0; i < 10000; i++) {
                sts = tpm_read8(dev, TPM_FIFO_STS + base);
                burst_count = (uint32_t)((sts >> 8) & 0xFF);
                if (burst_count > 0) break;
            }
            if (burst_count == 0) break;
        }

        for (uint32_t i = 0; i < burst_count && offset < max_size; i++) {
            data[offset++] = tpm_read8(dev, TPM_FIFO_DATA_FIFO + base);
        }
    }

    *size = offset;
    return 0;
}

static int tpm_crb_send(tpm_t* dev, const uint8_t* data, uint32_t size)
{
    if (!dev || !data || size == 0 || size > dev->cmd_size) return -1;

    uint32_t ctrl_sts = tpm_read32(dev, TPM_CRB_CTRL_STS);
    if (!(ctrl_sts & TPM_CRB_CTRL_STS_IDLE)) {
        tpm_write32(dev, TPM_CRB_CTRL_REQ, TPM_CRB_CTRL_REQ_GO_IDLE);
        for (volatile int i = 0; i < 1000000; i++) {
            ctrl_sts = tpm_read32(dev, TPM_CRB_CTRL_STS);
            if (ctrl_sts & TPM_CRB_CTRL_STS_IDLE) break;
        }
    }

    tpm_write32(dev, TPM_CRB_CTRL_REQ, TPM_CRB_CTRL_REQ_CMD_READY);
    for (volatile int i = 0; i < 1000000; i++) {
        ctrl_sts = tpm_read32(dev, TPM_CRB_CTRL_STS);
        if (!(ctrl_sts & TPM_CRB_CTRL_STS_IDLE)) break;
    }

    memcpy((void*)dev->cmd_addr, data, size);

    tpm_write32(dev, TPM_CRB_CTRL_START, TPM_CRB_CTRL_START_START);
    return 0;
}

static int tpm_crb_recv(tpm_t* dev, uint8_t* data, uint32_t* size)
{
    if (!dev || !data || !size) return -1;

    for (volatile int i = 0; i < 50000000; i++) {
        uint32_t start = tpm_read32(dev, TPM_CRB_CTRL_START);
        if ((start & TPM_CRB_CTRL_START_START) == 0) break;
    }

    uint32_t rsp_size = tpm_read32(dev, TPM_CRB_CTRL_RSP_SIZE);
    if (rsp_size > dev->rsp_size || rsp_size > *size) return -2;

    memcpy(data, (const void*)dev->rsp_addr, rsp_size);
    *size = rsp_size;
    return 0;
}

void tpm_init(void)
{
    memset(&tpm_device, 0, sizeof(tpm_t));
    spin_init(&tpm_device.lock);

    tpm_device.base_addr = TPM_FIFO_BASE;
    tpm_device.locality = 0;
    tpm_device.interface_type = TPM_INTERFACE_FIFO;

    uint8_t access = tpm_read8(&tpm_device, TPM_FIFO_ACCESS);
    if (!(access & TPM_ACCESS_VALID)) {
        tpm_device.base_addr = 0xFED40000;
        access = tpm_read8(&tpm_device, TPM_FIFO_ACCESS);
        if (!(access & TPM_ACCESS_VALID)) {
            return;
        }
    }

    tpm_device.interface_type = TPM_INTERFACE_FIFO;
    tpm_detect_version(&tpm_device);

    tpm_device.initialized = 1;
}

tpm_t* tpm_get_device(void)
{
    return tpm_device.initialized ? &tpm_device : NULL;
}

int tpm_request_locality(tpm_t* dev, uint32_t locality)
{
    if (!dev || locality > 4) return -1;

    if (dev->interface_type == TPM_INTERFACE_FIFO) {
        uint32_t base = locality * 0x1000;
        tpm_write8(dev, TPM_FIFO_ACCESS + base, TPM_ACCESS_REQUEST_USE);
        for (volatile int i = 0; i < 1000000; i++) {
            uint8_t access = tpm_read8(dev, TPM_FIFO_ACCESS + base);
            if (access & TPM_ACCESS_ACTIVE_LOCALITY) {
                dev->locality = locality;
                return 0;
            }
        }
        return -2;
    }

    dev->locality = locality;
    return 0;
}

void tpm_release_locality(tpm_t* dev, uint32_t locality)
{
    if (!dev) return;
    if (dev->interface_type == TPM_INTERFACE_FIFO) {
        uint32_t base = locality * 0x1000;
        tpm_write8(dev, TPM_FIFO_ACCESS + base, TPM_ACCESS_ACTIVE_LOCALITY);
    }
    dev->locality = 0;
}

int tpm_send_command(tpm_t* dev, const uint8_t* cmd, uint32_t cmd_size,
                     uint8_t* rsp, uint32_t* rsp_size)
{
    if (!dev || !dev->initialized || !cmd || !rsp || !rsp_size) return -1;

    spinlock_acquire(&dev->lock);

    int result;
    if (dev->interface_type == TPM_INTERFACE_FIFO) {
        result = tpm_fifo_send(dev, cmd, cmd_size);
        if (result != 0) {
            spinlock_release(&dev->lock);
            return result;
        }
        result = tpm_fifo_recv(dev, rsp, rsp_size);
    } else {
        result = tpm_crb_send(dev, cmd, cmd_size);
        if (result != 0) {
            spinlock_release(&dev->lock);
            return result;
        }
        result = tpm_crb_recv(dev, rsp, rsp_size);
    }

    spinlock_release(&dev->lock);
    return result;
}

int tpm_detect_version(tpm_t* dev)
{
    if (!dev) return -1;

    uint8_t cmd[12] = {0};
    cmd[0] = 0x00; cmd[1] = 0xC1;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x0C;
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x7A;
    cmd[10] = 0x00; cmd[11] = 0x00;

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);

    int result = tpm_send_command(dev, cmd, 12, rsp, &rsp_size);
    if (result != 0) {
        dev->tpm_version = TPM1_2;
        return 0;
    }

    if (rsp_size >= 10) {
        uint16_t tag = (uint16_t)((rsp[0] << 8) | rsp[1]);
        if (tag == 0x8001 || tag == 0x8002) {
            dev->tpm_version = TPM2_0;
        } else {
            dev->tpm_version = TPM1_2;
        }
    } else {
        dev->tpm_version = TPM1_2;
    }

    return 0;
}

int tpm2_startup(tpm_t* dev, uint16_t startup_type)
{
    if (!dev || !dev->initialized) return -1;

    uint8_t cmd[16];
    cmd[0] = 0x80; cmd[1] = 0x02;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x10;
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x43;
    cmd[10] = (uint8_t)(startup_type >> 8); cmd[11] = (uint8_t)(startup_type & 0xFF);
    cmd[12] = 0x00; cmd[13] = 0x00; cmd[14] = 0x00; cmd[15] = 0x00;

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);
    return tpm_send_command(dev, cmd, 16, rsp, &rsp_size);
}

int tpm2_self_test(tpm_t* dev, int full_test)
{
    if (!dev || !dev->initialized) return -1;

    uint8_t cmd[12];
    cmd[0] = 0x80; cmd[1] = 0x02;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x0C;
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x7C;
    cmd[10] = full_test ? 0x01 : 0x00; cmd[11] = 0x00;

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);
    return tpm_send_command(dev, cmd, 12, rsp, &rsp_size);
}

int tpm2_get_random(tpm_t* dev, uint16_t bytes_requested, uint8_t* random_bytes, uint16_t* bytes_returned)
{
    if (!dev || !dev->initialized || !random_bytes || !bytes_returned) return -1;

    uint8_t cmd[14];
    cmd[0] = 0x80; cmd[1] = 0x02;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x0E;
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x7B;
    cmd[10] = (uint8_t)(bytes_requested >> 8); cmd[11] = (uint8_t)(bytes_requested & 0xFF);
    cmd[12] = 0x00; cmd[13] = 0x00;

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);
    int result = tpm_send_command(dev, cmd, 14, rsp, &rsp_size);
    if (result != 0) return result;

    if (rsp_size > 12) {
        uint16_t len = (uint16_t)((rsp[10] << 8) | rsp[11]);
        if (len > bytes_requested) len = bytes_requested;
        memcpy(random_bytes, rsp + 12, len);
        *bytes_returned = len;
    } else {
        *bytes_returned = 0;
    }

    return 0;
}

int tpm2_pcr_extend(tpm_t* dev, uint32_t pcr_idx, const uint8_t* digest, uint16_t digest_size)
{
    if (!dev || !dev->initialized || !digest) return -1;

    uint32_t cmd_size = 14 + 4 + digest_size;
    uint8_t cmd[TPM_MAX_BUFFER_SIZE];
    cmd[0] = 0x80; cmd[1] = 0x02;
    cmd[2] = (uint8_t)(cmd_size >> 24); cmd[3] = (uint8_t)(cmd_size >> 16);
    cmd[4] = (uint8_t)(cmd_size >> 8); cmd[5] = (uint8_t)(cmd_size);
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x3C;
    cmd[10] = (uint8_t)(pcr_idx >> 24); cmd[11] = (uint8_t)(pcr_idx >> 16);
    cmd[12] = (uint8_t)(pcr_idx >> 8); cmd[13] = (uint8_t)(pcr_idx);
    cmd[14] = 0x00; cmd[15] = 0x00; cmd[16] = 0x00; cmd[17] = (uint8_t)digest_size;
    memcpy(cmd + 18, digest, digest_size);

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);
    return tpm_send_command(dev, cmd, cmd_size, rsp, &rsp_size);
}

int tpm2_pcr_read(tpm_t* dev, uint32_t pcr_idx, uint8_t* digest, uint16_t* digest_size)
{
    if (!dev || !dev->initialized || !digest || !digest_size) return -1;

    uint8_t cmd[22];
    cmd[0] = 0x80; cmd[1] = 0x02;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x16;
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x7E;
    cmd[10] = 0x00; cmd[11] = 0x01;
    cmd[12] = (uint8_t)(pcr_idx >> 24); cmd[13] = (uint8_t)(pcr_idx >> 16);
    cmd[14] = (uint8_t)(pcr_idx >> 8); cmd[15] = (uint8_t)(pcr_idx);
    cmd[16] = 0x00; cmd[17] = 0x00; cmd[18] = 0x00; cmd[19] = 0x20;
    cmd[20] = 0x00; cmd[21] = 0x00;

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);
    int result = tpm_send_command(dev, cmd, 22, rsp, &rsp_size);
    if (result != 0) return result;

    *digest_size = 32;
    if (rsp_size > 14 + 32) {
        memcpy(digest, rsp + 14, 32);
    } else {
        memset(digest, 0, 32);
    }

    return 0;
}

int tpm2_nv_define_space(tpm_t* dev, uint32_t nv_idx, uint16_t data_size, uint32_t attributes)
{
    if (!dev || !dev->initialized) return -1;
    (void)nv_idx; (void)data_size; (void)attributes;
    return 0;
}

int tpm2_nv_write(tpm_t* dev, uint32_t nv_idx, const uint8_t* data, uint16_t offset, uint16_t length)
{
    if (!dev || !dev->initialized || !data) return -1;
    (void)nv_idx; (void)offset; (void)length;
    return 0;
}

int tpm2_nv_read(tpm_t* dev, uint32_t nv_idx, uint8_t* data, uint16_t offset, uint16_t length, uint16_t* bytes_read)
{
    if (!dev || !dev->initialized || !data) return -1;
    (void)nv_idx; (void)offset; (void)length;
    if (bytes_read) *bytes_read = 0;
    return 0;
}

int tpm2_shutdown(tpm_t* dev, uint16_t shutdown_type)
{
    if (!dev || !dev->initialized) return -1;

    uint8_t cmd[12];
    cmd[0] = 0x80; cmd[1] = 0x02;
    cmd[2] = 0x00; cmd[3] = 0x00; cmd[4] = 0x00; cmd[5] = 0x0C;
    cmd[6] = 0x00; cmd[7] = 0x00; cmd[8] = 0x01; cmd[9] = 0x46;
    cmd[10] = (uint8_t)(shutdown_type >> 8); cmd[11] = (uint8_t)(shutdown_type & 0xFF);

    uint8_t rsp[TPM_MAX_BUFFER_SIZE];
    uint32_t rsp_size = sizeof(rsp);
    return tpm_send_command(dev, cmd, 12, rsp, &rsp_size);
}