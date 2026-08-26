#ifndef KAPI_LEONOS_EXT_H
#define KAPI_LEONOS_EXT_H

#include <stdint.h>
#include "kapi_socket.h"
#include "kapi_netdevice.h"
#include "kapi_vfs.h"
#include "kapi_security.h"
#include "kapi_time.h"
#include "kapi_string.h"

#define KAPI_NET_AF_INET       KAPI_AF_INET
#define KAPI_NET_SOCK_STREAM   KAPI_SOCK_STREAM
#define KAPI_NET_IPPROTO_TCP   KAPI_IPPROTO_TCP

#define KAPI_HTTP_HEADER_MAX   2048U
#define KAPI_HTTP_BODY_MAX     8192U

#define KAPI_INPUTM_MAX_PROVIDERS 8U
#define KAPI_INPUTM_MAX_CANDIDATES 5U
#define KAPI_INPUTM_ID_LEN 32U
#define KAPI_INPUTM_NAME_LEN 64U
#define KAPI_INPUTM_ABBREV_LEN 8U
#define KAPI_INPUTM_TEXT_LEN 128U
#define KAPI_INPUTM_START_MANUAL 0U
#define KAPI_INPUTM_START_LOGIN 1U
#define KAPI_INPUTM_START_ON_DEMAND 2U
#define KAPI_INPUTM_CONTEXT_FOCUSED 0x00000001U
#define KAPI_INPUTM_CONTEXT_SECURE 0x00000002U
#define KAPI_INPUTM_RESULT_COMPOSITION 1U
#define KAPI_INPUTM_RESULT_COMMIT 2U
#define KAPI_INPUTM_RESULT_CANCEL 3U
#define KAPI_INPUTM_RESULT_PASSTHROUGH 4U
#define KAPI_INPUTM_RENDER_CONTROLS 0x00000001U
#define KAPI_INPUTM_RENDER_PIXELS 0x00000002U

#define KAPI_DRIVER_ABI_VERSION 1U
#define KAPI_DRIVER_MODULE_MAGIC 0x4b445256U
#define KAPI_DRIVER_MAX 16U
#define KAPI_DRIVER_FILE_LEN 64U
#define KAPI_DRIVER_NAME_LEN 32U
#define KAPI_DRIVER_ERROR_LEN 96U
#define KAPI_DRIVER_KIND_INPUT 1U
#define KAPI_DRIVER_KIND_SERIAL 2U
#define KAPI_DRIVER_KIND_NETWORK 3U
#define KAPI_DRIVER_KIND_AUDIO 4U
#define KAPI_DRIVER_STATE_UNLOADED 0U
#define KAPI_DRIVER_STATE_LOADING 1U
#define KAPI_DRIVER_STATE_LOADED 2U
#define KAPI_DRIVER_STATE_DISABLED 3U
#define KAPI_DRIVER_STATE_FAILED 4U
#define KAPI_DRIVER_FLAG_AUTOSTART 0x00000001U
#define KAPI_DRIVER_FLAG_DISABLED 0x00000002U
#define KAPI_DRIVER_FLAG_BUILTIN 0x00000004U
#define KAPI_DRIVER_CONTROL_LOAD 1U
#define KAPI_DRIVER_CONTROL_UNLOAD 2U
#define KAPI_DRIVER_CONTROL_FORCE_UNLOAD 3U
#define KAPI_DRIVER_CONTROL_RESCAN 4U

#define KAPI_LICENSE_EMAIL_LEN 96U
#define KAPI_LICENSE_KEY_LEN 64U
#define KAPI_LICENSE_INSTALL_ID_LEN 48U
#define KAPI_LICENSE_SERVER_URL_LEN 128U
#define KAPI_LICENSE_STATUS_LEN 96U
#define KAPI_LICENSE_STATUS_OK 0U
#define KAPI_LICENSE_STATUS_MISSING 1U

#define KAPI_PNG_MAX_PIXELS (1024U * 1024U)
#define KAPI_PNG_MAX_FILE_BYTES (16U * 1024U * 1024U)

#define KAPI_PTY_PATH_LEN 160U
#define KAPI_PTY_NCCS 11U
#define KAPI_PTY_IFLAG_ICRNL 0x0002U
#define KAPI_PTY_LFLAG_ECHO 0x0001U
#define KAPI_PTY_LFLAG_ECHONL 0x0008U
#define KAPI_PTY_LFLAG_ICANON 0x0010U
#define KAPI_PTY_LFLAG_IEXTEN 0x0020U
#define KAPI_PTY_LFLAG_ISIG 0x0040U

