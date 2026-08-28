#include "tpm.h"
#include <string.h>

static void tpm_write8(tpm_dev_t* dev, uint32_t offset, uint8_t val)
{
    dev->mmio[offset] = val;
}

static uint8_t tpm_read8(tpm_dev_t* dev, uint32_t offset)
{
    return dev->mmio[offset];
}

static void tpm_write32(tpm_dev_t* dev, uint32_t offset, uint32_t val)
{
    *(volatile uint32_t*)(dev->mmio + offset) = val;
}

static uint32_t tpm_read32(tpm_dev_t* dev, uint32_t offset)
{
    return *(volatile uint32_t*)(dev->mmio + offset);
}

static int tpm_wait_status(tpm_dev_t* dev, uint8_t mask, uint8_t val, uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < timeout_ms * 1000; i++) {
        uint8_t sts = tpm_read8(dev, TPM_STS);
        if ((sts & mask) == val) return 0;
        for (volatile int d = 0; d < 100; d++);
    }
    return -1;
}

int tpm_request_locality(tpm_dev_t* dev, uint8_t locality)
{
    if (!dev) return -1;
    if (locality > 4) return -2;

    uint32_t base = locality * 0x1000;
    tpm_write8(dev, base + TPM_ACCESS, TPM_ACCESS_REQUEST_USE);

    for (int i = 0; i < 100000; i++) {
        uint8_t access = tpm_read8(dev, base + TPM_ACCESS);
        if ((access & (TPM_ACCESS_ACTIVE_LOCALITY | TPM_ACCESS_VALID)) ==
            (TPM_ACCESS_ACTIVE_LOCALITY | TPM_ACCESS_VALID)) {
            dev->locality = locality;
            return 0;
        }
    }
    return -3;
}

void tpm_release_locality(tpm_dev_t* dev, uint8_t locality)
{
    if (!dev) return;
    uint32_t base = locality * 0x1000;
    uint8_t access = tpm_read8(dev, base + TPM_ACCESS);
    tpm_write8(dev, base + TPM_ACCESS, TPM_ACCESS_ACTIVE_LOCALITY);
    (void)access;
}

int tpm_send_command(tpm_dev_t* dev, const uint8_t* cmd, uint32_t cmd_size, uint8_t* rsp, uint32_t* rsp_size)
{
    if (!dev || !cmd || !rsp) return -1;

    uint32_t base = dev->locality * 0x1000;

    int ret = tpm_wait_status(dev, TPM_STS_CMD_READY, TPM_STS_CMD_READY, 100);
    if (ret != 0) {
        tpm_write8(dev, base + TPM_STS, TPM_STS_CMD_READY);
        ret = tpm_wait_status(dev, TPM_STS_CMD_READY, TPM_STS_CMD_READY, 100);
        if (ret != 0) return -2;
    }

    uint32_t burst = 0;
    for (int i = 0; i < 1000; i++) {
        uint8_t sts = tpm_read8(dev, base + TPM_STS);
        burst = (uint32_t)((sts >> 8) & 0xFF);
        if (burst > 0) break;
    }
    if (burst == 0) return -3;

    uint32_t offset = 0;
    while (offset < cmd_size) {
        uint32_t count = burst;
        if (offset + count > cmd_size) count = cmd_size - offset;
        for (uint32_t i = 0; i < count; i++) {
            tpm_write8(dev, base + TPM_DATA_FIFO, cmd[offset++]);
        }
        if (offset < cmd_size) {
            burst = 0;
            for (int i = 0; i < 1000; i++) {
                uint8_t sts = tpm_read8(dev, base + TPM_STS);
                burst = (uint32_t)((sts >> 8) & 0xFF);
                if (burst > 0) break;
            }
            if (burst == 0) return -4;
        }
    }

    tpm_write8(dev, base + TPM_STS, TPM_STS_GO);

    ret = tpm_wait_status(dev, TPM_STS_DATA_AVAIL, TPM_STS_DATA_AVAIL, 5000);
    if (ret != 0) return -5;

    uint32_t rsp_offset = 0;
    uint32_t rsp_total = 0;
    int header_read = 0;

    while (1) {
        uint8_t sts = tpm_read8(dev, base + TPM_STS);
        if (!(sts & TPM_STS_DATA_AVAIL)) break;

        burst = (uint32_t)((sts >> 8) & 0xFF);
        if (burst == 0) {
            for (int i = 0; i < 1000; i++) {
                sts = tpm_read8(dev, base + TPM_STS);
                burst = (uint32_t)((sts >> 8) & 0xFF);
                if (burst > 0) break;
            }
            if (burst == 0) break;
        }

        for (uint32_t i = 0; i < burst && rsp_offset < TPM_MAX_BUF; i++) {
            rsp[rsp_offset++] = tpm_read8(dev, base + TPM_DATA_FIFO);
        }

        if (!header_read && rsp_offset >= 6) {
            rsp_total = ((uint32_t)rsp[2] << 24) | ((uint32_t)rsp[3] << 16) |
                        ((uint32_t)rsp[4] << 8) | rsp[5];
            header_read = 1;
        }

        if (header_read && rsp_offset >= rsp_total) break;
    }

    if (rsp_size) *rsp_size = rsp_offset;
    return 0;
}

