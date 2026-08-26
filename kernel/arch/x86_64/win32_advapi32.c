#include <arch/win32.h>
#include <arch/io.h>
#include <string.h>

extern uint64_t timer_get_jiffies(void);
extern uint64_t GetTickCount64(void);
extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);
extern DWORD GetLastError(void);
extern void SetLastError(DWORD dwErrCode);

#ifndef SC_HANDLE
typedef HANDLE SC_HANDLE;
#endif

#ifndef ALG_ID
typedef UINT ALG_ID;
#endif

#ifndef _LUID_DEFINED
typedef struct _LUID {
    DWORD LowPart;
    LONG  HighPart;
} LUID, *PLUID;
#endif

#ifndef _SID_NAME_USE_DEFINED
typedef enum _SID_NAME_USE {
    SidTypeUser = 1,
    SidTypeGroup,
    SidTypeDomain,
    SidTypeAlias,
    SidTypeWellKnownGroup,
    SidTypeDeletedAccount,
    SidTypeInvalid,
    SidTypeUnknown,
    SidTypeComputer,
    SidTypeLabel,
    SidTypeLogonSession
} SID_NAME_USE, *PSID_NAME_USE;
#define _SID_NAME_USE_DEFINED
#endif

#ifndef LPBYTE
typedef BYTE* LPBYTE;
#endif

#ifndef LPDWORD
typedef DWORD* LPDWORD;
#endif

#ifndef PHANDLE
typedef HANDLE* PHANDLE;
#endif

#ifndef ACCESS_MASK
typedef DWORD ACCESS_MASK;
#endif

#define advapi_set_last_error(err) SetLastError(err)

/* ================================================================
 * 1. EVENT LOGGING - 64KB ring buffer in memory
 * ================================================================ */

#define EVENTLOG_BUFFER_SIZE  (64 * 1024)
#define EVENTLOG_MAX_SOURCES  64
#define EVENTLOG_MAX_STRINGS  16
#define EVENTLOG_MAX_STRING_LEN 256

typedef struct {
    BOOL     used;
    HANDLE   handle;
    char     source_name[256];
    char     server_name[64];
} event_source_t;

typedef struct {
    DWORD    record_number;
    DWORD    time_written;
    WORD     event_type;
    WORD     category;
    DWORD    event_id;
    WORD     num_strings;
    DWORD    data_size;
    char     source[256];
    char     strings[EVENTLOG_MAX_STRINGS][EVENTLOG_MAX_STRING_LEN];
    BYTE     raw_data[1024];
} event_record_t;

static event_source_t g_event_sources[EVENTLOG_MAX_SOURCES];
static BYTE  g_event_buffer[EVENTLOG_BUFFER_SIZE];
static DWORD g_event_buf_head = 0;
static DWORD g_event_buf_tail = 0;
static DWORD g_event_record_count = 0;
static DWORD g_event_next_record = 1;
static HANDLE g_event_open_handles[EVENTLOG_MAX_SOURCES];
static DWORD g_event_open_count = 0;
static BOOL  g_eventlog_initialized = FALSE;

static void eventlog_init_internal(void) {
    if (g_eventlog_initialized) return;
    memset(g_event_sources, 0, sizeof(g_event_sources));
    memset(g_event_buffer, 0, sizeof(g_event_buffer));
    memset(g_event_open_handles, 0, sizeof(g_event_open_handles));
    g_event_buf_head = 0;
    g_event_buf_tail = 0;
    g_event_record_count = 0;
    g_event_next_record = 1;
    g_event_open_count = 0;
    g_eventlog_initialized = TRUE;
}

static HANDLE event_source_alloc_handle(void) {
    static DWORD next_handle = 0x1000;
    return (HANDLE)(uintptr_t)(++next_handle);
}

