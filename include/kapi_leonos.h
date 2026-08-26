#ifndef KAPI_LEONOS_H
#define KAPI_LEONOS_H

#include <stdint.h>
#include <stddef.h>

#include "kapi_string.h"
#include "kapi_time.h"
#include "kapi_security.h"
#include "kapi_vfs.h"
#include "kapi_netdevice.h"
#include "kapi_socket.h"

#define KAPI_LEON_ABI_VERSION 1U

#define KAPI_API_PATH_MAX 256U
#define KAPI_API_NAME_LEN 64U
#define KAPI_API_VERSION_LEN 16U

#define KAPI_O_RDONLY 0x0000
#define KAPI_O_WRONLY 0x0001
#define KAPI_O_RDWR 0x0002
#define KAPI_O_CREAT 0x0040
#define KAPI_O_TRUNC 0x0200
#define KAPI_O_APPEND 0x0400

#define KAPI_SEEK_SET 0
#define KAPI_SEEK_CUR 1
#define KAPI_SEEK_END 2

#define KAPI_TAR_BLOCK_SIZE 512U
#define KAPI_TAR_NAME_LEN 100U
#define KAPI_TAR_MAX_FILE_SIZE (32U * 1024U * 1024U)
#define KAPI_TAR_TYPE_FILE '0'
#define KAPI_TAR_TYPE_DIR '5'

#define KAPI_INI_MAX_SIZE (64U * 1024U)
#define KAPI_INI_MAX_SECTIONS 32U
#define KAPI_INI_MAX_KEYS_PER_SECTION 64U
#define KAPI_INI_NAME_LEN 64U
#define KAPI_INI_VALUE_LEN 256U

#define KAPI_DEVICE_MAX 24U
#define KAPI_DEVICE_NAME_LEN 32U
#define KAPI_DEVICE_STATUS_LEN 32U
#define KAPI_DEVICE_DETAIL_LEN 96U

#define KAPI_DEVICE_CLASS_SYSTEM 1U
#define KAPI_DEVICE_CLASS_INPUT 2U
#define KAPI_DEVICE_CLASS_DISPLAY 3U
#define KAPI_DEVICE_CLASS_STORAGE 4U
#define KAPI_DEVICE_CLASS_SERIAL 5U
#define KAPI_DEVICE_CLASS_NETWORK 6U
#define KAPI_DEVICE_CLASS_AUDIO 7U

#define KAPI_DEVICE_FLAG_PRESENT 0x00000001U
#define KAPI_DEVICE_FLAG_ACTIVE 0x00000002U
#define KAPI_DEVICE_FLAG_BOOT 0x00000004U
#define KAPI_DEVICE_FLAG_REMOVABLE 0x00000008U

#define KAPI_AUDIO_MAX_WRITE (64U * 1024U)
#define KAPI_AUDIO_STATUS_OK 0U
#define KAPI_AUDIO_STATUS_NO_DEVICE 1U
#define KAPI_AUDIO_STATUS_BAD_FORMAT 2U
#define KAPI_AUDIO_STATUS_PLAYBACK_FAILED 3U

typedef struct {
    char name[KAPI_API_NAME_LEN];
    char version[KAPI_API_VERSION_LEN];
    char main_exe[KAPI_API_PATH_MAX];
    char default_path[KAPI_API_PATH_MAX];
    char icon[KAPI_API_PATH_MAX];
    uint32_t requires_admin;
    uint32_t desktop_shortcut;
    uint32_t input_method;
    char input_method_id[32];
    char input_method_abbreviation[16];
    uint32_t input_method_startup_mode;
    uint32_t input_method_launch_after_install;
    char input_method_settings[KAPI_API_PATH_MAX];
    char input_method_settings_app[KAPI_API_PATH_MAX];
} KAPI_API_INFO;

typedef int (*KAPI_API_PROGRESS_FN)(uint32_t processed, uint32_t total,
                                    void* context);

typedef int (*KAPI_TAR_PROGRESS_FN)(uint32_t processed, uint32_t total,
                                    void* context);

typedef struct {
    uint32_t id;
    uint32_t device_class;
    uint32_t flags;
    uint32_t reserved;
    uint64_t value0;
    uint64_t value1;
    char name[KAPI_DEVICE_NAME_LEN];
    char status[KAPI_DEVICE_STATUS_LEN];
    char detail[KAPI_DEVICE_DETAIL_LEN];
} KAPI_DEVICE_INFO;

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
} KAPI_AUDIO_FORMAT;

typedef struct {
    uint32_t status;
    uint32_t queued_bytes;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t reserved;
} KAPI_AUDIO_STATE;

int KAPI_API_ParseInfo(const char* api_path, KAPI_API_INFO* info);
int KAPI_API_ExtractFiles(const char* api_path, const char* dest_dir);
int KAPI_API_Install(const char* api_path, const char* dest_dir,
                     uint32_t create_shortcut);
int KAPI_API_InstallWithProgress(const char* api_path, const char* dest_dir,
                                 uint32_t create_shortcut,
                                 KAPI_API_PROGRESS_FN progress,
                                 void* context);

int KAPI_INI_Load(const char* path);
int KAPI_INI_LoadStrict(const char* path);
int KAPI_INI_Get(const char* section, const char* key,
                 char* value, uint32_t capacity);
int KAPI_INI_SectionCount(void);
int KAPI_INI_SectionName(uint32_t index, char* name, uint32_t capacity);
int KAPI_INI_KeyCount(const char* section);
int KAPI_INI_KeyName(const char* section, uint32_t index,
                     char* name, uint32_t capacity);

int KAPI_TAR_List(const char* tar_path, char* output, uint32_t capacity);
int KAPI_TAR_ExtractFile(const char* tar_path, const char* stored_name,
                         const char* dest_path);
int KAPI_TAR_ExtractAll(const char* tar_path, const char* dest_dir);
int KAPI_TAR_Create(const char* tar_path);
int KAPI_TAR_PackFile(const char* tar_path, const char* file_path,
                      const char* stored_name);
int KAPI_TAR_PackFileAppend(int tar_fd, const char* file_path,
                            const char* stored_name);
int KAPI_TAR_Finalize(int tar_fd);
int KAPI_TAR_PackDir(const char* tar_path, const char* dir_path);
int KAPI_TAR_PackDirAppend(int tar_fd, const char* dir_path);
int KAPI_TAR_ExtractAllWithProgress(const char* tar_path, const char* dest_dir,
                                    KAPI_TAR_PROGRESS_FN progress,
                                    void* context);

int KAPI_Device_List(KAPI_DEVICE_INFO* devices, uint32_t capacity,
                     uint32_t* out_count);

int KAPI_Audio_Configure(const KAPI_AUDIO_FORMAT* format);
long KAPI_Audio_Write(const void* data, uint32_t length, uint32_t* written);
int KAPI_Audio_GetState(KAPI_AUDIO_STATE* state);

#include "kapi_leonos_ext.h"

#endif