int tpm_init(tpm_dev_t* dev, int use_tpm2)
{
    if (!dev) return -1;
    memset(dev, 0, sizeof(tpm_dev_t));
    spin_init(&dev->lock);

    dev->mmio = (volatile uint8_t*)TPM_BASE_ADDR;
    dev->is_tpm2 = use_tpm2;

    uint8_t access = tpm_read8(dev, TPM_ACCESS);
    if (!(access & TPM_ACCESS_VALID)) return -2;

    dev->vid = (uint16_t)tpm_read32(dev, 0x0F00 + TPM_DID_VID);
    dev->did = (uint16_t)(tpm_read32(dev, 0x0F00 + TPM_DID_VID) >> 16);
    dev->rid = tpm_read8(dev, 0x0F00 + TPM_RID);
    dev->intf_cap = tpm_read32(dev, 0x0F00 + TPM_INTF_CAP);

    int ret = tpm_request_locality(dev, 0);
    if (ret != 0) return -3;

    if (dev->is_tpm2) {
        tpm2_self_test(dev, 1);
    } else {
        tpm_self_test(dev);
    }

    return 0;
}

void tpm_shutdown(tpm_dev_t* dev)
{
    if (!dev) return;
    tpm_release_locality(dev, dev->locality);
}

int tpm_self_test(tpm_dev_t* dev)
{
    if (!dev) return -1;
    uint8_t cmd[10];
    cmd[0] = 0; cmd[1] = TPM_TAG_RQU_COMMAND >> 8;
    cmd[2] = TPM_TAG_RQU_COMMAND & 0xFF;
    cmd[3] = 0; cmd[4] = 0; cmd[5] = 10;
    cmd[6] = 0; cmd[7] = 0;
    cmd[8] = TPM_ORD_SelfTestFull >> 8;
    cmd[9] = TPM_ORD_SelfTestFull & 0xFF;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    return tpm_send_command(dev, cmd, 10, rsp, &rsp_size);
}

int tpm_pcr_read(tpm_dev_t* dev, uint32_t pcr_idx, uint8_t* digest, uint32_t* digest_size)
{
    if (!dev || !digest || pcr_idx >= TPM_PCR_COUNT) return -1;

    uint8_t cmd[14];
    cmd[0] = 0; cmd[1] = TPM_TAG_RQU_COMMAND >> 8;
    cmd[2] = TPM_TAG_RQU_COMMAND & 0xFF;
    cmd[3] = 0; cmd[4] = 0; cmd[5] = 14;
    cmd[6] = 0; cmd[7] = 0;
    cmd[8] = TPM_ORD_PCRRead >> 8;
    cmd[9] = TPM_ORD_PCRRead & 0xFF;
    cmd[10] = 0; cmd[11] = 0;
    cmd[12] = (uint8_t)(pcr_idx >> 8);
    cmd[13] = (uint8_t)pccr_idx;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    int ret = tpm_send_command(dev, cmd, 14, rsp, &rsp_size);
    if (ret != 0) return ret;

    if (rsp_size >= 10 + TPM_SHA1_DIGEST_SIZE) {
        memcpy(digest, rsp + 10, TPM_SHA1_DIGEST_SIZE);
        if (digest_size) *digest_size = TPM_SHA1_DIGEST_SIZE;
    }
    return 0;
}