HANDLE RegisterEventSourceA(const char* lpUNCServerName,
                            const char* lpSourceName) {
    DWORD i;
    event_source_t* src;
    if (!g_eventlog_initialized) eventlog_init_internal();
    if (lpSourceName == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    for (i = 0; i < EVENTLOG_MAX_SOURCES; i++) {
        if (!g_event_sources[i].used) break;
    }
    if (i >= EVENTLOG_MAX_SOURCES) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    src = &g_event_sources[i];
    memset(src, 0, sizeof(*src));
    src->used = TRUE;
    src->handle = event_source_alloc_handle();
    strncpy(src->source_name, lpSourceName, sizeof(src->source_name) - 1);
    if (lpUNCServerName != NULL) {
        strncpy(src->server_name, lpUNCServerName, sizeof(src->server_name) - 1);
    }
    return src->handle;
}

BOOL DeregisterEventSource(HANDLE hEventLog) {
    DWORD i;
    if (!g_eventlog_initialized) return FALSE;
    if (hEventLog == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    for (i = 0; i < EVENTLOG_MAX_SOURCES; i++) {
        if (g_event_sources[i].used && g_event_sources[i].handle == hEventLog) {
            memset(&g_event_sources[i], 0, sizeof(event_source_t));
            return TRUE;
        }
    }
    advapi_set_last_error(ERROR_INVALID_HANDLE);
    return FALSE;
}

static event_source_t* find_event_source(HANDLE h) {
    DWORD i;
    for (i = 0; i < EVENTLOG_MAX_SOURCES; i++) {
        if (g_event_sources[i].used && g_event_sources[i].handle == h) {
            return &g_event_sources[i];
        }
    }
    return NULL;
}

static void event_buffer_write(const void* data, DWORD len) {
    const BYTE* p = (const BYTE*)data;
    DWORD i;
    for (i = 0; i < len; i++) {
        g_event_buffer[g_event_buf_head] = p[i];
        g_event_buf_head = (g_event_buf_head + 1) % EVENTLOG_BUFFER_SIZE;
        if (g_event_buf_head == g_event_buf_tail) {
            g_event_buf_tail = (g_event_buf_tail + 1) % EVENTLOG_BUFFER_SIZE;
        }
    }
}

BOOL ReportEventA(HANDLE hEventLog, WORD wType, WORD wCategory,
                  DWORD dwEventID, void* lpUserSid,
                  WORD wNumStrings, DWORD dwDataSize,
                  const char** lpStrings, LPVOID lpRawData) {
    event_source_t* src;
    event_record_t rec;
    WORD i;
    size_t slen;
    (void)lpUserSid;
    if (!g_eventlog_initialized) eventlog_init_internal();
    src = find_event_source(hEventLog);
    if (src == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    memset(&rec, 0, sizeof(rec));
    rec.record_number = g_event_next_record++;
    rec.time_written = (DWORD)GetTickCount64();
    rec.event_type = wType;
    rec.category = wCategory;
    rec.event_id = dwEventID;
    strncpy(rec.source, src->source_name, sizeof(rec.source) - 1);
    rec.num_strings = (wNumStrings > EVENTLOG_MAX_STRINGS) ? EVENTLOG_MAX_STRINGS : wNumStrings;
    if (lpStrings != NULL) {
        for (i = 0; i < rec.num_strings; i++) {
            if (lpStrings[i] != NULL) {
                slen = strlen(lpStrings[i]);
                if (slen >= EVENTLOG_MAX_STRING_LEN) slen = EVENTLOG_MAX_STRING_LEN - 1;
                memcpy(rec.strings[i], lpStrings[i], slen);
                rec.strings[i][slen] = 0;
            }
        }
    }
    rec.data_size = (dwDataSize > sizeof(rec.raw_data)) ? sizeof(rec.raw_data) : dwDataSize;
    if (lpRawData != NULL && rec.data_size > 0) {
        memcpy(rec.raw_data, lpRawData, rec.data_size);
    }
    event_buffer_write(&rec, sizeof(rec));
    if (g_event_record_count < 0xFFFFFFFF) g_event_record_count++;
    return TRUE;
}

HANDLE OpenEventLogA(const char* lpUNCServerName, const char* lpSourceName) {
    HANDLE h;
    (void)lpUNCServerName;
    (void)lpSourceName;
    if (!g_eventlog_initialized) eventlog_init_internal();
    if (g_event_open_count >= EVENTLOG_MAX_SOURCES) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    h = event_source_alloc_handle();
    g_event_open_handles[g_event_open_count++] = h;
    return h;
}

BOOL CloseEventLog(HANDLE hEventLog) {
    DWORD i;
    if (!g_eventlog_initialized) return FALSE;
    for (i = 0; i < g_event_open_count; i++) {
        if (g_event_open_handles[i] == hEventLog) {
            g_event_open_handles[i] = g_event_open_handles[--g_event_open_count];
            g_event_open_handles[g_event_open_count] = NULL;
            return TRUE;
        }
    }
    advapi_set_last_error(ERROR_INVALID_HANDLE);
    return FALSE;
}

#define EVENTLOG_FORWARDS_READ       0x0004
#define EVENTLOG_BACKWARDS_READ      0x0008
#define EVENTLOG_SEQUENTIAL_READ     0x0001
#define EVENTLOG_SEEK_READ           0x0002

BOOL ReadEventLogA(HANDLE hEventLog, DWORD dwReadFlags,
                   DWORD dwRecordOffset, LPVOID lpBuffer,
                   DWORD nNumberOfBytesToRead,
                   DWORD* pnBytesRead, DWORD* pnMinNumberOfBytesNeeded) {
    DWORD total_bytes;
    (void)hEventLog;
    (void)dwReadFlags;
    (void)dwRecordOffset;
    if (!g_eventlog_initialized) eventlog_init_internal();
    if (pnBytesRead == NULL || pnMinNumberOfBytesNeeded == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (g_event_record_count == 0) {
        *pnBytesRead = 0;
        *pnMinNumberOfBytesNeeded = 0;
        advapi_set_last_error(ERROR_NO_MORE_ITEMS);
        return FALSE;
    }
    total_bytes = g_event_record_count * sizeof(event_record_t);
    *pnMinNumberOfBytesNeeded = total_bytes;
    if (lpBuffer == NULL || nNumberOfBytesToRead < sizeof(event_record_t)) {
        *pnBytesRead = 0;
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    {
        DWORD rec_size = sizeof(event_record_t);
        DWORD count = nNumberOfBytesToRead / rec_size;
        if (count > g_event_record_count) count = g_event_record_count;
        if (count > 1) count = 1;
        *pnBytesRead = count * rec_size;
    }
    return TRUE;
}

BOOL ClearEventLogA(HANDLE hEventLog, const char* lpBackupFileName) {
    (void)hEventLog;
    (void)lpBackupFileName;
    if (!g_eventlog_initialized) eventlog_init_internal();
    g_event_buf_head = 0;
    g_event_buf_tail = 0;
    g_event_record_count = 0;
    return TRUE;
}

BOOL BackupEventLogA(HANDLE hEventLog, const char* lpBackupFileName) {
    (void)hEventLog;
    (void)lpBackupFileName;
    return TRUE;
}

/* ================================================================
 * 2. SERVICE API EXTENSIONS (complement scm.c)
 * ================================================================ */

#define SERVICE_QUERY_CONFIG        0x0001
#define SERVICE_CONFIG_DESCRIPTION  1
#define SERVICE_CONFIG_FAILURE_ACTIONS 2

typedef struct _QUERY_SERVICE_CONFIGA {
    DWORD dwServiceType;
    DWORD dwStartType;
    DWORD dwErrorControl;
    char  lpBinaryPathName[512];
    char  lpLoadOrderGroup[128];
    DWORD dwTagId;
    char  lpDependencies[1024];
    char  lpServiceStartName[64];
    char  lpDisplayName[256];
} QUERY_SERVICE_CONFIGA, *LPQUERY_SERVICE_CONFIGA;

typedef struct _ENUM_SERVICE_STATUSA {
    char          lpServiceName[256];
    char          lpDisplayName[256];
    struct {
        DWORD dwServiceType;
        DWORD dwCurrentState;
        DWORD dwControlsAccepted;
        DWORD dwWin32ExitCode;
        DWORD dwServiceSpecificExitCode;
        DWORD dwCheckPoint;
        DWORD dwWaitHint;
    } ServiceStatus;
} ENUM_SERVICE_STATUSA, *LPENUM_SERVICE_STATUSA;

typedef struct _SERVICE_TABLE_ENTRYA {
    char* lpServiceName;
    void* lpServiceProc;
} SERVICE_TABLE_ENTRYA, *LPSERVICE_TABLE_ENTRYA;

typedef struct _SERVICE_STATUS_PROCESS {
    DWORD dwServiceType;
    DWORD dwCurrentState;
    DWORD dwControlsAccepted;
    DWORD dwWin32ExitCode;
    DWORD dwServiceSpecificExitCode;
    DWORD dwCheckPoint;
    DWORD dwWaitHint;
    DWORD dwProcessId;
    DWORD dwServiceFlags;
} SERVICE_STATUS_PROCESS, *LPSERVICE_STATUS_PROCESS;

typedef uint32_t (*SERVICE_MAIN_FUNCTIONA)(uint32_t, char**);
typedef void (*SERVICE_HANDLER_PROC)(DWORD);

extern HANDLE OpenSCManagerA(const char*, const char*, uint32_t);
extern BOOL CloseServiceHandle(HANDLE);
extern HANDLE OpenServiceA(HANDLE, const char*, uint32_t);
extern BOOL QueryServiceStatus(HANDLE, void*);

typedef struct {
    uint32_t service_handle;
    char name[256];
    char display_name[256];
    uint32_t service_type;
    uint32_t start_type;
    uint32_t error_control;
    char binary_path[512];
    char load_order_group[128];
    uint32_t tag_id;
    char dependencies[1024];
    char service_start_name[64];
    char password[256];
    uint32_t state;
    uint32_t controls_accepted;
    uint32_t win32_exit_code;
    uint32_t service_specific_exit_code;
    uint32_t check_point;
    uint32_t wait_hint;
    uint32_t process_id;
    void* handler;
    void* main_func;
    void* user_data;
    uint64_t start_time;
    char description[1024];
} ext_scm_service_t;

#define EXT_SCM_MAX 1024
static ext_scm_service_t g_ext_services[EXT_SCM_MAX];
static HANDLE g_ext_scm_handle = 0;
static BOOL g_ext_scm_inited = FALSE;

static void ext_scm_init(void) {
    if (g_ext_scm_inited) return;
    memset(g_ext_services, 0, sizeof(g_ext_services));
    g_ext_scm_handle = (HANDLE)1u;
    g_ext_scm_inited = TRUE;
}

static ext_scm_service_t* ext_find_svc_by_handle(HANDLE h) {
    uint32_t i;
    uint32_t hv;
    if (h == NULL) return NULL;
    hv = (uint32_t)(uint64_t)h;
    for (i = 0; i < EXT_SCM_MAX; i++) {
        if (g_ext_services[i].service_handle == hv && g_ext_services[i].service_handle != 0) {
            return &g_ext_services[i];
        }
    }
    return NULL;
}

static ext_scm_service_t* ext_find_svc_by_name(const char* name) {
    uint32_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < EXT_SCM_MAX; i++) {
        if (g_ext_services[i].service_handle != 0 && strcmp(g_ext_services[i].name, name) == 0) {
            return &g_ext_services[i];
        }
    }
    return NULL;
}

static void ext_ensure_svc(HANDLE hService) {
    uint32_t i;
    uint32_t hv;
    ext_scm_service_t* s;
    if (hService == NULL) return;
    hv = (uint32_t)(uint64_t)hService;
    for (i = 0; i < EXT_SCM_MAX; i++) {
        if (g_ext_services[i].service_handle == hv) return;
    }
    for (i = 0; i < EXT_SCM_MAX; i++) {
        if (g_ext_services[i].service_handle == 0) {
            s = &g_ext_services[i];
            s->service_handle = hv;
            s->service_type = 0x10;
            s->start_type = 3;
            s->error_control = 1;
            strncpy(s->name, "UnknownService", sizeof(s->name) - 1);
            strncpy(s->display_name, "Unknown Service", sizeof(s->display_name) - 1);
            strncpy(s->binary_path, "C:\\Windows\\system32\\unknown.exe", sizeof(s->binary_path) - 1);
            strncpy(s->service_start_name, "LocalSystem", sizeof(s->service_start_name) - 1);
            s->state = 1;
            s->process_id = hv + 100;
            return;
        }
    }
}

BOOL QueryServiceConfigA(SC_HANDLE hService, void* lpServiceConfig,
                         DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
    ext_scm_service_t* s;
    LPQUERY_SERVICE_CONFIGA cfg;
    DWORD needed;
    size_t len;
    char* str_ptr;
    if (!g_ext_scm_inited) ext_scm_init();
    if (pcbBytesNeeded == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    ext_ensure_svc(hService);
    s = ext_find_svc_by_handle(hService);
    if (s == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    needed = sizeof(QUERY_SERVICE_CONFIGA);
    needed += (DWORD)strlen(s->binary_path) + 1;
    needed += (DWORD)strlen(s->load_order_group) + 1;
    needed += (DWORD)strlen(s->dependencies) + 2;
    needed += (DWORD)strlen(s->service_start_name) + 1;
    needed += (DWORD)strlen(s->display_name) + 1;
    *pcbBytesNeeded = needed;
    if (lpServiceConfig == NULL || cbBufSize < needed) {
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    cfg = (LPQUERY_SERVICE_CONFIGA)lpServiceConfig;
    memset(cfg, 0, cbBufSize);
    cfg->dwServiceType = s->service_type;
    cfg->dwStartType = s->start_type;
    cfg->dwErrorControl = s->error_control;
    cfg->dwTagId = s->tag_id;
    str_ptr = (char*)cfg + sizeof(QUERY_SERVICE_CONFIGA);
    len = strlen(s->binary_path) + 1;
    memcpy(str_ptr, s->binary_path, len);
    cfg->lpBinaryPathName[0] = 0;
    str_ptr += len;
    len = strlen(s->load_order_group) + 1;
    memcpy(str_ptr, s->load_order_group, len);
    str_ptr += len;
    len = strlen(s->dependencies) + 2;
    memcpy(str_ptr, s->dependencies, len);
    str_ptr += len;
    len = strlen(s->service_start_name) + 1;
    memcpy(str_ptr, s->service_start_name, len);
    str_ptr += len;
    len = strlen(s->display_name) + 1;
    memcpy(str_ptr, s->display_name, len);
    return TRUE;
}

BOOL ChangeServiceConfigA(SC_HANDLE hService, DWORD dwServiceType,
                          DWORD dwStartType, DWORD dwErrorControl,
                          const char* lpBinaryPathName,
                          const char* lpLoadOrderGroup,
                          LPDWORD lpdwTagId,
                          const char* lpDependencies,
                          const char* lpServiceStartName,
                          const char* lpPassword,
                          const char* lpDisplayName) {
    ext_scm_service_t* s;
    size_t len;
    (void)lpdwTagId;
    if (!g_ext_scm_inited) ext_scm_init();
    ext_ensure_svc(hService);
    s = ext_find_svc_by_handle(hService);
    if (s == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
#define SVC_NO_CHANGE 0xFFFFFFFFu
    if (dwServiceType != SVC_NO_CHANGE) s->service_type = dwServiceType;
    if (dwStartType != SVC_NO_CHANGE) s->start_type = dwStartType;
    if (dwErrorControl != SVC_NO_CHANGE) s->error_control = dwErrorControl;
    if (lpBinaryPathName != NULL) {
        len = strlen(lpBinaryPathName);
        if (len >= sizeof(s->binary_path)) len = sizeof(s->binary_path) - 1;
        memcpy(s->binary_path, lpBinaryPathName, len);
        s->binary_path[len] = 0;
    }
    if (lpLoadOrderGroup != NULL) {
        len = strlen(lpLoadOrderGroup);
        if (len >= sizeof(s->load_order_group)) len = sizeof(s->load_order_group) - 1;
        memcpy(s->load_order_group, lpLoadOrderGroup, len);
        s->load_order_group[len] = 0;
    }
    if (lpDependencies != NULL) {
        len = strlen(lpDependencies);
        if (len + 2u >= sizeof(s->dependencies)) len = sizeof(s->dependencies) - 2u;
        memcpy(s->dependencies, lpDependencies, len);
        s->dependencies[len] = 0;
        s->dependencies[len + 1u] = 0;
    }
    if (lpServiceStartName != NULL) {
        len = strlen(lpServiceStartName);
        if (len >= sizeof(s->service_start_name)) len = sizeof(s->service_start_name) - 1;
        memcpy(s->service_start_name, lpServiceStartName, len);
        s->service_start_name[len] = 0;
    }
    if (lpPassword != NULL) {
        len = strlen(lpPassword);
        if (len >= sizeof(s->password)) len = sizeof(s->password) - 1;
        memcpy(s->password, lpPassword, len);
        s->password[len] = 0;
    }
    if (lpDisplayName != NULL) {
        len = strlen(lpDisplayName);
        if (len >= sizeof(s->display_name)) len = sizeof(s->display_name) - 1;
        memcpy(s->display_name, lpDisplayName, len);
        s->display_name[len] = 0;
    }
    return TRUE;
}

typedef struct _SERVICE_DESCRIPTIONA {
    char* lpDescription;
} SERVICE_DESCRIPTIONA, *LPSERVICE_DESCRIPTIONA;

BOOL QueryServiceConfig2A(SC_HANDLE hService, DWORD dwInfoLevel,
                          LPBYTE lpBuffer, DWORD cbBufSize,
                          LPDWORD pcbBytesNeeded) {
    ext_scm_service_t* s;
    DWORD needed;
    if (!g_ext_scm_inited) ext_scm_init();
    if (pcbBytesNeeded == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    ext_ensure_svc(hService);
    s = ext_find_svc_by_handle(hService);
    if (s == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    needed = sizeof(SERVICE_DESCRIPTIONA) + (DWORD)strlen(s->description) + 1;
    *pcbBytesNeeded = needed;
    if (lpBuffer == NULL || cbBufSize < needed) {
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    if (dwInfoLevel == SERVICE_CONFIG_DESCRIPTION) {
        LPSERVICE_DESCRIPTIONA desc = (LPSERVICE_DESCRIPTIONA)lpBuffer;
        char* strp = (char*)lpBuffer + sizeof(SERVICE_DESCRIPTIONA);
        size_t dlen = strlen(s->description);
        memcpy(strp, s->description, dlen + 1);
        desc->lpDescription = strp;
    } else {
        memset(lpBuffer, 0, cbBufSize);
    }
    return TRUE;
}

BOOL ChangeServiceConfig2A(SC_HANDLE hService, DWORD dwInfoLevel,
                           LPVOID lpInfo) {
    ext_scm_service_t* s;
    if (!g_ext_scm_inited) ext_scm_init();
    ext_ensure_svc(hService);
    s = ext_find_svc_by_handle(hService);
    if (s == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (dwInfoLevel == SERVICE_CONFIG_DESCRIPTION && lpInfo != NULL) {
        LPSERVICE_DESCRIPTIONA desc = (LPSERVICE_DESCRIPTIONA)lpInfo;
        if (desc->lpDescription != NULL) {
            size_t dlen = strlen(desc->lpDescription);
            if (dlen >= sizeof(s->description)) dlen = sizeof(s->description) - 1;
            memcpy(s->description, desc->lpDescription, dlen);
            s->description[dlen] = 0;
        }
    }
    return TRUE;
}

BOOL EnumDependentServicesA(SC_HANDLE hService, DWORD dwServiceState,
                            void* lpServices, DWORD cbBufSize,
                            LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned) {
    (void)dwServiceState;
    if (!g_ext_scm_inited) ext_scm_init();
    if (pcbBytesNeeded == NULL || lpServicesReturned == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    ext_ensure_svc(hService);
    *pcbBytesNeeded = sizeof(ENUM_SERVICE_STATUSA);
    *lpServicesReturned = 0;
    if (lpServices == NULL || cbBufSize < sizeof(ENUM_SERVICE_STATUSA)) {
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    memset(lpServices, 0, cbBufSize);
    return TRUE;
}

BOOL StartServiceCtrlDispatcherA(const void* lpServiceStartTable) {
    const LPSERVICE_TABLE_ENTRYA table = (const LPSERVICE_TABLE_ENTRYA)lpServiceStartTable;
    if (!g_ext_scm_inited) ext_scm_init();
    if (table == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (table[0].lpServiceName == NULL && table[0].lpServiceProc == NULL) {
        advapi_set_last_error(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT);
        return FALSE;
    }
    return TRUE;
}

SC_HANDLE RegisterServiceCtrlHandlerA(const char* lpServiceName,
                                       void* lpHandlerProc) {
    ext_scm_service_t* s;
    if (!g_ext_scm_inited) ext_scm_init();
    if (lpServiceName == NULL || lpHandlerProc == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    s = ext_find_svc_by_name(lpServiceName);
    if (s != NULL) {
        s->handler = lpHandlerProc;
        return (SC_HANDLE)(uint64_t)s->service_handle;
    }
    return (SC_HANDLE)(uint64_t)0x2000u;
}

/* ================================================================
 * 3. CRYPTO API (CAPI) stubs
 * ================================================================ */

#define CRYPTO_MAX_PROVS 64
#define CRYPTO_MAX_KEYS 256
#define CRYPTO_MAX_HASHES 256

#define HP_HASHVAL  2
#define HP_HASHSIZE 4

#define PUBLICKEYBLOB  0x6
#define PRIVATEKEYBLOB 0x7
#define SIMPLEBLOB     0x1
#define PLAINTEXTKEYBLOB 0x8

#define CRYPTO_MAGIC 0x534e4547u

typedef struct {
    BOOL       used;
    HCRYPTPROV handle;
    char       container[128];
    char       provider[128];
    DWORD      prov_type;
    DWORD      flags;
    uint32_t   seed;
} crypto_prov_t;

typedef struct {
    BOOL       used;
    HCRYPTKEY  handle;
    HCRYPTPROV prov_handle;
    ALG_ID     algid;
    DWORD      flags;
    BYTE       key_material[64];
    DWORD      key_len;
} crypto_key_t;

typedef struct {
    BOOL       used;
    HCRYPTHASH handle;
    HCRYPTPROV prov_handle;
    ALG_ID     algid;
    DWORD      flags;
    union {
        DWORD md5_accum[4];
        DWORD sha_accum[8];
        BYTE  raw[64];
    } hash_state;
    DWORD      total_len;
    BYTE       final_hash[64];
    DWORD      hash_size;
} crypto_hash_t;

typedef struct _SIMPLEBLOBHEADER {
    BYTE  bType;
    BYTE  bVersion;
    WORD  reserved;
    ALG_ID aiKeyAlg;
} BLOBHEADER;

static crypto_prov_t g_crypto_provs[CRYPTO_MAX_PROVS];
static crypto_key_t g_crypto_keys[CRYPTO_MAX_KEYS];
static crypto_hash_t g_crypto_hashes[CRYPTO_MAX_HASHES];
static BOOL g_crypto_initialized = FALSE;
static uint32_t g_crypto_global_seed = 0x12345678u;

static void crypto_init_internal(void) {
    if (g_crypto_initialized) return;
    memset(g_crypto_provs, 0, sizeof(g_crypto_provs));
    memset(g_crypto_keys, 0, sizeof(g_crypto_keys));
    memset(g_crypto_hashes, 0, sizeof(g_crypto_hashes));
    g_crypto_initialized = TRUE;
}

static uint32_t crypto_rand32(void) {
    g_crypto_global_seed = g_crypto_global_seed * 1103515245u + 12345u;
    return g_crypto_global_seed;
}

static uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

BOOL CryptAcquireContextA(HCRYPTPROV* phProv, const char* pszContainer,
                          const char* pszProvider, DWORD dwProvType,
                          DWORD dwFlags) {
    DWORD i;
    crypto_prov_t* p;
    if (!g_crypto_initialized) crypto_init_internal();
    if (phProv == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < CRYPTO_MAX_PROVS; i++) {
        if (!g_crypto_provs[i].used) break;
    }
    if (i >= CRYPTO_MAX_PROVS) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    p = &g_crypto_provs[i];
    memset(p, 0, sizeof(*p));
    p->used = TRUE;
    p->handle = (HCRYPTPROV)(uintptr_t)(0x3000u + i);
    if (pszContainer != NULL) {
        strncpy(p->container, pszContainer, sizeof(p->container) - 1);
    }
    if (pszProvider != NULL) {
        strncpy(p->provider, pszProvider, sizeof(p->provider) - 1);
    }
    p->prov_type = dwProvType;
    p->flags = dwFlags;
    p->seed = (uint32_t)read_tsc();
    *phProv = p->handle;
    return TRUE;
}

BOOL CryptReleaseContext(HCRYPTPROV hProv, DWORD dwFlags) {
    DWORD i;
    (void)dwFlags;
    if (!g_crypto_initialized) return FALSE;
    for (i = 0; i < CRYPTO_MAX_PROVS; i++) {
        if (g_crypto_provs[i].used && g_crypto_provs[i].handle == hProv) {
            memset(&g_crypto_provs[i], 0, sizeof(crypto_prov_t));
            return TRUE;
        }
    }
    advapi_set_last_error(ERROR_INVALID_HANDLE);
    return FALSE;
}

static crypto_prov_t* find_prov(HCRYPTPROV h) {
    DWORD i;
    for (i = 0; i < CRYPTO_MAX_PROVS; i++) {
        if (g_crypto_provs[i].used && g_crypto_provs[i].handle == h) {
            return &g_crypto_provs[i];
        }
    }
    return NULL;
}

BOOL CryptGenKey(HCRYPTPROV hProv, ALG_ID Algid, DWORD dwFlags,
                 HCRYPTKEY* phKey) {
    DWORD i;
    crypto_prov_t* p;
    crypto_key_t* k;
    if (!g_crypto_initialized) crypto_init_internal();
    p = find_prov(hProv);
    if (p == NULL || phKey == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < CRYPTO_MAX_KEYS; i++) {
        if (!g_crypto_keys[i].used) break;
    }
    if (i >= CRYPTO_MAX_KEYS) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    k = &g_crypto_keys[i];
    memset(k, 0, sizeof(*k));
    k->used = TRUE;
    k->handle = (HCRYPTKEY)(uintptr_t)(0x4000u + i);
    k->prov_handle = hProv;
    k->algid = Algid;
    k->flags = dwFlags;
    k->key_len = 32;
    {
        DWORD j;
        for (j = 0; j < k->key_len; j++) {
            k->key_material[j] = (BYTE)(crypto_rand32() & 0xFF);
        }
    }
    *phKey = k->handle;
    return TRUE;
}

BOOL CryptDeriveKey(HCRYPTPROV hProv, ALG_ID Algid, HCRYPTHASH hBaseData,
                    DWORD dwFlags, HCRYPTKEY* phKey) {
    DWORD i, j;
    crypto_prov_t* p;
    crypto_key_t* k;
    crypto_hash_t* h;
    (void)hBaseData;
    if (!g_crypto_initialized) crypto_init_internal();
    p = find_prov(hProv);
    if (p == NULL || phKey == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    h = NULL;
    for (i = 0; i < CRYPTO_MAX_HASHES; i++) {
        if (g_crypto_hashes[i].used && g_crypto_hashes[i].handle == hBaseData) {
            h = &g_crypto_hashes[i];
            break;
        }
    }
    for (i = 0; i < CRYPTO_MAX_KEYS; i++) {
        if (!g_crypto_keys[i].used) break;
    }
    if (i >= CRYPTO_MAX_KEYS) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    k = &g_crypto_keys[i];
    memset(k, 0, sizeof(*k));
    k->used = TRUE;
    k->handle = (HCRYPTKEY)(uintptr_t)(0x4000u + i);
    k->prov_handle = hProv;
    k->algid = Algid;
    k->flags = dwFlags;
    k->key_len = 32;
    if (h != NULL) {
        DWORD cpsz = (h->hash_size > 0) ? h->hash_size : 32;
        if (cpsz > k->key_len) cpsz = k->key_len;
        for (j = 0; j < cpsz; j++) {
            k->key_material[j] = h->final_hash[j];
        }
        for (; j < k->key_len; j++) {
            k->key_material[j] = (BYTE)(crypto_rand32() & 0xFF);
        }
    } else {
        for (j = 0; j < k->key_len; j++) {
            k->key_material[j] = (BYTE)(crypto_rand32() & 0xFF);
        }
    }
    *phKey = k->handle;
    return TRUE;
}

static crypto_key_t* find_key(HCRYPTKEY h) {
    DWORD i;
    for (i = 0; i < CRYPTO_MAX_KEYS; i++) {
        if (g_crypto_keys[i].used && g_crypto_keys[i].handle == h) {
            return &g_crypto_keys[i];
        }
    }
    return NULL;
}

BOOL CryptDestroyKey(HCRYPTKEY hKey) {
    crypto_key_t* k = find_key(hKey);
    if (k == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    memset(k, 0, sizeof(*k));
    return TRUE;
}

BOOL CryptExportKey(HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType,
                    DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen) {
    crypto_key_t* k;
    DWORD blob_size;
    BLOBHEADER* hdr;
    (void)hExpKey;
    (void)dwFlags;
    k = find_key(hKey);
    if (k == NULL || pdwDataLen == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    blob_size = sizeof(BLOBHEADER) + sizeof(DWORD) + k->key_len;
    if (pbData == NULL || *pdwDataLen < blob_size) {
        *pdwDataLen = blob_size;
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    hdr = (BLOBHEADER*)pbData;
    hdr->bType = (BYTE)dwBlobType;
    hdr->bVersion = 2;
    hdr->reserved = 0;
    hdr->aiKeyAlg = k->algid;
    {
        DWORD* plen = (DWORD*)(pbData + sizeof(BLOBHEADER));
        *plen = k->key_len;
    }
    memcpy(pbData + sizeof(BLOBHEADER) + sizeof(DWORD), k->key_material, k->key_len);
    *pdwDataLen = blob_size;
    return TRUE;
}

BOOL CryptImportKey(HCRYPTPROV hProv, const BYTE* pbData, DWORD dwDataLen,
                    HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey) {
    crypto_prov_t* p;
    crypto_key_t* k;
    BLOBHEADER* hdr;
    DWORD i;
    DWORD* plen;
    (void)hPubKey;
    if (!g_crypto_initialized) crypto_init_internal();
    p = find_prov(hProv);
    if (p == NULL || pbData == NULL || phKey == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (dwDataLen < sizeof(BLOBHEADER) + sizeof(DWORD)) {
        advapi_set_last_error(ERROR_INVALID_DATA);
        return FALSE;
    }
    hdr = (BLOBHEADER*)pbData;
    plen = (DWORD*)(pbData + sizeof(BLOBHEADER));
    for (i = 0; i < CRYPTO_MAX_KEYS; i++) {
        if (!g_crypto_keys[i].used) break;
    }
    if (i >= CRYPTO_MAX_KEYS) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    k = &g_crypto_keys[i];
    memset(k, 0, sizeof(*k));
    k->used = TRUE;
    k->handle = (HCRYPTKEY)(uintptr_t)(0x4000u + i);
    k->prov_handle = hProv;
    k->algid = hdr->aiKeyAlg;
    k->flags = dwFlags;
    k->key_len = (*plen > sizeof(k->key_material)) ? sizeof(k->key_material) : *plen;
    if (sizeof(BLOBHEADER) + sizeof(DWORD) + k->key_len <= dwDataLen) {
        memcpy(k->key_material, pbData + sizeof(BLOBHEADER) + sizeof(DWORD), k->key_len);
    }
    *phKey = k->handle;
    return TRUE;
}

BOOL CryptEncrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
                  DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen,
                  DWORD dwBufLen) {
    crypto_key_t* k;
    DWORD i, ki;
    (void)hHash;
    (void)Final;
    (void)dwFlags;
    k = find_key(hKey);
    if (k == NULL || pdwDataLen == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pbData == NULL) {
        *pdwDataLen = *pdwDataLen;
        return TRUE;
    }
    if (dwBufLen < *pdwDataLen) {
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    ki = 0;
    for (i = 0; i < *pdwDataLen; i++) {
        pbData[i] ^= k->key_material[ki % k->key_len];
        ki++;
    }
    return TRUE;
}

BOOL CryptDecrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
                  DWORD dwFlags, BYTE* pbData, DWORD* pdwDataLen) {
    crypto_key_t* k;
    DWORD i, ki;
    (void)hHash;
    (void)Final;
    (void)dwFlags;
    k = find_key(hKey);
    if (k == NULL || pdwDataLen == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (pbData == NULL) return TRUE;
    ki = 0;
    for (i = 0; i < *pdwDataLen; i++) {
        pbData[i] ^= k->key_material[ki % k->key_len];
        ki++;
    }
    return TRUE;
}

static crypto_hash_t* find_hash(HCRYPTHASH h) {
    DWORD i;
    for (i = 0; i < CRYPTO_MAX_HASHES; i++) {
        if (g_crypto_hashes[i].used && g_crypto_hashes[i].handle == h) {
            return &g_crypto_hashes[i];
        }
    }
    return NULL;
}

BOOL CryptCreateHash(HCRYPTPROV hProv, ALG_ID Algid, HCRYPTKEY hKey,
                     DWORD dwFlags, HCRYPTHASH* phHash) {
    DWORD i;
    crypto_prov_t* p;
    crypto_hash_t* h;
    (void)hKey;
    if (!g_crypto_initialized) crypto_init_internal();
    p = find_prov(hProv);
    if (p == NULL || phHash == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < CRYPTO_MAX_HASHES; i++) {
        if (!g_crypto_hashes[i].used) break;
    }
    if (i >= CRYPTO_MAX_HASHES) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    h = &g_crypto_hashes[i];
    memset(h, 0, sizeof(*h));
    h->used = TRUE;
    h->handle = (HCRYPTHASH)(uintptr_t)(0x5000u + i);
    h->prov_handle = hProv;
    h->algid = Algid;
    h->flags = dwFlags;
    h->total_len = 0;
    switch (Algid) {
        case CALG_MD5:
            h->hash_size = 16;
            h->hash_state.md5_accum[0] = 0x67452301u;
            h->hash_state.md5_accum[1] = 0xefcdab89u;
            h->hash_state.md5_accum[2] = 0x98badcfeu;
            h->hash_state.md5_accum[3] = 0x10325476u;
            break;
        case CALG_SHA1:
            h->hash_size = 20;
            h->hash_state.sha_accum[0] = 0x67452301u;
            h->hash_state.sha_accum[1] = 0xefcdab89u;
            h->hash_state.sha_accum[2] = 0x98badcfeu;
            h->hash_state.sha_accum[3] = 0x10325476u;
            h->hash_state.sha_accum[4] = 0xc3d2e1f0u;
            break;
        case CALG_SHA_256:
        default:
            h->hash_size = 32;
            h->hash_state.sha_accum[0] = 0x6a09e667u;
            h->hash_state.sha_accum[1] = 0xbb67ae85u;
            h->hash_state.sha_accum[2] = 0x3c6ef372u;
            h->hash_state.sha_accum[3] = 0xa54ff53au;
            h->hash_state.sha_accum[4] = 0x510e527fu;
            h->hash_state.sha_accum[5] = 0x9b05688cu;
            h->hash_state.sha_accum[6] = 0x1f83d9abu;
            h->hash_state.sha_accum[7] = 0x5be0cd19u;
            break;
    }
    *phHash = h->handle;
    return TRUE;
}

BOOL CryptHashData(HCRYPTHASH hHash, const BYTE* pbData,
                   DWORD dwDataLen, DWORD dwFlags) {
    crypto_hash_t* h;
    DWORD i;
    BYTE b;
    (void)dwFlags;
    h = find_hash(hHash);
    if (h == NULL || pbData == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < dwDataLen; i++) {
        b = pbData[i];
        switch (h->algid) {
            case CALG_MD5:
                h->hash_state.md5_accum[0] ^= (b * 0x01010101u) + h->total_len;
                h->hash_state.md5_accum[1] += (b << 8) | (b >> 0);
                h->hash_state.md5_accum[2] ^= h->hash_state.md5_accum[0];
                h->hash_state.md5_accum[3] += h->hash_state.md5_accum[1] ^ b;
                break;
            case CALG_SHA1:
                h->hash_state.sha_accum[0] += (b * 0x01010101u);
                h->hash_state.sha_accum[1] ^= b + h->total_len;
                h->hash_state.sha_accum[2] = (h->hash_state.sha_accum[2] << 5) | (h->hash_state.sha_accum[2] >> 27);
                h->hash_state.sha_accum[3] += b;
                h->hash_state.sha_accum[4] ^= h->hash_state.sha_accum[0];
                break;
            case CALG_SHA_256:
            default:
                h->hash_state.sha_accum[0] ^= (b * 0x01010101u) + h->total_len;
                h->hash_state.sha_accum[1] += (b << 3) | (b >> 5);
                h->hash_state.sha_accum[2] ^= h->hash_state.sha_accum[1];
                h->hash_state.sha_accum[3] += b * 31u;
                h->hash_state.sha_accum[4] ^= (b + 0x9E3779B9u);
                h->hash_state.sha_accum[5] += h->hash_state.sha_accum[0] ^ b;
                h->hash_state.sha_accum[6] ^= h->hash_state.sha_accum[3];
                h->hash_state.sha_accum[7] += (h->hash_state.sha_accum[5] >> 3) + b;
                break;
        }
        h->total_len++;
    }
    return TRUE;
}

BOOL CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam,
                       BYTE* pbData, DWORD* pdwDataLen, DWORD dwFlags) {
    crypto_hash_t* h;
    DWORD i;
    (void)dwFlags;
    h = find_hash(hHash);
    if (h == NULL || pdwDataLen == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (dwParam == HP_HASHSIZE) {
        if (*pdwDataLen < sizeof(DWORD)) {
            *pdwDataLen = sizeof(DWORD);
            advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        *((DWORD*)pbData) = h->hash_size;
        *pdwDataLen = sizeof(DWORD);
        return TRUE;
    }
    if (dwParam == HP_HASHVAL) {
        if (h->hash_size == 0) {
            for (i = 0; i < 8 && i * 4 < sizeof(h->final_hash); i++) {
                ((DWORD*)h->final_hash)[i] = h->hash_state.sha_accum[i];
            }
            if (h->algid == CALG_MD5) h->hash_size = 16;
            else if (h->algid == CALG_SHA1) h->hash_size = 20;
            else h->hash_size = 32;
        }
        if (pbData == NULL || *pdwDataLen < h->hash_size) {
            *pdwDataLen = h->hash_size;
            advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        if (h->hash_state.raw[0] == 0 && h->hash_state.raw[1] == 0) {
            for (i = 0; i < 8 && i * 4 < sizeof(h->final_hash); i++) {
                ((DWORD*)h->final_hash)[i] = h->hash_state.sha_accum[i];
            }
        }
        memcpy(pbData, h->final_hash, h->hash_size);
        *pdwDataLen = h->hash_size;
        return TRUE;
    }
    advapi_set_last_error(ERROR_INVALID_PARAMETER);
    return FALSE;
}

BOOL CryptDestroyHash(HCRYPTHASH hHash) {
    crypto_hash_t* h = find_hash(hHash);
    if (h == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    memset(h, 0, sizeof(*h));
    return TRUE;
}

BOOL CryptGenRandom(HCRYPTPROV hProv, DWORD dwLen, BYTE* pbBuffer) {
    crypto_prov_t* p;
    DWORD i;
    uint64_t tsc;
    (void)hProv;
    if (!g_crypto_initialized) crypto_init_internal();
    if (pbBuffer == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    p = find_prov(hProv);
    tsc = read_tsc();
    for (i = 0; i < dwLen; i++) {
        uint32_t r = crypto_rand32();
        if (p != NULL) {
            p->seed = p->seed * 1664525u + 1013904223u;
            r ^= p->seed;
        }
        r ^= (uint32_t)(tsc >> (i % 32));
        pbBuffer[i] = (BYTE)(r & 0xFF);
    }
    return TRUE;
}

static const BYTE g_crypt_mask[32] = {
    0x5A, 0x3C, 0x7E, 0x12, 0x98, 0xF4, 0xB6, 0xD0,
    0x24, 0x68, 0xAE, 0xE2, 0x40, 0x0C, 0x8A, 0x56,
    0x1F, 0x7B, 0x3D, 0xC9, 0x95, 0xE1, 0x27, 0x8F,
    0x4B, 0xA3, 0x05, 0x6D, 0xB1, 0xDE, 0x52, 0x9C
};

typedef struct _CRYPTOAPI_BLOB {
    DWORD cbData;
    BYTE* pbData;
} CRYPTOAPI_BLOB, DATA_BLOB, *PDATA_BLOB;

typedef struct _CRYPTPROTECT_PROMPTSTRUCT {
    DWORD cbSize;
    DWORD dwPromptFlags;
    HWND  hwndApp;
    char* szPrompt;
} CRYPTPROTECT_PROMPTSTRUCT, *PCRYPTPROTECT_PROMPTSTRUCT;

BOOL CryptProtectData(const void* pDataIn, const char* szDataDescr,
                      const void* pOptionalEntropy, PVOID pvReserved,
                      const void* pPromptStruct, DWORD dwFlags,
                      void* pDataOut) {
    const DATA_BLOB* in = (const DATA_BLOB*)pDataIn;
    DATA_BLOB* out = (DATA_BLOB*)pDataOut;
    DWORD i;
    (void)szDataDescr;
    (void)pOptionalEntropy;
    (void)pvReserved;
    (void)pPromptStruct;
    (void)dwFlags;
    if (in == NULL || out == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (in->pbData != NULL && in->cbData > 0) {
        for (i = 0; i < in->cbData; i++) {
            in->pbData[i] ^= g_crypt_mask[i % sizeof(g_crypt_mask)];
        }
    }
    out->cbData = in->cbData;
    out->pbData = in->pbData;
    return TRUE;
}

BOOL CryptUnprotectData(const void* pDataIn, char** ppszDataDescr,
                        void* pOptionalEntropy, PVOID pvReserved,
                        void* pPromptStruct, DWORD dwFlags,
                        void* pDataOut) {
    const DATA_BLOB* in = (const DATA_BLOB*)pDataIn;
    DATA_BLOB* out = (DATA_BLOB*)pDataOut;
    DWORD i;
    (void)pOptionalEntropy;
    (void)pvReserved;
    (void)pPromptStruct;
    (void)dwFlags;
    if (in == NULL || out == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (in->pbData != NULL && in->cbData > 0) {
        for (i = 0; i < in->cbData; i++) {
            in->pbData[i] ^= g_crypt_mask[i % sizeof(g_crypt_mask)];
        }
    }
    out->cbData = in->cbData;
    out->pbData = in->pbData;
    if (ppszDataDescr != NULL) {
        *ppszDataDescr = NULL;
    }
    return TRUE;
}

/* ================================================================
 * 4. CREDENTIAL MANAGEMENT - simple memory array
 * ================================================================ */

#define CRED_MAX 256
#define CRED_TYPE_GENERIC 1
#define CRED_TYPE_DOMAIN_PASSWORD 2

typedef struct _CREDENTIALA {
    DWORD Flags;
    DWORD Type;
    char* TargetName;
    char* Comment;
    DWORD LastWritten;
    DWORD CredentialBlobSize;
    BYTE* CredentialBlob;
    DWORD Persist;
    DWORD AttributeCount;
    void* Attributes;
    char* TargetAlias;
    char* UserName;
} CREDENTIALA, *PCREDENTIALA;

typedef struct {
    BOOL         used;
    DWORD        type;
    char         target_name[256];
    char         comment[256];
    char         user_name[128];
    char         target_alias[128];
    DWORD        last_written;
    DWORD        persist;
    DWORD        blob_size;
    BYTE         blob[512];
} cred_entry_t;

static cred_entry_t g_cred_table[CRED_MAX];
static BOOL g_cred_initialized = FALSE;

static void cred_init_internal(void) {
    if (g_cred_initialized) return;
    memset(g_cred_table, 0, sizeof(g_cred_table));
    g_cred_initialized = TRUE;
}

static cred_entry_t* cred_find(const char* target, DWORD type) {
    DWORD i;
    if (target == NULL) return NULL;
    for (i = 0; i < CRED_MAX; i++) {
        if (g_cred_table[i].used &&
            g_cred_table[i].type == type &&
            strcmp(g_cred_table[i].target_name, target) == 0) {
            return &g_cred_table[i];
        }
    }
    return NULL;
}

BOOL CredReadA(const char* TargetName, DWORD Type, DWORD Flags,
               void** Credential) {
    cred_entry_t* e;
    PCREDENTIALA c;
    (void)Flags;
    if (!g_cred_initialized) cred_init_internal();
    if (TargetName == NULL || Credential == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    e = cred_find(TargetName, Type);
    if (e == NULL) {
        advapi_set_last_error(ERROR_NOT_FOUND);
        return FALSE;
    }
    c = (PCREDENTIALA)memory_alloc(sizeof(CREDENTIALA) +
                                   strlen(e->target_name) + 1 +
                                   strlen(e->comment) + 1 +
                                   strlen(e->user_name) + 1 +
                                   strlen(e->target_alias) + 1 +
                                   e->blob_size);
    if (c == NULL) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memset(c, 0, sizeof(CREDENTIALA) + strlen(e->target_name) + 1 +
               strlen(e->comment) + 1 + strlen(e->user_name) + 1 +
               strlen(e->target_alias) + 1 + e->blob_size);
    {
        char* p = (char*)c + sizeof(CREDENTIALA);
        c->TargetName = p;
        strcpy(p, e->target_name);
        p += strlen(p) + 1;
        c->Comment = p;
        strcpy(p, e->comment);
        p += strlen(p) + 1;
        c->UserName = p;
        strcpy(p, e->user_name);
        p += strlen(p) + 1;
        c->TargetAlias = p;
        strcpy(p, e->target_alias);
        p += strlen(p) + 1;
        c->CredentialBlob = (BYTE*)p;
        if (e->blob_size > 0) memcpy(c->CredentialBlob, e->blob, e->blob_size);
    }
    c->Flags = 0;
    c->Type = e->type;
    c->LastWritten = e->last_written;
    c->Persist = e->persist;
    c->CredentialBlobSize = e->blob_size;
    c->AttributeCount = 0;
    *Credential = c;
    return TRUE;
}

BOOL CredWriteA(const void* Credential, DWORD Flags) {
    const PCREDENTIALA c = (const PCREDENTIALA)Credential;
    cred_entry_t* e;
    DWORD i;
    size_t len;
    (void)Flags;
    if (!g_cred_initialized) cred_init_internal();
    if (c == NULL || c->TargetName == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    e = cred_find(c->TargetName, c->Type);
    if (e == NULL) {
        for (i = 0; i < CRED_MAX; i++) {
            if (!g_cred_table[i].used) {
                e = &g_cred_table[i];
                memset(e, 0, sizeof(*e));
                e->used = TRUE;
                break;
            }
        }
    }
    if (e == NULL) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    e->type = c->Type;
    len = strlen(c->TargetName);
    if (len >= sizeof(e->target_name)) len = sizeof(e->target_name) - 1;
    memcpy(e->target_name, c->TargetName, len);
    e->target_name[len] = 0;
    if (c->Comment != NULL) {
        len = strlen(c->Comment);
        if (len >= sizeof(e->comment)) len = sizeof(e->comment) - 1;
        memcpy(e->comment, c->Comment, len);
        e->comment[len] = 0;
    } else e->comment[0] = 0;
    if (c->UserName != NULL) {
        len = strlen(c->UserName);
        if (len >= sizeof(e->user_name)) len = sizeof(e->user_name) - 1;
        memcpy(e->user_name, c->UserName, len);
        e->user_name[len] = 0;
    } else e->user_name[0] = 0;
    if (c->TargetAlias != NULL) {
        len = strlen(c->TargetAlias);
        if (len >= sizeof(e->target_alias)) len = sizeof(e->target_alias) - 1;
        memcpy(e->target_alias, c->TargetAlias, len);
        e->target_alias[len] = 0;
    } else e->target_alias[0] = 0;
    e->persist = c->Persist;
    e->last_written = (DWORD)GetTickCount64();
    e->blob_size = (c->CredentialBlobSize > sizeof(e->blob)) ? sizeof(e->blob) : c->CredentialBlobSize;
    if (c->CredentialBlob != NULL && e->blob_size > 0) {
        memcpy(e->blob, c->CredentialBlob, e->blob_size);
    }
    return TRUE;
}

BOOL CredDeleteA(const char* TargetName, DWORD Type, DWORD Flags) {
    cred_entry_t* e;
    (void)Flags;
    if (!g_cred_initialized) cred_init_internal();
    if (TargetName == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    e = cred_find(TargetName, Type);
    if (e == NULL) {
        advapi_set_last_error(ERROR_NOT_FOUND);
        return FALSE;
    }
    memset(e, 0, sizeof(*e));
    return TRUE;
}

BOOL CredFree(void* Buffer) {
    if (Buffer == NULL) return FALSE;
    memory_free(Buffer);
    return TRUE;
}

BOOL CredEnumerateA(const char* Filter, DWORD Flags,
                    DWORD* Count, void*** Credentials) {
    DWORD i, found;
    void** arr;
    (void)Filter;
    (void)Flags;
    if (!g_cred_initialized) cred_init_internal();
    if (Count == NULL || Credentials == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    found = 0;
    for (i = 0; i < CRED_MAX; i++) {
        if (g_cred_table[i].used) found++;
    }
    *Count = found;
    if (found == 0) {
        *Credentials = NULL;
        return TRUE;
    }
    arr = (void**)memory_alloc(sizeof(void*) * found);
    if (arr == NULL) {
        advapi_set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memset(arr, 0, sizeof(void*) * found);
    {
        DWORD idx = 0;
        for (i = 0; i < CRED_MAX && idx < found; i++) {
            if (g_cred_table[i].used) {
                cred_entry_t* e = &g_cred_table[i];
                PCREDENTIALA c = (PCREDENTIALA)memory_alloc(sizeof(CREDENTIALA) + 4096);
                if (c == NULL) continue;
                memset(c, 0, sizeof(CREDENTIALA) + 4096);
                {
                    char* p = (char*)c + sizeof(CREDENTIALA);
                    c->TargetName = p;
                    strcpy(p, e->target_name);
                    p += strlen(p) + 1;
                    c->Comment = p;
                    strcpy(p, e->comment);
                    p += strlen(p) + 1;
                    c->UserName = p;
                    strcpy(p, e->user_name);
                    p += strlen(p) + 1;
                    c->TargetAlias = p;
                    strcpy(p, e->target_alias);
                    p += strlen(p) + 1;
                    c->CredentialBlob = (BYTE*)p;
                    if (e->blob_size > 0) memcpy(c->CredentialBlob, e->blob, e->blob_size);
                }
                c->Type = e->type;
                c->LastWritten = e->last_written;
                c->Persist = e->persist;
                c->CredentialBlobSize = e->blob_size;
                arr[idx++] = c;
            }
        }
    }
    *Credentials = arr;
    return TRUE;
}

/* ================================================================
 * 5. POWER / SHUTDOWN - use QEMU exit port
 * ================================================================ */

#define QEMU_EXIT_PORT      0x604
#define QEMU_REBOOT_VALUE   0x2000
#define QEMU_POWEROFF_VALUE 0x2001

BOOL InitiateSystemShutdownA(const char* lpMachineName,
                             const char* lpMessage,
                             DWORD dwTimeout, BOOL bForceAppsClosed,
                             BOOL bRebootAfterShutdown) {
    (void)lpMachineName;
    (void)lpMessage;
    (void)dwTimeout;
    (void)bForceAppsClosed;
    if (bRebootAfterShutdown) {
        outw(QEMU_EXIT_PORT, QEMU_REBOOT_VALUE);
    } else {
        outw(QEMU_EXIT_PORT, QEMU_POWEROFF_VALUE);
    }
    return TRUE;
}

BOOL AbortSystemShutdownA(const char* lpMachineName) {
    (void)lpMachineName;
    return TRUE;
}

BOOL ExitWindowsEx(UINT uFlags, DWORD dwReason) {
    (void)dwReason;
    if (uFlags & EWX_REBOOT) {
        outw(QEMU_EXIT_PORT, QEMU_REBOOT_VALUE);
    } else {
        outw(QEMU_EXIT_PORT, QEMU_POWEROFF_VALUE);
    }
    return TRUE;
}

/* ================================================================
 * 6. TOKENS / AUTH HELPERS
 * ================================================================ */


typedef struct {
    BOOL   used;
    HANDLE handle;
    BOOL   is_process;
    DWORD  access;
    DWORD  token_type;
    DWORD  impersonation_level;
    DWORD  session_id;
    BYTE   user_sid[68];
    DWORD  user_sid_len;
    char   user_name[64];
    char   domain_name[64];
    DWORD  privilege_count;
    struct {
        LUID  luid;
        DWORD attrs;
        char  name[64];
    } privileges[32];
} token_entry_t;
#define TOKEN_ASSIGN_PRIMARY    0x0001
#define TOKEN_DUPLICATE         0x0002
#define TOKEN_IMPERSONATE       0x0004
#define TOKEN_QUERY             0x0008
#define TOKEN_QUERY_SOURCE      0x0010
#define TOKEN_ADJUST_PRIVILEGES 0x0020
#define TOKEN_ADJUST_GROUPS     0x0040
#define TOKEN_ADJUST_DEFAULT    0x0080
#define TOKEN_ADJUST_SESSIONID  0x0100
#define TOKEN_ALL_ACCESS        0xF01FF

#define SE_PRIVILEGE_ENABLED    0x00000002L

#define TOKEN_MAX 64

static token_entry_t g_token_table[TOKEN_MAX];
static BOOL g_tokens_initialized = FALSE;

static void tokens_init_internal(void) {
    DWORD i, j;
    if (g_tokens_initialized) return;
    memset(g_token_table, 0, sizeof(g_token_table));
    for (i = 0; i < 4; i++) {
        token_entry_t* t = &g_token_table[i];
        t->used = TRUE;
        t->handle = (HANDLE)(uintptr_t)(0x6000u + i);
        t->is_process = TRUE;
        t->access = TOKEN_ALL_ACCESS;
        t->token_type = 1;
        t->impersonation_level = 0;
        t->session_id = 1;
        strncpy(t->user_name, "SYSTEM", sizeof(t->user_name) - 1);
        strncpy(t->domain_name, "NT AUTHORITY", sizeof(t->domain_name) - 1);
        t->user_sid_len = 12;
        t->user_sid[0] = 1;
        t->user_sid[1] = 1;
        ((DWORD*)&t->user_sid[4])[0] = 0x515u;
        ((DWORD*)&t->user_sid[8])[0] = 0x12u;
        t->privilege_count = 0;
        for (j = 0; j < 16; j++) {
            t->privileges[t->privilege_count].luid.LowPart = 2u + j;
            t->privileges[t->privilege_count].luid.HighPart = 0;
            t->privileges[t->privilege_count].attrs = SE_PRIVILEGE_ENABLED;
            snprintf(t->privileges[t->privilege_count].name,
                     sizeof(t->privileges[t->privilege_count].name),
                     "SePrivilege%u", j);
            t->privilege_count++;
        }
    }
    g_tokens_initialized = TRUE;
}

static token_entry_t* token_find(HANDLE h) {
    DWORD i;
    for (i = 0; i < TOKEN_MAX; i++) {
        if (g_token_table[i].used && g_token_table[i].handle == h) {
            return &g_token_table[i];
        }
    }
    return NULL;
}

typedef struct _SID_AND_ATTRIBUTES {
    void* Sid;
    DWORD Attributes;
} SID_AND_ATTRIBUTES;

typedef struct _TOKEN_USER {
    SID_AND_ATTRIBUTES User;
} TOKEN_USER, *PTOKEN_USER;

typedef struct _TOKEN_OWNER {
    void* Owner;
} TOKEN_OWNER, *PTOKEN_OWNER;

typedef struct _TOKEN_PRIMARY_GROUP {
    void* PrimaryGroup;
} TOKEN_PRIMARY_GROUP, *PTOKEN_PRIMARY_GROUP;

typedef struct _TOKEN_PRIVILEGES {
    DWORD PrivilegeCount;
    struct {
        LUID  Luid;
        DWORD Attributes;
    } Privileges[1];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

typedef struct _TOKEN_STATISTICS {
    LUID   TokenId;
    LUID   AuthenticationId;
    LONGLONG ExpirationTime;
    DWORD  TokenType;
    DWORD  ImpersonationLevel;
    DWORD  DynamicCharged;
    DWORD  DynamicAvailable;
    DWORD  GroupCount;
    DWORD  PrivilegeCount;
    LUID   ModifiedId;
} TOKEN_STATISTICS, *PTOKEN_STATISTICS;

#define TOKEN_INFORMATION_CLASS_USER              1
#define TOKEN_INFORMATION_CLASS_GROUPS           2
#define TOKEN_INFORMATION_CLASS_PRIVILEGES       3
#define TOKEN_INFORMATION_CLASS_OWNER            4
#define TOKEN_INFORMATION_CLASS_PRIMARY_GROUP    5
#define TOKEN_INFORMATION_CLASS_DEFAULT_DACL     6
#define TOKEN_INFORMATION_CLASS_SOURCE           7
#define TOKEN_INFORMATION_CLASS_TYPE             8
#define TOKEN_INFORMATION_CLASS_IMPERSONATION_LEVEL 9
#define TOKEN_INFORMATION_CLASS_STATISTICS       10

BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess,
                      PHANDLE TokenHandle) {
    DWORD i;
    (void)ProcessHandle;
    if (!g_tokens_initialized) tokens_init_internal();
    if (TokenHandle == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < TOKEN_MAX; i++) {
        if (g_token_table[i].used && g_token_table[i].is_process) {
            *TokenHandle = g_token_table[i].handle;
            g_token_table[i].access = DesiredAccess;
            return TRUE;
        }
    }
    advapi_set_last_error(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL OpenThreadToken(HANDLE ThreadHandle, DWORD DesiredAccess,
                     BOOL OpenAsSelf, PHANDLE TokenHandle) {
    DWORD i;
    (void)ThreadHandle;
    (void)OpenAsSelf;
    if (!g_tokens_initialized) tokens_init_internal();
    if (TokenHandle == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < TOKEN_MAX; i++) {
        if (g_token_table[i].used) {
            *TokenHandle = g_token_table[i].handle;
            g_token_table[i].access = DesiredAccess;
            return TRUE;
        }
    }
    advapi_set_last_error(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL GetTokenInformation(HANDLE TokenHandle, ULONG TokenInformationClass,
                         LPVOID TokenInformation, DWORD TokenInformationLength,
                         PDWORD ReturnLength) {
    token_entry_t* t;
    DWORD needed;
    BYTE* out;
    DWORD i;
    if (!g_tokens_initialized) tokens_init_internal();
    if (ReturnLength == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    t = token_find(TokenHandle);
    if (t == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    needed = 0;
    switch (TokenInformationClass) {
        case TOKEN_INFORMATION_CLASS_USER:
            needed = sizeof(TOKEN_USER) + 128;
            break;
        case TOKEN_INFORMATION_CLASS_OWNER:
            needed = sizeof(TOKEN_OWNER) + 68;
            break;
        case TOKEN_INFORMATION_CLASS_PRIMARY_GROUP:
            needed = sizeof(TOKEN_PRIMARY_GROUP) + 68;
            break;
        case TOKEN_INFORMATION_CLASS_PRIVILEGES:
            needed = sizeof(TOKEN_PRIVILEGES) + (t->privilege_count - 1) * 12;
            break;
        case TOKEN_INFORMATION_CLASS_STATISTICS:
            needed = sizeof(TOKEN_STATISTICS);
            break;
        case TOKEN_INFORMATION_CLASS_TYPE:
            needed = sizeof(DWORD);
            break;
        case TOKEN_INFORMATION_CLASS_IMPERSONATION_LEVEL:
            needed = sizeof(DWORD);
            break;
        default:
            needed = 256;
            break;
    }
    *ReturnLength = needed;
    if (TokenInformation == NULL || TokenInformationLength < needed) {
        advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    memset(TokenInformation, 0, TokenInformationLength);
    out = (BYTE*)TokenInformation;
    switch (TokenInformationClass) {
        case TOKEN_INFORMATION_CLASS_USER: {
            PTOKEN_USER tu = (PTOKEN_USER)out;
            tu->User.Sid = out + sizeof(TOKEN_USER);
            tu->User.Attributes = 0;
            memcpy(tu->User.Sid, t->user_sid, t->user_sid_len);
            break;
        }
        case TOKEN_INFORMATION_CLASS_OWNER: {
            PTOKEN_OWNER to = (PTOKEN_OWNER)out;
            to->Owner = out + sizeof(TOKEN_OWNER);
            memcpy(to->Owner, t->user_sid, t->user_sid_len);
            break;
        }
        case TOKEN_INFORMATION_CLASS_PRIMARY_GROUP: {
            PTOKEN_PRIMARY_GROUP tpg = (PTOKEN_PRIMARY_GROUP)out;
            tpg->PrimaryGroup = out + sizeof(TOKEN_PRIMARY_GROUP);
            memcpy(tpg->PrimaryGroup, t->user_sid, t->user_sid_len);
            break;
        }
        case TOKEN_INFORMATION_CLASS_PRIVILEGES: {
            PTOKEN_PRIVILEGES tp = (PTOKEN_PRIVILEGES)out;
            tp->PrivilegeCount = t->privilege_count;
            for (i = 0; i < t->privilege_count; i++) {
                tp->Privileges[i].Luid = t->privileges[i].luid;
                tp->Privileges[i].Attributes = t->privileges[i].attrs;
            }
            break;
        }
        case TOKEN_INFORMATION_CLASS_STATISTICS: {
            PTOKEN_STATISTICS ts = (PTOKEN_STATISTICS)out;
            ts->TokenId.LowPart = 1; ts->TokenId.HighPart = 0;
            ts->AuthenticationId.LowPart = 999; ts->AuthenticationId.HighPart = 0;
            ts->TokenType = t->token_type;
            ts->ImpersonationLevel = t->impersonation_level;
            ts->PrivilegeCount = t->privilege_count;
            ts->GroupCount = 1;
            break;
        }
        case TOKEN_INFORMATION_CLASS_TYPE:
            *(DWORD*)out = t->token_type;
            break;
        case TOKEN_INFORMATION_CLASS_IMPERSONATION_LEVEL:
            *(DWORD*)out = t->impersonation_level;
            break;
        default:
            break;
    }
    return TRUE;
}

BOOL SetTokenInformation(HANDLE TokenHandle, ULONG TokenInformationClass,
                         LPVOID TokenInformation, DWORD TokenInformationLength) {
    token_entry_t* t;
    (void)TokenInformationClass;
    (void)TokenInformation;
    (void)TokenInformationLength;
    if (!g_tokens_initialized) tokens_init_internal();
    t = token_find(TokenHandle);
    if (t == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return TRUE;
}

BOOL AdjustTokenPrivileges(HANDLE TokenHandle, BOOL DisableAllPrivileges,
                           void* NewState, DWORD BufferLength,
                           void* PreviousState, PDWORD ReturnLength) {
    token_entry_t* t;
    PTOKEN_PRIVILEGES newp;
    PTOKEN_PRIVILEGES prevp;
    DWORD i, j;
    if (!g_tokens_initialized) tokens_init_internal();
    t = token_find(TokenHandle);
    if (t == NULL) {
        advapi_set_last_error(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (ReturnLength != NULL) {
        *ReturnLength = sizeof(TOKEN_PRIVILEGES) + (t->privilege_count - 1) * 12;
    }
    if (PreviousState != NULL && BufferLength > 0) {
        prevp = (PTOKEN_PRIVILEGES)PreviousState;
        prevp->PrivilegeCount = t->privilege_count;
        for (j = 0; j < t->privilege_count; j++) {
            prevp->Privileges[j].Luid = t->privileges[j].luid;
            prevp->Privileges[j].Attributes = t->privileges[j].attrs;
        }
    }
    if (DisableAllPrivileges) {
        for (j = 0; j < t->privilege_count; j++) {
            t->privileges[j].attrs &= ~SE_PRIVILEGE_ENABLED;
        }
        return TRUE;
    }
    if (NewState != NULL) {
        newp = (PTOKEN_PRIVILEGES)NewState;
        for (i = 0; i < newp->PrivilegeCount; i++) {
            for (j = 0; j < t->privilege_count; j++) {
                if (t->privileges[j].luid.LowPart == newp->Privileges[i].Luid.LowPart &&
                    t->privileges[j].luid.HighPart == newp->Privileges[i].Luid.HighPart) {
                    t->privileges[j].attrs = newp->Privileges[i].Attributes;
                    break;
                }
            }
            if (j >= t->privilege_count && t->privilege_count < 32) {
                t->privileges[t->privilege_count].luid = newp->Privileges[i].Luid;
                t->privileges[t->privilege_count].attrs = newp->Privileges[i].Attributes;
                snprintf(t->privileges[t->privilege_count].name,
                         sizeof(t->privileges[t->privilege_count].name),
                         "SeDynamicPriv%u", t->privilege_count);
                t->privilege_count++;
            }
        }
    }
    return TRUE;
}

typedef struct {
    const char* name;
    DWORD luid_low;
} priv_map_t;

static const priv_map_t g_privilege_map[] = {
    {"SeCreateTokenPrivilege",          2},
    {"SeAssignPrimaryTokenPrivilege",   3},
    {"SeLockMemoryPrivilege",           4},
    {"SeIncreaseQuotaPrivilege",        5},
    {"SeUnsolicitedInputPrivilege",     6},
    {"SeMachineAccountPrivilege",       7},
    {"SeTcbPrivilege",                  8},
    {"SeSecurityPrivilege",             9},
    {"SeTakeOwnershipPrivilege",       10},
    {"SeLoadDriverPrivilege",          11},
    {"SeSystemProfilePrivilege",       12},
    {"SeSystemtimePrivilege",          13},
    {"SeProfileSingleProcessPrivilege",14},
    {"SeIncreaseBasePriorityPrivilege",15},
    {"SeCreatePagefilePrivilege",      16},
    {"SeCreatePermanentPrivilege",     17},
    {"SeBackupPrivilege",              18},
    {"SeRestorePrivilege",             19},
    {"SeShutdownPrivilege",            20},
    {"SeDebugPrivilege",               21},
    {"SeAuditPrivilege",               22},
    {"SeSystemEnvironmentPrivilege",   23},
    {"SeChangeNotifyPrivilege",        24},
    {"SeRemoteShutdownPrivilege",      25},
    {"SeUndockPrivilege",              26},
    {"SeSyncAgentPrivilege",           27},
    {"SeEnableDelegationPrivilege",    28},
    {"SeManageVolumePrivilege",        29},
    {"SeImpersonatePrivilege",         30},
    {"SeCreateGlobalPrivilege",        31},
};
#define PRIV_MAP_COUNT (sizeof(g_privilege_map)/sizeof(g_privilege_map[0]))

BOOL LookupPrivilegeValueA(const char* lpSystemName, const char* lpName,
                           PLUID lpLuid) {
    DWORD i;
    (void)lpSystemName;
    if (lpName == NULL || lpLuid == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < PRIV_MAP_COUNT; i++) {
        if (strcmp(g_privilege_map[i].name, lpName) == 0) {
            lpLuid->LowPart = g_privilege_map[i].luid_low;
            lpLuid->HighPart = 0;
            return TRUE;
        }
    }
    lpLuid->LowPart = 1000u + (DWORD)(strlen(lpName) & 0xFF);
    lpLuid->HighPart = 0;
    return TRUE;
}

BOOL LookupPrivilegeNameA(const char* lpSystemName, PLUID lpLuid,
                          char* lpName, LPDWORD cchName) {
    DWORD i;
    size_t slen;
    (void)lpSystemName;
    if (lpLuid == NULL || cchName == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < PRIV_MAP_COUNT; i++) {
        if (g_privilege_map[i].luid_low == lpLuid->LowPart) {
            slen = strlen(g_privilege_map[i].name) + 1;
            if (*cchName < (DWORD)slen) {
                *cchName = (DWORD)slen;
                advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            strcpy(lpName, g_privilege_map[i].name);
            *cchName = (DWORD)slen;
            return TRUE;
        }
    }
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "SeUnknownPriv%lu", (unsigned long)lpLuid->LowPart);
        slen = strlen(buf) + 1;
        if (*cchName < (DWORD)slen) {
            *cchName = (DWORD)slen;
            advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        strcpy(lpName, buf);
        *cchName = (DWORD)slen;
    }
    return TRUE;
}

typedef struct {
    const char* account;
    const char* domain;
    SID_NAME_USE use;
    BYTE sid[68];
    DWORD sid_len;
} account_map_t;

static account_map_t g_account_map[] = {
    {"Administrator", "BUILTIN", SidTypeUser,
     {1,1,0,0,0,0,0,5,0x20,0,0,0,0x20,0x20,0,0}, 16, SidTypeUser},
    {"Guest", "BUILTIN", SidTypeUser,
     {1,1,0,0,0,0,0,5,0x20,0,0,0,0x22,0x20,0,0}, 16, SidTypeUser},
    {"SYSTEM", "NT AUTHORITY", SidTypeWellKnownGroup,
     {1,1,0,0,0,0,0,5,0x12,0,0,0}, 12, SidTypeWellKnownGroup},
    {"LocalService", "NT AUTHORITY", SidTypeUser,
     {1,1,0,0,0,0,0,5,0x13,0,0,0}, 12, SidTypeUser},
    {"NetworkService", "NT AUTHORITY", SidTypeUser,
     {1,1,0,0,0,0,0,5,0x14,0,0,0}, 12, SidTypeUser},
    {"Everyone", "BUILTIN", SidTypeWellKnownGroup,
     {1,1,0,0,0,0,0,1,0,0,0,0}, 12, SidTypeWellKnownGroup},
    {"Administrators", "BUILTIN", SidTypeAlias,
     {1,2,0,0,0,0,0,5,0x20,0,0,0,0x20,0x20,0,0}, 16, SidTypeAlias},
    {"Users", "BUILTIN", SidTypeAlias,
     {1,2,0,0,0,0,0,5,0x20,0,0,0,0x21,0x20,0,0}, 16, SidTypeAlias},
    {"Guests", "BUILTIN", SidTypeAlias,
     {1,2,0,0,0,0,0,5,0x20,0,0,0,0x22,0x20,0,0}, 16, SidTypeAlias},
    {"Power Users", "BUILTIN", SidTypeAlias,
     {1,2,0,0,0,0,0,5,0x20,0,0,0,0x23,0x20,0,0}, 16, SidTypeAlias},
};
#define ACC_MAP_COUNT (sizeof(g_account_map)/sizeof(g_account_map[0]))

static DWORD sid_len_from_ptr(const BYTE* s) {
    if (s == NULL) return 0;
    return (DWORD)(4u + 4u * s[1]);
}

static int sid_compare(const BYTE* a, const BYTE* b, DWORD l) {
    DWORD i;
    for (i = 0; i < l; i++) if (a[i] != b[i]) return 0;
    return 1;
}

BOOL LookupAccountNameA(const char* lpSystemName, const char* lpAccountName,
                        void* Sid, LPDWORD cbSid,
                        char* ReferencedDomainName,
                        LPDWORD cchReferencedDomainName,
                        PSID_NAME_USE peUse) {
    DWORD i;
    size_t slen;
    (void)lpSystemName;
    if (lpAccountName == NULL || cbSid == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    for (i = 0; i < ACC_MAP_COUNT; i++) {
        if (strcmp(g_account_map[i].account, lpAccountName) == 0) {
            const account_map_t* a = &g_account_map[i];
            if (*cbSid < a->sid_len) {
                *cbSid = a->sid_len;
                advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            slen = strlen(a->domain) + 1;
            if (cchReferencedDomainName != NULL) {
                if (ReferencedDomainName != NULL && *cchReferencedDomainName >= (DWORD)slen) {
                    memcpy(ReferencedDomainName, a->domain, slen);
                }
                *cchReferencedDomainName = (DWORD)slen;
            }
            if (Sid != NULL) memcpy(Sid, a->sid, a->sid_len);
            *cbSid = a->sid_len;
            if (peUse != NULL) *peUse = a->use;
            return TRUE;
        }
    }
    {
        static BYTE gen_sid[68];
        DWORD sz = 16;
        const char* dom = "BUILTIN";
        if (*cbSid < sz) {
            *cbSid = sz;
            advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        memset(gen_sid, 0, sizeof(gen_sid));
        gen_sid[0] = 1;
        gen_sid[1] = 1;
        ((DWORD*)&gen_sid[4])[0] = 0x515u;
        ((DWORD*)&gen_sid[8])[0] = (DWORD)(0x1000u + (strlen(lpAccountName) & 0xFFF));
        slen = strlen(dom) + 1;
        if (cchReferencedDomainName != NULL) {
            if (ReferencedDomainName != NULL && *cchReferencedDomainName >= (DWORD)slen) {
                memcpy(ReferencedDomainName, dom, slen);
            }
            *cchReferencedDomainName = (DWORD)slen;
        }
        if (Sid != NULL) memcpy(Sid, gen_sid, sz);
        *cbSid = sz;
        if (peUse != NULL) *peUse = SidTypeUser;
        return TRUE;
    }
}

BOOL LookupAccountSidA(const char* lpSystemName, void* Sid,
                       char* Name, LPDWORD cchName,
                       char* ReferencedDomainName,
                       LPDWORD cchReferencedDomainName,
                       PSID_NAME_USE peUse) {
    DWORD i;
    BYTE* s = (BYTE*)Sid;
    size_t slen;
    DWORD sl;
    (void)lpSystemName;
    if (Sid == NULL || cchName == NULL) {
        advapi_set_last_error(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    sl = sid_len_from_ptr(s);
    for (i = 0; i < ACC_MAP_COUNT; i++) {
        const account_map_t* a = &g_account_map[i];
        if (a->sid_len == sl && sid_compare(a->sid, s, sl)) {
            slen = strlen(a->account) + 1;
            if (*cchName < (DWORD)slen) {
                *cchName = (DWORD)slen;
                advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }
            if (Name != NULL) memcpy(Name, a->account, slen);
            *cchName = (DWORD)slen;
            if (cchReferencedDomainName != NULL) {
                slen = strlen(a->domain) + 1;
                if (*cchReferencedDomainName >= (DWORD)slen && ReferencedDomainName != NULL) {
                    memcpy(ReferencedDomainName, a->domain, slen);
                }
                *cchReferencedDomainName = (DWORD)slen;
            }
            if (peUse != NULL) *peUse = a->use;
            return TRUE;
        }
    }
    {
        char namebuf[64];
        const char* dom = "BUILTIN";
        snprintf(namebuf, sizeof(namebuf), "Account%08lX", (unsigned long)((DWORD*)s)[2]);
        slen = strlen(namebuf) + 1;
        if (*cchName < (DWORD)slen) {
            *cchName = (DWORD)slen;
            advapi_set_last_error(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        if (Name != NULL) memcpy(Name, namebuf, slen);
        *cchName = (DWORD)slen;
        if (cchReferencedDomainName != NULL) {
            slen = strlen(dom) + 1;
            if (*cchReferencedDomainName >= (DWORD)slen && ReferencedDomainName != NULL) {
                memcpy(ReferencedDomainName, dom, slen);
            }
            *cchReferencedDomainName = (DWORD)slen;
        }
        if (peUse != NULL) *peUse = SidTypeUser;
        return TRUE;
    }
}

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

int advapi32_init(void) {
    eventlog_init_internal();
    ext_scm_init();
    crypto_init_internal();
    cred_init_internal();
    tokens_init_internal();
    {
        extern int scm_init(void);
        scm_init();
    }
    g_crypto_global_seed ^= (uint32_t)read_tsc();
    return 0;
}
