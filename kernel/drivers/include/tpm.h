#ifndef KERNEL_DRIVERS_TPM_H
#define KERNEL_DRIVERS_TPM_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define TPM_BASE_ADDR     0xFED40000

#define TPM_ACCESS        0x00
#define TPM_STS           0x18
#define TPM_DATA_FIFO     0x24
#define TPM_DID_VID       0x00
#define TPM_RID           0x04
#define TPM_INTF_CAP      0x08
#define TPM_INT_ENABLE    0x08
#define TPM_INT_VECTOR    0x0C
#define TPM_INT_STATUS    0x10

#define TPM_ACCESS_TPM_ESTABLISH  0x01
#define TPM_ACCESS_REQUEST_USE    0x02
#define TPM_ACCESS_PENDING        0x04
#define TPM_ACCESS_SEIZE         0x08
#define TPM_ACCESS_BEEN_SEIZED   0x10
#define TPM_ACCESS_ACTIVE_LOCALITY 0x20
#define TPM_ACCESS_VALID         0x80

#define TPM_STS_CMD_READY        0x01
#define TPM_STS_GO               0x02
#define TPM_STS_DATA_AVAIL       0x04
#define TPM_STS_RESTART          0x08
#define TPM_STS_VALID            0x10
#define TPM_STS_TPM_FAMILY_MASK  0x60

#define TPM_TAG_RQU_COMMAND      0x00C1
#define TPM_TAG_RQU_AUTH1_COMMAND 0x00C2
#define TPM_TAG_RQU_AUTH2_COMMAND 0x00C3
#define TPM_TAG_RSP_COMMAND      0x00C4
#define TPM_TAG_RSP_AUTH1_COMMAND 0x00C5
#define TPM_TAG_RSP_AUTH2_COMMAND 0x00C6

#define TPM_ORD_OIAP             0x000A
#define TPM_ORD_OSAP             0x00B
#define TPM_ORD_TakeOwnership    0x000D
#define TPM_ORD_DisableOwnerClear 0x0018
#define TPM_ORD_ForceClear       0x001D
#define TPM_ORD_ClearEnable      0x001E
#define TPM_ORD_PhysicalEnable   0x001F
#define TPM_ORD_PhysicalDisable  0x0020
#define TPM_ORD_PhysicalSetDeactivated 0x0021
#define TPM_ORD_SetOwnerInstall  0x0022
#define TPM_ORD_Extend           0x0028
#define TPM_ORD_PCRRead          0x0029
#define TPM_ORD_SHA1Start        0x0031
#define TPM_ORD_SHA1Update       0x0032
#define TPM_ORD_SHA1Complete     0x0033
#define TPM_ORD_GetCapability    0x0065
#define TPM_ORD_GetRandom        0x006C
#define TPM_ORD_SelfTestFull     0x0053
#define TPM_ORD_SelfTestStartup  0x0054

#define TPM2_ST_NO_SESSIONS      0x8001
#define TPM2_ST_SESSIONS         0x8002

#define TPM2_CC_SelfTest         0x0143
#define TPM2_CC_GetRandom        0x0176
#define TPM2_CC_PCR_Read         0x017E
#define TPM2_CC_PCR_Extend       0x017F
#define TPM2_CC_PCR_Reset        0x017D
#define TPM2_CC_Create           0x0153
#define TPM2_CC_Load             0x0157
#define TPM2_CC_Unseal           0x015E
#define TPM2_CC_FlushContext     0x0165
#define TPM2_CC_Clear            0x0126
#define TPM2_CC_ClearControl     0x0127
#define TPM2_CC_HierarchyChangeAuth 0x0129
#define TPM2_CC_GetCapability    0x017A
#define TPM2_CC_Sign             0x015C
#define TPM2_CC_VerifySignature  0x0177
#define TPM2_CC_EncryptDecrypt  0x0159
#define TPM2_CC_Hash             0x0178
#define TPM2_CC_HashSequenceStart 0x0179
#define TPM2_CC_SequenceUpdate   0x015D
#define TPM2_CC_SequenceComplete 0x0160
#define TPM2_CC_NV_Read          0x014E
#define TPM2_CC_NV_Write         0x014D
#define TPM2_CC_NV_UndefineSpace 0x0142
#define TPM2_CC_NV_DefineSpace   0x0141

#define TPM_PCR_COUNT            24
#define TPM_SHA1_DIGEST_SIZE     20
#define TPM2_SHA256_DIGEST_SIZE  32
#define TPM_MAX_BUF              4096

typedef struct {
    volatile uint8_t* mmio;
    int               is_tpm2;
    uint16_t          vid;
    uint16_t          did;
    uint8_t           rid;
    uint32_t          intf_cap;
    uint8_t           locality;
    uint8_t           pcr_values[TPM_PCR_COUNT][TPM2_SHA256_DIGEST_SIZE];
    uint8_t           cmd_buf[TPM_MAX_BUF];
    uint8_t           rsp_buf[TPM_MAX_BUF];
    uint32_t          cmd_size;
    uint32_t          rsp_size;
    spinlock_t        lock;
} tpm_dev_t;

int  tpm_init(tpm_dev_t* dev, int use_tpm2);
void tpm_shutdown(tpm_dev_t* dev);
int  tpm_request_locality(tpm_dev_t* dev, uint8_t locality);
void tpm_release_locality(tpm_dev_t* dev, uint8_t locality);
int  tpm_send_command(tpm_dev_t* dev, const uint8_t* cmd, uint32_t cmd_size, uint8_t* rsp, uint32_t* rsp_size);
int  tpm_self_test(tpm_dev_t* dev);
int  tpm_pcr_read(tpm_dev_t* dev, uint32_t pcr_idx, uint8_t* digest, uint32_t* digest_size);
int  tpm_pcr_extend(tpm_dev_t* dev, uint32_t pcr_idx, const uint8_t* digest, uint32_t digest_size);
int  tpm_get_random(tpm_dev_t* dev, uint8_t* buf, uint32_t size);
int  tpm2_self_test(tpm_dev_t* dev, int full);
int  tpm2_pcr_read(tpm_dev_t* dev, uint32_t pcr_idx, uint8_t* digest, uint32_t* digest_size);
int  tpm2_pcr_extend(tpm_dev_t* dev, uint32_t pcr_idx, const uint8_t* digest, uint32_t digest_size);
int  tpm2_get_random(tpm_dev_t* dev, uint8_t* buf, uint32_t size);
int  tpm2_nv_read(tpm_dev_t* dev, uint32_t nv_idx, void* buf, uint32_t size, uint32_t offset);
int  tpm2_nv_write(tpm_dev_t* dev, uint32_t nv_idx, const void* buf, uint32_t size, uint32_t offset);

#endif