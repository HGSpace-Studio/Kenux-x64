#ifndef ARCH_X86_64_TPM_H
#define ARCH_X86_64_TPM_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define TPM_INTERFACE_FIFO    0
#define TPM_INTERFACE_CRB     1

#define TPM1_2                1
#define TPM2_0                2

#define TPM_FIFO_BASE         0xFED40000

#define TPM_FIFO_ACCESS       0x00
#define TPM_FIFO_STS          0x04
#define TPM_FIFO_DATA_FIFO    0x08
#define TPM_FIFO_DID_VID      0x1C
#define TPM_FIFO_RID          0x20

#define TPM_ACCESS_TPM_ESTABLISH  0x01
#define TPM_ACCESS_REQUEST_USE    0x02
#define TPM_ACCESS_PENDING        0x04
#define TPM_ACCESS_SEIZE         0x08
#define TPM_ACCESS_BEEN_SEIZED   0x10
#define TPM_ACCESS_ACTIVE_LOCALITY 0x20
#define TPM_ACCESS_VALID         0x80

#define TPM_STS_VALID            0x01
#define TPM_STS_CMD_READY        0x02
#define TPM_STS_TPM_GO           0x04
#define TPM_STS_DATA_AVAIL       0x08
#define TPM_STS_DATA_EXPECT      0x10
#define TPM_STS_RETRY            0x20
#define TPM_STS_SELF_TEST_DONE   0x40
#define TPM_STS_RESP_RETRY       0x80

#define TPM_CRB_CTRL_REQ         0x00
#define TPM_CRB_CTRL_STS         0x04
#define TPM_CRB_CTRL_CANCEL      0x08
#define TPM_CRB_CTRL_START       0x0C
#define TPM_CRB_CTRL_INT_EN      0x10
#define TPM_CRB_CTRL_INT_STS     0x14
#define TPM_CRB_CTRL_CMD_SIZE    0x18
#define TPM_CRB_CTRL_CMD_LOW     0x1C
#define TPM_CRB_CTRL_CMD_HI      0x20
#define TPM_CRB_CTRL_RSP_SIZE    0x24
#define TPM_CRB_CTRL_RSP_LOW     0x28
#define TPM_CRB_CTRL_RSP_HI      0x2C

#define TPM_CRB_CTRL_STS_IDLE    0x00000001
#define TPM_CRB_CTRL_REQ_GO_IDLE 0x00000001
#define TPM_CRB_CTRL_REQ_CMD_READY 0x00000002
#define TPM_CRB_CTRL_START_START  0x00000001

#define TPM2_ST_NO_COMMANDS       0x00
#define TPM2_ST_NULL              0x8001
#define TPM2_ST_AUTH_SESSION      0x8002
#define TPM2_ST_HASH_CHECK       0x8003
#define TPM2_ST_AUTH_POLICY       0x8004
#define TPM2_ST_COMMAND_CHANGE    0x8005
#define TPM2_ST_VERIFIED          0x8006

#define TPM2_CC_NV_UndefineSpace  0x00000122
#define TPM2_CC_NV_DefineSpace    0x00000123
#define TPM2_CC_NV_Write          0x00000137
#define TPM2_CC_NV_Read           0x00000138
#define TPM2_CC_NV_WriteLock      0x00000139
#define TPM2_CC_NV_ReadLock       0x0000013A
#define TPM2_CC_GetCapability     0x0000017A
#define TPM2_CC_GetRandom         0x0000017B
#define TPM2_CC_SelfTest          0x0000017C
#define TPM2_CC_Startup           0x0000017E
#define TPM2_CC_Shutdown          0x0000017F
#define TPM2_CC_Create            0x00000153
#define TPM2_CC_Load              0x00000157
#define TPM2_CC_Unload            0x00000158
#define TPM2_CC_FlushContext      0x00000165
#define TPM2_CC_Sign              0x0000015C
#define TPM2_CC_VerifySignature   0x00000177
#define TPM2_CC_EncryptDecrypt    0x00000159
#define TPM2_CC_Hash              0x0000017D
#define TPM2_CC_PCR_Extend        0x0000013C
#define TPM2_CC_PCR_Read          0x0000017E
#define TPM2_CC_PCR_Reset         0x0000013D

#define TPM2_RC_SUCCESS           0x00000000
#define TPM2_RC_BAD_TAG           0x000001E0
#define TPM2_RC_INITIALIZE        0x00001000
#define TPM2_RC_FAILURE           0x00001001
#define TPM2_RC_DISABLED          0x00001020
#define TPM2_RC_DISABLED_CMD      0x00001021
#define TPM2_RC_NV_DEFINED        0x0000148C

#define TPM2_SU_CLEAR             0x0000
#define TPM2_SU_STATE             0x0001

#define TPM_MAX_BUFFER_SIZE       4096

typedef struct {
    uint16_t tag;
    uint32_t command_code;
    uint32_t size;
} tpm_cmd_header_t;

typedef struct {
    uint16_t tag;
    uint32_t response_code;
    uint32_t size;
} tpm_resp_header_t;

typedef struct {
    int       interface_type;
    int       tpm_version;
    uint64_t  base_addr;
    uint32_t  locality;
    uint32_t  cmd_size;
    uint32_t  rsp_size;
    uint64_t  cmd_addr;
    uint64_t  rsp_addr;
    uint8_t   cmd_buffer[TPM_MAX_BUFFER_SIZE];
    uint8_t   rsp_buffer[TPM_MAX_BUFFER_SIZE];
    uint32_t  cmd_offset;
    spinlock_t lock;
    int       initialized;
} tpm_t;

void tpm_init(void);
tpm_t* tpm_get_device(void);
int tpm_request_locality(tpm_t* dev, uint32_t locality);
void tpm_release_locality(tpm_t* dev, uint32_t locality);
int tpm_send_command(tpm_t* dev, const uint8_t* cmd, uint32_t cmd_size,
                     uint8_t* rsp, uint32_t* rsp_size);
int tpm2_startup(tpm_t* dev, uint16_t startup_type);
int tpm2_self_test(tpm_t* dev, int full_test);
int tpm2_get_random(tpm_t* dev, uint16_t bytes_requested, uint8_t* random_bytes, uint16_t* bytes_returned);
int tpm2_pcr_extend(tpm_t* dev, uint32_t pcr_idx, const uint8_t* digest, uint16_t digest_size);
int tpm2_pcr_read(tpm_t* dev, uint32_t pcr_idx, uint8_t* digest, uint16_t* digest_size);
int tpm2_nv_define_space(tpm_t* dev, uint32_t nv_idx, uint16_t data_size, uint32_t attributes);
int tpm2_nv_write(tpm_t* dev, uint32_t nv_idx, const uint8_t* data, uint16_t offset, uint16_t length);
int tpm2_nv_read(tpm_t* dev, uint32_t nv_idx, uint8_t* data, uint16_t offset, uint16_t length, uint16_t* bytes_read);
int tpm2_shutdown(tpm_t* dev, uint16_t shutdown_type);
int tpm_detect_version(tpm_t* dev);

#endif