#define KAPI_STARTUP_MAX_ENTRIES 16U
#define KAPI_STARTUP_MAX_ARGS 7U
#define KAPI_STARTUP_ARG_LEN 64U
#define KAPI_STARTUP_STATUS_PENDING 1U
#define KAPI_STARTUP_STATUS_APPROVED 2U
#define KAPI_STARTUP_STATUS_DENIED 3U
#define KAPI_STARTUP_STATUS_FAILED 7U
#define KAPI_STARTUP_DECISION_ALLOW 1U
#define KAPI_STARTUP_DECISION_DENY 2U

typedef struct {
    char id[KAPI_INPUTM_ID_LEN];
    char name[KAPI_INPUTM_NAME_LEN];
    char abbreviation[KAPI_INPUTM_ABBREV_LEN];
    uint32_t startup_mode;
    uint32_t render_flags;
    uint32_t enabled;
} KAPI_INPUTM_PROVIDER;

typedef struct {
    uint32_t sequence;
    uint32_t client_pid;
    uint32_t window_id;
    uint32_t context_flags;
    uint8_t keycode;
    uint8_t pressed;
    uint8_t reserved0;
    uint8_t reserved1;
    int32_t caret_x;
    int32_t caret_y;
    uint32_t caret_w;
    uint32_t caret_h;
} KAPI_INPUTM_KEY_EVENT;

typedef struct {
    uint32_t sequence;
    uint32_t client_pid;
    uint32_t window_id;
    uint32_t type;
    char text[KAPI_INPUTM_TEXT_LEN];
    char candidates[KAPI_INPUTM_MAX_CANDIDATES][KAPI_INPUTM_TEXT_LEN];
    uint32_t candidate_count;
    uint32_t selected_candidate;
    uint8_t keycode;
    uint8_t pressed;
    uint8_t reserved0;
    uint8_t reserved1;
} KAPI_INPUTM_RESULT;

typedef struct {
    uint32_t window_id;
    uint32_t flags;
    int32_t caret_x;
    int32_t caret_y;
    uint32_t caret_w;
    uint32_t caret_h;
} KAPI_INPUTM_CONTEXT;

typedef struct {
    uint32_t uid;
    char active_id[KAPI_INPUTM_ID_LEN];
    char composition[KAPI_INPUTM_TEXT_LEN];
    char candidates[KAPI_INPUTM_MAX_CANDIDATES][KAPI_INPUTM_TEXT_LEN];
    uint32_t candidate_count;
    uint32_t selected_candidate;
    uint32_t render_flags;
    uint32_t config_generation;
    uint32_t window_id;
    int32_t caret_x;
    int32_t caret_y;
    uint32_t caret_w;
    uint32_t caret_h;
} KAPI_INPUTM_STATE;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t reserved;
} KAPI_DRIVER_PCI_DEVICE;

typedef struct {
    uint32_t id;
    uint32_t state;
    uint32_t kind;
    uint32_t flags;
    uint32_t abi_version;
    uint32_t version;
    uint64_t load_address;
    uint64_t image_size;
    char file[KAPI_DRIVER_FILE_LEN];
    char name[KAPI_DRIVER_NAME_LEN];
    char error[KAPI_DRIVER_ERROR_LEN];
} KAPI_DRIVER_INFO;

typedef struct {
    uint32_t status;
    char mode[16];
    char email_hash[24];
    char install_id[KAPI_LICENSE_INSTALL_ID_LEN];
    char detail[KAPI_LICENSE_STATUS_LEN];
} KAPI_LICENSE_INFO;

typedef struct {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_cc[KAPI_PTY_NCCS];
    uint8_t reserved;
    uint32_t c_ispeed;
    uint32_t c_ospeed;
} KAPI_PTY_TERMIOS;

typedef struct {
    uint16_t ws_row;
    uint16_t ws_col;
} KAPI_PTY_WINSIZE;

typedef struct {
    uint32_t argc;
    uint32_t reserved;
    char path[256];
    char args[KAPI_STARTUP_MAX_ARGS][KAPI_STARTUP_ARG_LEN];
} KAPI_STARTUP_COMMAND;

typedef struct {
    uint32_t id;
    uint32_t enabled;
    KAPI_STARTUP_COMMAND command;
} KAPI_STARTUP_ENTRY;