int tpm_pcr_extend(tpm_dev_t* dev, uint32_t pcr_idx, const uint8_t* digest, uint32_t digest_size)
{
    if (!dev || !digest || pcr_idx >= TPM_PCR_COUNT) return -1;

    uint32_t cmd_size = 14 + digest_size;
    uint8_t cmd[TPM_MAX_BUF];
    cmd[0] = 0; cmd[1] = TPM_TAG_RQU_COMMAND >> 8;
    cmd[2] = TPM_TAG_RQU_COMMAND & 0xFF;
    cmd[3] = (uint8_t)(cmd_size >> 8); cmd[4] = (uint8_t)(cmd_size >> 16); cmd[5] = (uint8_t)cmd_size;
    cmd[6] = 0; cmd[7] = 0;
    cmd[8] = TPM_ORD_Extend >> 8;
    cmd[9] = TPM_ORD_Extend & 0xFF;
    cmd[10] = 0; cmd[11] = 0;
    cmd[12] = (uint8_t)(pcr_idx >> 8);
    cmd[13] = (uint8_t)pcr_idx;
    memcpy(cmd + 14, digest, digest_size);

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    return tpm_send_command(dev, cmd, cmd_size, rsp, &rsp_size);
}

int tpm_get_random(tpm_dev_t* dev, uint8_t* buf, uint32_t size)
{
    if (!dev || !buf) return -1;

    uint8_t cmd[18];
    cmd[0] = 0; cmd[1] = TPM_TAG_RQU_COMMAND >> 8;
    cmd[2] = TPM_TAG_RQU_COMMAND & 0xFF;
    cmd[3] = 0; cmd[4] = 0; cmd[5] = 18;
    cmd[6] = 0; cmd[7] = 0;
    cmd[8] = TPM_ORD_GetRandom >> 8;
    cmd[9] = TPM_ORD_GetRandom & 0xFF;
    cmd[10] = 0; cmd[11] = 0; cmd[12] = 0; cmd[13] = 0;
    cmd[14] = (uint8_t)(size >> 24); cmd[15] = (uint8_t)(size >> 16);
    cmd[16] = (uint8_t)(size >> 8); cmd[17] = (uint8_t)size;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    int ret = tpm_send_command(dev, cmd, 18, rsp, &rsp_size);
    if (ret != 0) return ret;

    if (rsp_size >= 14) {
        uint32_t recv_size = ((uint32_t)rsp[10] << 24) | ((uint32_t)rsp[11] << 16) |
                             ((uint32_t)rsp[12] << 8) | rsp[13];
        if (recv_size <= size) {
            memcpy(buf, rsp + 14, recv_size);
            return (int)recv_size;
        }
    }
    return -2;
}

int tpm2_self_test(tpm_dev_t* dev, int full)
{
    if (!dev) return -1;
    uint8_t cmd[12];
    uint32_t tag = TPM2_ST_NO_SESSIONS;
    cmd[0] = (uint8_t)(tag >> 8); cmd[1] = (uint8_t)tag;
    cmd[2] = 0; cmd[3] = 0; cmd[4] = 0; cmd[5] = 12;
    cmd[6] = (uint8_t)(TPM2_CC_SelfTest >> 24); cmd[7] = (uint8_t)(TPM2_CC_SelfTest >> 16);
    cmd[8] = (uint8_t)(TPM2_CC_SelfTest >> 8); cmd[9] = (uint8_t)TPM2_CC_SelfTest;
    cmd[10] = 0; cmd[11] = full ? 1 : 0;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    return tpm_send_command(dev, cmd, 12, rsp, &rsp_size);
}

int tpm2_pcr_read(tpm_dev_t* dev, uint32_t pcr_idx, uint8_t* digest, uint32_t* digest_size)
{
    if (!dev || !digest) return -1;
    uint8_t cmd[24];
    uint32_t tag = TPM2_ST_NO_SESSIONS;
    cmd[0] = (uint8_t)(tag >> 8); cmd[1] = (uint8_t)tag;
    cmd[2] = 0; cmd[3] = 0; cmd[4] = 0; cmd[5] = 24;
    cmd[6] = (uint8_t)(TPM2_CC_PCR_Read >> 24); cmd[7] = (uint8_t)(TPM2_CC_PCR_Read >> 16);
    cmd[8] = (uint8_t)(TPM2_CC_PCR_Read >> 8); cmd[9] = (uint8_t)TPM2_CC_PCR_Read;
    memset(cmd + 10, 0, 14);
    cmd[14] = 1;
    cmd[18] = (uint8_t)(pcr_idx >> 24); cmd[19] = (uint8_t)(pcr_idx >> 16);
    cmd[20] = (uint8_t)(pcr_idx >> 8); cmd[21] = (uint8_t)pcr_idx;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    int ret = tpm_send_command(dev, cmd, 24, rsp, &rsp_size);
    if (ret != 0) return ret;

    if (rsp_size >= 10 + TPM2_SHA256_DIGEST_SIZE) {
        memcpy(digest, rsp + 10, TPM2_SHA256_DIGEST_SIZE);
        if (digest_size) *digest_size = TPM2_SHA256_DIGEST_SIZE;
    }
    return 0;
}

int tpm2_pcr_extend(tpm_dev_t* dev, uint32_t pcr_idx, const uint8_t* digest, uint32_t digest_size)
{
    if (!dev || !digest) return -1;
    uint32_t cmd_size = 33 + digest_size;
    uint8_t cmd[TPM_MAX_BUF];
    uint32_t tag = TPM2_ST_SESSIONS;
    cmd[0] = (uint8_t)(tag >> 8); cmd[1] = (uint8_t)tag;
    cmd[2] = (uint8_t)(cmd_size >> 8); cmd[3] = (uint8_t)(cmd_size >> 16);
    cmd[4] = (uint8_t)(cmd_size >> 24); cmd[5] = (uint8_t)cmd_size;
    cmd[6] = (uint8_t)(TPM2_CC_PCR_Extend >> 24); cmd[7] = (uint8_t)(TPM2_CC_PCR_Extend >> 16);
    cmd[8] = (uint8_t)(TPM2_CC_PCR_Extend >> 8); cmd[9] = (uint8_t)TPM2_CC_PCR_Extend;
    cmd[10] = (uint8_t)(pcr_idx >> 24); cmd[11] = (uint8_t)(pcr_idx >> 16);
    cmd[12] = (uint8_t)(pcr_idx >> 8); cmd[13] = (uint8_t)pcr_idx;
    memset(cmd + 14, 0, 19);
    memcpy(cmd + 33, digest, digest_size);

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    return tpm_send_command(dev, cmd, cmd_size, rsp, &rsp_size);
}

int tpm2_get_random(tpm_dev_t* dev, uint8_t* buf, uint32_t size)
{
    if (!dev || !buf) return -1;
    uint8_t cmd[18];
    uint32_t tag = TPM2_ST_NO_SESSIONS;
    cmd[0] = (uint8_t)(tag >> 8); cmd[1] = (uint8_t)tag;
    cmd[2] = 0; cmd[3] = 0; cmd[4] = 0; cmd[5] = 18;
    cmd[6] = (uint8_t)(TPM2_CC_GetRandom >> 24); cmd[7] = (uint8_t)(TPM2_CC_GetRandom >> 16);
    cmd[8] = (uint8_t)(TPM2_CC_GetRandom >> 8); cmd[9] = (uint8_t)TPM2_CC_GetRandom;
    cmd[10] = 0; cmd[11] = 0; cmd[12] = 0; cmd[13] = 0;
    cmd[14] = (uint8_t)(size >> 24); cmd[15] = (uint8_t)(size >> 16);
    cmd[16] = (uint8_t)(size >> 8); cmd[17] = (uint8_t)size;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    int ret = tpm_send_command(dev, cmd, 18, rsp, &rsp_size);
    if (ret != 0) return ret;

    if (rsp_size >= 14) {
        uint32_t recv_size = ((uint32_t)rsp[10] << 24) | ((uint32_t)rsp[11] << 16) |
                             ((uint32_t)rsp[12] << 8) | rsp[13];
        if (recv_size <= size) {
            memcpy(buf, rsp + 14, recv_size);
            return (int)recv_size;
        }
    }
    return -2;
}