int KAPI_InputM_Register(const KAPI_INPUTM_PROVIDER* provider);
int KAPI_InputM_Unregister(void);
int KAPI_InputM_ProviderNext(KAPI_INPUTM_KEY_EVENT* event);
int KAPI_InputM_ProviderResult(const KAPI_INPUTM_RESULT* result);
int KAPI_InputM_SubmitKey(uint32_t window_id, uint8_t keycode, uint8_t pressed);
int KAPI_InputM_PollResult(KAPI_INPUTM_RESULT* result);
int KAPI_InputM_SetActive(uint32_t uid, const char* id);
int KAPI_InputM_List(uint32_t uid, KAPI_INPUTM_PROVIDER* providers,
                     uint32_t capacity, uint32_t* out_count);
int KAPI_InputM_SetContext(const KAPI_INPUTM_CONTEXT* context);
int KAPI_InputM_GetState(uint32_t uid, KAPI_INPUTM_STATE* state);
int KAPI_InputM_NotifyConfig(uint32_t uid);
int KAPI_InputM_ObserveGUIKey(uint32_t window_id, uint8_t* keycode, uint8_t pressed);
int KAPI_InputM_PollGUICommit(uint32_t window_id);
int KAPI_InputM_TakeText(char* buffer, uint32_t capacity);
int KAPI_InputM_TakeKey(uint8_t* keycode, uint8_t* pressed);
void KAPI_InputM_NoteGUIWindow(uint32_t window_id);
int KAPI_InputM_SetCurrentContext(uint32_t flags, int32_t caret_x,
                                  int32_t caret_y, uint32_t caret_w,
                                  uint32_t caret_h);

int KAPI_Driver_List(KAPI_DRIVER_INFO* drivers, uint32_t capacity, uint32_t* out_count);
int KAPI_Driver_Control(uint32_t action, const char* file);

int KAPI_License_Status(KAPI_LICENSE_INFO* info);
int KAPI_License_Required(void);
int KAPI_License_DefaultServer(char* out, uint32_t cap);
int KAPI_License_InstallID(char* out, uint32_t cap);
int KAPI_License_ActivateOnline(const char* email, const char* key,
                                char* detail, uint32_t detail_cap);
int KAPI_License_ActivateOffline(const char* email, const char* offline_key,
                                 char* detail, uint32_t detail_cap);

int KAPI_PNG_DecodeFile(const char* path, uint32_t** out_pixels,
                        uint32_t* out_width, uint32_t* out_height);
void KAPI_PNG_Free(uint32_t* pixels);

int KAPI_PTY_Create(void);
int KAPI_PTY_ReadOutput(uint32_t pty_id, char* buffer, uint32_t length);
int KAPI_PTY_WriteInput(uint32_t pty_id, const char* buffer, uint32_t length);
int KAPI_PTY_Spawn(const char* path, uint32_t pty_id);
int KAPI_PTY_SpawnArgv(const char* path, uint32_t pty_id, char* const argv[], char* const envp[]);
int KAPI_PTY_Self(void);
int KAPI_PTY_InputAvailable(void);
int KAPI_PTY_GetTermios(uint32_t pty_id, KAPI_PTY_TERMIOS* termios);
int KAPI_PTY_SetTermios(uint32_t pty_id, const KAPI_PTY_TERMIOS* termios);
int KAPI_PTY_GetWinsize(uint32_t pty_id, KAPI_PTY_WINSIZE* winsize);
int KAPI_PTY_SetWinsize(uint32_t pty_id, const KAPI_PTY_WINSIZE* winsize);

int KAPI_Startup_Request(const KAPI_STARTUP_COMMAND* command, uint32_t* out_request_id);
int KAPI_Startup_RequestStatus(uint32_t request_id, uint32_t* out_status);
int KAPI_Startup_DialogGet(void* request);
int KAPI_Startup_DialogResolve(uint32_t request_id, uint32_t decision);
int KAPI_Startup_List(uint32_t uid, KAPI_STARTUP_ENTRY* entries,
                      uint32_t capacity, uint32_t* out_count);
int KAPI_Startup_SetEnabled(uint32_t uid, uint32_t entry_id, uint32_t enabled);
int KAPI_Startup_Remove(uint32_t uid, uint32_t entry_id);
int KAPI_Startup_LaunchCurrentUser(void);

#endif