int tpm2_nv_read(tpm_dev_t* dev, uint32_t nv_idx, void* buf, uint32_t size, uint32_t offset)
{
    if (!dev || !buf) return -1;
    uint8_t cmd[30];
    uint32_t tag = TPM2_ST_SESSIONS;
    cmd[0] = (uint8_t)(tag >> 8); cmd[1] = (uint8_t)tag;
    cmd[2] = 0; cmd[3] = 0; cmd[4] = 0; cmd[5] = 30;
    cmd[6] = (uint8_t)(TPM2_CC_NV_Read >> 24); cmd[7] = (uint8_t)(TPM2_CC_NV_Read >> 16);
    cmd[8] = (uint8_t)(TPM2_CC_NV_Read >> 8); cmd[9] = (uint8_t)TPM2_CC_NV_Read;
    cmd[10] = (uint8_t)(nv_idx >> 24); cmd[11] = (uint8_t)(nv_idx >> 16);
    cmd[12] = (uint8_t)(nv_idx >> 8); cmd[13] = (uint8_t)nv_idx;
    memset(cmd + 14, 0, 8);
    cmd[22] = (uint8_t)(size >> 24); cmd[23] = (uint8_t)(size >> 16);
    cmd[24] = (uint8_t)(size >> 8); cmd[25] = (uint8_t)size;
    cmd[26] = (uint8_t)(offset >> 24); cmd[27] = (uint8_t)(offset >> 16);
    cmd[28] = (uint8_t)(offset >> 8); cmd[29] = (uint8_t)offset;

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    int ret = tpm_send_command(dev, cmd, 30, rsp, &rsp_size);
    if (ret != 0) return ret;
    (void)buf;
    return 0;
}

int tpm2_nv_write(tpm_dev_t* dev, uint32_t nv_idx, const void* buf, uint32_t size, uint32_t offset)
{
    if (!dev || !buf) return -1;
    uint32_t cmd_size = 30 + size;
    uint8_t cmd[TPM_MAX_BUF];
    uint32_t tag = TPM2_ST_SESSIONS;
    cmd[0] = (uint8_t)(tag >> 8); cmd[1] = (uint8_t)tag;
    cmd[2] = (uint8_t)(cmd_size >> 8); cmd[3] = (uint8_t)(cmd_size >> 16);
    cmd[4] = (uint8_t)(cmd_size >> 24); cmd[5] = (uint8_t)cmd_size;
    cmd[6] = (uint8_t)(TPM2_CC_NV_Write >> 24); cmd[7] = (uint8_t)(TPM2_CC_NV_Write >> 16);
    cmd[8] = (uint8_t)(TPM2_CC_NV_Write >> 8); cmd[9] = (uint8_t)TPM2_CC_NV_Write;
    cmd[10] = (uint8_t)(nv_idx >> 24); cmd[11] = (uint8_t)(nv_idx >> 16);
    cmd[12] = (uint8_t)(nv_idx >> 8); cmd[13] = (uint8_t)nv_idx;
    memset(cmd + 14, 0, 8);
    cmd[22] = (uint8_t)(size >> 24); cmd[23] = (uint8_t)(size >> 16);
    cmd[24] = (uint8_t)(size >> 8); cmd[25] = (uint8_t)size;
    cmd[26] = (uint8_t)(offset >> 24); cmd[27] = (uint8_t)(offset >> 16);
    cmd[28] = (uint8_t)(offset >> 8); cmd[29] = (uint8_t)offset;
    memcpy(cmd + 30, buf, size);

    uint8_t rsp[TPM_MAX_BUF];
    uint32_t rsp_size = 0;
    return tpm_send_command(dev, cmd, cmd_size, rsp, &rsp_size);
}