#include <arch/win32.h>
#include <arch/registry.h>
#include <arch/slab.h>
#include <string.h>
extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);

#define SERVICE_STOPPED 1
#define SERVICE_START_PENDING 2
#define SERVICE_STOP_PENDING 3
#define SERVICE_RUNNING 4
#define SERVICE_CONTINUE_PENDING 5
#define SERVICE_PAUSE_PENDING 6
#define SERVICE_PAUSED 7

#define SERVICE_KERNEL_DRIVER 1
#define SERVICE_FILE_SYSTEM_DRIVER 2
#define SERVICE_ADAPTER 4
#define SERVICE_RECOGNIZER_DRIVER 8
#define SERVICE_WIN32_OWN_PROCESS 0x10
#define SERVICE_WIN32_SHARE_PROCESS 0x20
#define SERVICE_INTERACTIVE_PROCESS 0x100
#define SERVICE_DRIVER 0x0000000B
#define SERVICE_WIN32 0x00000030
#define SERVICE_TYPE_ALL 0x000001BF

#define SERVICE_BOOT_START 0
#define SERVICE_SYSTEM_START 1
#define SERVICE_AUTO_START 2
#define SERVICE_DEMAND_START 3
#define SERVICE_DISABLED 4

#define SERVICE_ERROR_IGNORE 0
#define SERVICE_ERROR_NORMAL 1
#define SERVICE_ERROR_SEVERE 2
#define SERVICE_ERROR_CRITICAL 3

#define SERVICE_CONTROL_STOP 1
#define SERVICE_CONTROL_PAUSE 2
#define SERVICE_CONTROL_CONTINUE 3
#define SERVICE_CONTROL_INTERROGATE 4
#define SERVICE_CONTROL_SHUTDOWN 5
#define SERVICE_CONTROL_PARAMCHANGE 6
#define SERVICE_CONTROL_NETBINDADD 7
#define SERVICE_CONTROL_NETBINDREMOVE 8
#define SERVICE_CONTROL_NETBINDENABLE 9
#define SERVICE_CONTROL_NETBINDDISABLE 10
#define SERVICE_CONTROL_DEVICEEVENT 11
#define SERVICE_CONTROL_HARDWAREPROFILECHANGE 12
#define SERVICE_CONTROL_POWER 13
#define SERVICE_CONTROL_SESSIONCHANGE 14
#define SERVICE_CONTROL_PRESHUTDOWN 16
#define SERVICE_CONTROL_TIMECHANGE 32
#define SERVICE_CONTROL_TRIGGEREVENT 60

#define SERVICE_ACCEPT_STOP 1
#define SERVICE_ACCEPT_PAUSE_CONTINUE 2
#define SERVICE_ACCEPT_SHUTDOWN 4
#define SERVICE_ACCEPT_PARAMCHANGE 8
#define SERVICE_ACCEPT_NETBINDCHANGE 16
#define SERVICE_ACCEPT_HARDWAREPROFILECHANGE 32
#define SERVICE_ACCEPT_POWER_EVENT 64
#define SERVICE_ACCEPT_SESSION_CHANGE 128
#define SERVICE_ACCEPT_PRESHUTDOWN 256
#define SERVICE_ACCEPT_TIMECHANGE 512
#define SERVICE_ACCEPT_TRIGGEREVENT 1024

#define SERVICE_NO_CHANGE 0xFFFFFFFFu

#define SERVICE_ACTIVE 1
#define SERVICE_INACTIVE 2
#define SERVICE_STATE_ALL 3

#define ERROR_SERVICE_EXISTS 1073
#define ERROR_SERVICE_DISABLED 1058

typedef uint32_t (*SERVICE_MAIN_FUNCTION)(uint32_t dwNumServicesArgs, char** lpServiceArgVectors);
typedef void (*SERVICE_HANDLER_EX)(uint32_t dwControl, uint32_t dwEventType, void* lpEventData, void* lpContext);

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
    SERVICE_HANDLER_EX handler;
    SERVICE_MAIN_FUNCTION main_func;
    void* user_data;
    uint64_t start_time;
} scm_service_t;

#define SCM_MAX_SERVICES 1024
static scm_service_t g_services[SCM_MAX_SERVICES];
static HANDLE g_scm_database_handle = 0;
static BOOL g_scm_initialized = FALSE;

typedef struct {
    const char* name;
    const char* display_name;
    uint32_t service_type;
    uint32_t start_type;
    const char* binary_path;
} builtin_svc_t;

static const builtin_svc_t g_builtin_services[] = {
    {"EventLog",           "Event Log",                            SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,   "C:\\Windows\\system32\\services.exe"},
    {"Dhcp",               "DHCP Client",                           SERVICE_KERNEL_DRIVER,     SERVICE_AUTO_START,   "%SystemRoot%\\system32\\drivers\\dhcp.sys"},
    {"Winmgmt",            "WMI",                                   SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"Themes",             "Themes",                                SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"Schedule",           "Task Scheduler",                        SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"W32Time",            "Windows Time",                          SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"Audiosrv",           "Windows Audio",                         SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"ShellHWDetection",   "Shell Hardware Detection",              SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"DcomLaunch",         "DCOM Server Process Launcher",          SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k DcomLaunch"},
    {"PlugPlay",           "Plug and Play",                         SERVICE_KERNEL_DRIVER,     SERVICE_AUTO_START,   "%SystemRoot%\\system32\\drivers\\plugplay.sys"},
    {"Power",              "Power",                                 SERVICE_KERNEL_DRIVER,     SERVICE_AUTO_START,   "%SystemRoot%\\system32\\drivers\\power.sys"},
    {"RpcSs",              "Remote Procedure Call (RPC)",           SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k rpcss"},
    {"LanmanWorkstation",  "Workstation",                           SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k NetworkService"},
    {"Netman",             "Network Connections",                   SERVICE_WIN32_SHARE_PROCESS,SERVICE_DEMAND_START,"C:\\Windows\\system32\\svchost.exe -k LocalSystemNetworkRestricted"},
    {"wscsvc",             "Security Center",                       SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k LocalServiceNetworkRestricted"},
    {"wuauserv",           "Windows Update",                        SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k wuauserv"},
    {"MpsSvc",             "Windows Firewall",                      SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k LocalServiceNoNetwork"},
    {"SystemEventsBroker", "System Events Broker",                  SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k DcomLaunch"},
    {"ProfSvc",            "User Profile Service",                  SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"PolicyAgent",        "IPsec Policy Agent",                    SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k NetworkService"},
    {"EventSystem",        "COM+ Event System",                     SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k EventLog"},
    {"COMSysApp",          "COM+ System Application",               SERVICE_WIN32_SHARE_PROCESS,SERVICE_DEMAND_START,"C:\\Windows\\system32\\dllhost.exe"},
    {"SENS",               "System Event Notification Service",     SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"WinHTTP",            "WinHTTP Web Proxy Auto-Discovery Svc",  SERVICE_WIN32_SHARE_PROCESS,SERVICE_DEMAND_START,"C:\\Windows\\system32\\svchost.exe -k LocalService"},
    {"TermService",        "Remote Desktop Services",               SERVICE_WIN32_SHARE_PROCESS,SERVICE_DEMAND_START,"C:\\Windows\\System32\\svchost.exe -k NetworkService"},
    {"lanmanserver",       "Server",                                SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
    {"TapiSrv",            "Telephony",                             SERVICE_WIN32_SHARE_PROCESS,SERVICE_DEMAND_START,"C:\\Windows\\System32\\svchost.exe -k NetworkService"},
    {"SysMain",            "Superfetch",                            SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k LocalSystemNetworkRestricted"},
    {"FontCache",          "Windows Font Cache Service",            SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\system32\\svchost.exe -k LocalService"},
    {"BITS",               "Background Intelligent Transfer Svc",  SERVICE_WIN32_SHARE_PROCESS,SERVICE_AUTO_START,  "C:\\Windows\\System32\\svchost.exe -k netsvcs"},
};

static void scm_set_last_error(uint32_t err) {
    extern void SetLastError(uint32_t dwErrCode);
    SetLastError(err);
}

int scm_init(void) {
    uint32_t i;
    uint32_t builtin_count;
    if (g_scm_initialized) return 0;

    memset(g_services, 0, sizeof(g_services));
    g_scm_database_handle = (HANDLE)1u;
    builtin_count = sizeof(g_builtin_services) / sizeof(g_builtin_services[0]);

    for (i = 0; i < builtin_count && i < SCM_MAX_SERVICES; i++) {
        scm_service_t* svc = &g_services[i];
        const builtin_svc_t* b = &g_builtin_services[i];
        size_t name_len;
        size_t disp_len;
        size_t bin_len;

        svc->service_handle = (uint32_t)(i + 1000u);
        name_len = strlen(b->name);
        if (name_len >= sizeof(svc->name)) name_len = sizeof(svc->name) - 1u;
        memcpy(svc->name, b->name, name_len);
        svc->name[name_len] = 0;

        disp_len = strlen(b->display_name);
        if (disp_len >= sizeof(svc->display_name)) disp_len = sizeof(svc->display_name) - 1u;
        memcpy(svc->display_name, b->display_name, disp_len);
        svc->display_name[disp_len] = 0;

        svc->service_type = b->service_type;
        svc->start_type = b->start_type;
        svc->error_control = SERVICE_ERROR_NORMAL;

        bin_len = strlen(b->binary_path);
        if (bin_len >= sizeof(svc->binary_path)) bin_len = sizeof(svc->binary_path) - 1u;
        memcpy(svc->binary_path, b->binary_path, bin_len);
        svc->binary_path[bin_len] = 0;

        svc->state = SERVICE_STOPPED;
        if (svc->service_type & SERVICE_WIN32) {
            svc->controls_accepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        } else {
            svc->controls_accepted = 0;
        }
        svc->win32_exit_code = 0;
        svc->service_specific_exit_code = 0;
        svc->check_point = 0;
        svc->wait_hint = 0;
        svc->process_id = (uint32_t)(4u + i * 4u);
        svc->handler = NULL;
        svc->main_func = NULL;
        svc->user_data = NULL;
        svc->start_time = 0;
    }

    g_scm_initialized = TRUE;
    return 0;
}

HANDLE OpenSCManagerA(const char* lpMachineName, const char* lpDatabaseName, uint32_t dwDesiredAccess) {
    (void)lpMachineName;
    (void)lpDatabaseName;
    (void)dwDesiredAccess;
    if (!g_scm_initialized) scm_init();
    if (g_scm_database_handle == NULL) {
        g_scm_database_handle = (HANDLE)1u;
    }
    return g_scm_database_handle;
}

BOOL CloseServiceHandle(HANDLE hSCObject) {
    (void)hSCObject;
    return TRUE;
}

static scm_service_t* find_service_by_name(const char* name) {
    uint32_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < SCM_MAX_SERVICES; i++) {
        if (g_services[i].service_handle != 0 && strcmp(g_services[i].name, name) == 0) {
            return &g_services[i];
        }
    }
    return NULL;
}

static scm_service_t* find_service_by_handle(HANDLE h) {
    uint32_t i;
    uint32_t handle_val;
    if (h == NULL) return NULL;
    handle_val = (uint32_t)((uint64_t)h);
    for (i = 0; i < SCM_MAX_SERVICES; i++) {
        if (g_services[i].service_handle == handle_val) {
            return &g_services[i];
        }
    }
    return NULL;
}

HANDLE CreateServiceA(HANDLE hSCManager, const char* lpServiceName, const char* lpDisplayName,
                       uint32_t dwDesiredAccess, uint32_t dwServiceType, uint32_t dwStartType,
                       uint32_t dwErrorControl, const char* lpBinaryPathName,
                       const char* lpLoadOrderGroup, uint32_t* lpdwTagId, const char* lpDependencies,
                       const char* lpServiceStartName, const char* lpPassword) {
    uint32_t i;
    scm_service_t* svc;
    size_t len;
    (void)dwDesiredAccess;
    (void)lpdwTagId;

    if (hSCManager != g_scm_database_handle || lpServiceName == NULL || lpBinaryPathName == NULL) {
        return NULL;
    }
    if (!g_scm_initialized) scm_init();

    if (find_service_by_name(lpServiceName) != NULL) {
        scm_set_last_error(ERROR_SERVICE_EXISTS);
        return NULL;
    }

    svc = NULL;
    for (i = 0; i < SCM_MAX_SERVICES; i++) {
        if (g_services[i].service_handle == 0) {
            svc = &g_services[i];
            break;
        }
    }
    if (svc == NULL) return NULL;

    memset(svc, 0, sizeof(scm_service_t));
    svc->service_handle = (uint32_t)(i + 1000u);

    len = strlen(lpServiceName);
    if (len >= sizeof(svc->name)) len = sizeof(svc->name) - 1u;
    memcpy(svc->name, lpServiceName, len);
    svc->name[len] = 0;

    if (lpDisplayName != NULL) {
        len = strlen(lpDisplayName);
        if (len >= sizeof(svc->display_name)) len = sizeof(svc->display_name) - 1u;
        memcpy(svc->display_name, lpDisplayName, len);
        svc->display_name[len] = 0;
    }

    svc->service_type = dwServiceType;
    svc->start_type = dwStartType;
    svc->error_control = dwErrorControl;

    len = strlen(lpBinaryPathName);
    if (len >= sizeof(svc->binary_path)) len = sizeof(svc->binary_path) - 1u;
    memcpy(svc->binary_path, lpBinaryPathName, len);
    svc->binary_path[len] = 0;

    if (lpLoadOrderGroup != NULL) {
        len = strlen(lpLoadOrderGroup);
        if (len >= sizeof(svc->load_order_group)) len = sizeof(svc->load_order_group) - 1u;
        memcpy(svc->load_order_group, lpLoadOrderGroup, len);
        svc->load_order_group[len] = 0;
    }

    if (lpDependencies != NULL) {
        len = strlen(lpDependencies);
        if (len + 2u >= sizeof(svc->dependencies)) len = sizeof(svc->dependencies) - 2u;
        memcpy(svc->dependencies, lpDependencies, len);
        svc->dependencies[len] = 0;
        svc->dependencies[len + 1u] = 0;
    }

    if (lpServiceStartName != NULL) {
        len = strlen(lpServiceStartName);
        if (len >= sizeof(svc->service_start_name)) len = sizeof(svc->service_start_name) - 1u;
        memcpy(svc->service_start_name, lpServiceStartName, len);
        svc->service_start_name[len] = 0;
    }

    if (lpPassword != NULL) {
        len = strlen(lpPassword);
        if (len >= sizeof(svc->password)) len = sizeof(svc->password) - 1u;
        memcpy(svc->password, lpPassword, len);
        svc->password[len] = 0;
    }

    svc->state = SERVICE_STOPPED;
    if (svc->service_type & SERVICE_WIN32) {
        svc->controls_accepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }
    return (HANDLE)((uint64_t)svc->service_handle);
}

HANDLE OpenServiceA(HANDLE hSCManager, const char* lpServiceName, uint32_t dwDesiredAccess) {
    scm_service_t* svc;
    (void)dwDesiredAccess;
    if (hSCManager != g_scm_database_handle) return NULL;
    if (!g_scm_initialized) scm_init();
    svc = find_service_by_name(lpServiceName);
    if (svc == NULL) return NULL;
    return (HANDLE)((uint64_t)svc->service_handle);
}

BOOL StartServiceA(HANDLE hService, uint32_t dwNumServiceArgs, const char** lpServiceArgVectors) {
    scm_service_t* svc;
    (void)dwNumServiceArgs;
    (void)lpServiceArgVectors;
    if (!g_scm_initialized) scm_init();
    svc = find_service_by_handle(hService);
    if (svc == NULL) return FALSE;
    if (svc->start_type == SERVICE_DISABLED) {
        scm_set_last_error(ERROR_SERVICE_DISABLED);
        return FALSE;
    }
    if (svc->state == SERVICE_RUNNING) return TRUE;

    if (svc->dependencies[0] != 0) {
        const char* dep = svc->dependencies;
        while (*dep != 0) {
            scm_service_t* dep_svc = find_service_by_name(dep);
            if (dep_svc != NULL && dep_svc->state != SERVICE_RUNNING) {
                StartServiceA((HANDLE)((uint64_t)dep_svc->service_handle), 0, NULL);
            }
            dep += strlen(dep) + 1u;
        }
    }

    svc->state = SERVICE_START_PENDING;
    svc->check_point = 1;
    svc->wait_hint = 3000;

    if (svc->main_func != NULL) {
        svc->main_func(0, NULL);
    }

    svc->state = SERVICE_RUNNING;
    svc->check_point = 0;
    svc->wait_hint = 0;
    svc->start_time = 0;
    return TRUE;
}

typedef struct _SERVICE_STATUS {
    uint32_t dwServiceType;
    uint32_t dwCurrentState;
    uint32_t dwControlsAccepted;
    uint32_t dwWin32ExitCode;
    uint32_t dwServiceSpecificExitCode;
    uint32_t dwCheckPoint;
    uint32_t dwWaitHint;
} SERVICE_STATUS, *LPSERVICE_STATUS;

BOOL ControlService(HANDLE hService, uint32_t dwControl, void* lpServiceStatus) {
    scm_service_t* svc;
    SERVICE_STATUS* st = (SERVICE_STATUS*)lpServiceStatus;
    if (!g_scm_initialized) scm_init();
    svc = find_service_by_handle(hService);
    if (svc == NULL) return FALSE;

    switch (dwControl) {
        case SERVICE_CONTROL_STOP:
            if ((svc->controls_accepted & SERVICE_ACCEPT_STOP) != 0) {
                svc->state = SERVICE_STOP_PENDING;
                svc->check_point = 1;
                svc->wait_hint = 3000;
                if (svc->handler != NULL) {
                    svc->handler(dwControl, 0, NULL, svc->user_data);
                }
                svc->state = SERVICE_STOPPED;
                svc->check_point = 0;
                svc->wait_hint = 0;
            }
            break;
        case SERVICE_CONTROL_PAUSE:
            if ((svc->controls_accepted & SERVICE_ACCEPT_PAUSE_CONTINUE) != 0) {
                svc->state = SERVICE_PAUSE_PENDING;
                if (svc->handler != NULL) {
                    svc->handler(dwControl, 0, NULL, svc->user_data);
                }
                svc->state = SERVICE_PAUSED;
            }
            break;
        case SERVICE_CONTROL_CONTINUE:
            if ((svc->controls_accepted & SERVICE_ACCEPT_PAUSE_CONTINUE) != 0 && svc->state == SERVICE_PAUSED) {
                svc->state = SERVICE_CONTINUE_PENDING;
                if (svc->handler != NULL) {
                    svc->handler(dwControl, 0, NULL, svc->user_data);
                }
                svc->state = SERVICE_RUNNING;
            }
            break;
        case SERVICE_CONTROL_INTERROGATE:
            break;
        case SERVICE_CONTROL_SHUTDOWN:
            if ((svc->controls_accepted & SERVICE_ACCEPT_SHUTDOWN) != 0) {
                if (svc->handler != NULL) {
                    svc->handler(dwControl, 0, NULL, svc->user_data);
                }
                svc->state = SERVICE_STOPPED;
            }
            break;
        default:
            if (svc->handler != NULL) {
                svc->handler(dwControl, 0, NULL, svc->user_data);
            }
            break;
    }

    if (st != NULL) {
        st->dwServiceType = svc->service_type;
        st->dwCurrentState = svc->state;
        st->dwControlsAccepted = svc->controls_accepted;
        st->dwWin32ExitCode = svc->win32_exit_code;
        st->dwServiceSpecificExitCode = svc->service_specific_exit_code;
        st->dwCheckPoint = svc->check_point;
        st->dwWaitHint = svc->wait_hint;
    }
    return TRUE;
}

BOOL DeleteService(HANDLE hService) {
    scm_service_t* svc;
    if (!g_scm_initialized) scm_init();
    svc = find_service_by_handle(hService);
    if (svc == NULL) return FALSE;
    memset(svc, 0, sizeof(scm_service_t));
    return TRUE;
}

BOOL QueryServiceStatus(HANDLE hService, void* lpServiceStatus) {
    scm_service_t* svc;
    SERVICE_STATUS* st = (SERVICE_STATUS*)lpServiceStatus;
    if (!g_scm_initialized) scm_init();
    svc = find_service_by_handle(hService);
    if (svc == NULL || st == NULL) return FALSE;
    st->dwServiceType = svc->service_type;
    st->dwCurrentState = svc->state;
    st->dwControlsAccepted = svc->controls_accepted;
    st->dwWin32ExitCode = svc->win32_exit_code;
    st->dwServiceSpecificExitCode = svc->service_specific_exit_code;
    st->dwCheckPoint = svc->check_point;
    st->dwWaitHint = svc->wait_hint;
    return TRUE;
}

typedef struct _ENUM_SERVICE_STATUS_PROCESSA {
    char* lpServiceName;
    char* lpDisplayName;
    SERVICE_STATUS ServiceStatus;
    uint32_t dwProcessId;
    uint32_t dwServiceFlags;
} ENUM_SERVICE_STATUS_PROCESSA, *LPENUM_SERVICE_STATUS_PROCESSA;

BOOL EnumServicesStatusExA(HANDLE hSCManager, uint32_t InfoLevel, uint32_t dwServiceType,
                            uint32_t dwServiceState, uint8_t* lpServices, uint32_t cbBufSize,
                            uint32_t* pcbBytesNeeded, uint32_t* lpServicesReturned,
                            uint32_t* lpResumeHandle, const char* pszGroupName) {
    uint32_t i;
    uint32_t count = 0;
    uint32_t needed = 0;
    uint8_t* buf_ptr;
    uint8_t* str_ptr;
    (void)InfoLevel;
    (void)pszGroupName;
    (void)lpResumeHandle;

    if (!g_scm_initialized) scm_init();
    if (hSCManager != g_scm_database_handle) return FALSE;
    if (pcbBytesNeeded == NULL || lpServicesReturned == NULL) return FALSE;

    for (i = 0; i < SCM_MAX_SERVICES; i++) {
        scm_service_t* svc = &g_services[i];
        BOOL match_type;
        BOOL match_state;
        if (svc->service_handle == 0) continue;
        match_type = (dwServiceType == SERVICE_TYPE_ALL) || ((svc->service_type & dwServiceType) != 0);
        if (!match_type) continue;
        if (dwServiceState == SERVICE_ACTIVE) {
            match_state = (svc->state != SERVICE_STOPPED);
        } else if (dwServiceState == SERVICE_INACTIVE) {
            match_state = (svc->state == SERVICE_STOPPED);
        } else {
            match_state = TRUE;
        }
        if (!match_state) continue;
        count++;
        needed += (uint32_t)(sizeof(ENUM_SERVICE_STATUS_PROCESSA) + strlen(svc->name) + 1u + strlen(svc->display_name) + 1u);
    }

    *pcbBytesNeeded = needed;
    if (lpServices == NULL || cbBufSize < needed) {
        *lpServicesReturned = 0;
        return FALSE;
    }

    buf_ptr = lpServices;
    str_ptr = lpServices + sizeof(ENUM_SERVICE_STATUS_PROCESSA) * count;
    count = 0;

    for (i = 0; i < SCM_MAX_SERVICES; i++) {
        scm_service_t* svc = &g_services[i];
        BOOL match_type;
        BOOL match_state;
        size_t len;
        ENUM_SERVICE_STATUS_PROCESSA* entry;
        if (svc->service_handle == 0) continue;
        match_type = (dwServiceType == SERVICE_TYPE_ALL) || ((svc->service_type & dwServiceType) != 0);
        if (!match_type) continue;
        if (dwServiceState == SERVICE_ACTIVE) {
            match_state = (svc->state != SERVICE_STOPPED);
        } else if (dwServiceState == SERVICE_INACTIVE) {
            match_state = (svc->state == SERVICE_STOPPED);
        } else {
            match_state = TRUE;
        }
        if (!match_state) continue;

        entry = (ENUM_SERVICE_STATUS_PROCESSA*)buf_ptr;
        buf_ptr += sizeof(ENUM_SERVICE_STATUS_PROCESSA);

        len = strlen(svc->name) + 1u;
        memcpy(str_ptr, svc->name, len);
        entry->lpServiceName = (char*)str_ptr;
        str_ptr += len;

        len = strlen(svc->display_name) + 1u;
        memcpy(str_ptr, svc->display_name, len);
        entry->lpDisplayName = (char*)str_ptr;
        str_ptr += len;

        entry->ServiceStatus.dwServiceType = svc->service_type;
        entry->ServiceStatus.dwCurrentState = svc->state;
        entry->ServiceStatus.dwControlsAccepted = svc->controls_accepted;
        entry->ServiceStatus.dwWin32ExitCode = svc->win32_exit_code;
        entry->ServiceStatus.dwServiceSpecificExitCode = svc->service_specific_exit_code;
        entry->ServiceStatus.dwCheckPoint = svc->check_point;
        entry->ServiceStatus.dwWaitHint = svc->wait_hint;
        entry->dwProcessId = svc->process_id;
        entry->dwServiceFlags = 0;
        count++;
    }

    *lpServicesReturned = count;
    return TRUE;
}

BOOL ChangeServiceConfigA(HANDLE hService, uint32_t dwServiceType, uint32_t dwStartType,
                           uint32_t dwErrorControl, const char* lpBinaryPathName,
                           const char* lpLoadOrderGroup, uint32_t* lpdwTagId,
                           const char* lpDependencies, const char* lpServiceStartName,
                           const char* lpPassword, const char* lpDisplayName) {
    scm_service_t* svc;
    size_t len;
    (void)lpdwTagId;
    if (!g_scm_initialized) scm_init();
    svc = find_service_by_handle(hService);
    if (svc == NULL) return FALSE;

    if (dwServiceType != SERVICE_NO_CHANGE) svc->service_type = dwServiceType;
    if (dwStartType != SERVICE_NO_CHANGE) svc->start_type = dwStartType;
    if (dwErrorControl != SERVICE_NO_CHANGE) svc->error_control = dwErrorControl;

    if (lpBinaryPathName != NULL) {
        len = strlen(lpBinaryPathName);
        if (len >= sizeof(svc->binary_path)) len = sizeof(svc->binary_path) - 1u;
        memcpy(svc->binary_path, lpBinaryPathName, len);
        svc->binary_path[len] = 0;
    }
    if (lpLoadOrderGroup != NULL) {
        len = strlen(lpLoadOrderGroup);
        if (len >= sizeof(svc->load_order_group)) len = sizeof(svc->load_order_group) - 1u;
        memcpy(svc->load_order_group, lpLoadOrderGroup, len);
        svc->load_order_group[len] = 0;
    }
    if (lpDependencies != NULL) {
        len = strlen(lpDependencies);
        if (len + 2u >= sizeof(svc->dependencies)) len = sizeof(svc->dependencies) - 2u;
        memcpy(svc->dependencies, lpDependencies, len);
        svc->dependencies[len] = 0;
        svc->dependencies[len + 1u] = 0;
    }
    if (lpServiceStartName != NULL) {
        len = strlen(lpServiceStartName);
        if (len >= sizeof(svc->service_start_name)) len = sizeof(svc->service_start_name) - 1u;
        memcpy(svc->service_start_name, lpServiceStartName, len);
        svc->service_start_name[len] = 0;
    }
    if (lpPassword != NULL) {
        len = strlen(lpPassword);
        if (len >= sizeof(svc->password)) len = sizeof(svc->password) - 1u;
        memcpy(svc->password, lpPassword, len);
        svc->password[len] = 0;
    }
    if (lpDisplayName != NULL) {
        len = strlen(lpDisplayName);
        if (len >= sizeof(svc->display_name)) len = sizeof(svc->display_name) - 1u;
        memcpy(svc->display_name, lpDisplayName, len);
        svc->display_name[len] = 0;
    }
    return TRUE;
}

BOOL SetServiceStatus(HANDLE hServiceStatus, const void* lpServiceStatus) {
    scm_service_t* svc;
    SERVICE_STATUS* st = (SERVICE_STATUS*)(uintptr_t)lpServiceStatus;
    if (!g_scm_initialized) scm_init();
    if (st == NULL) return FALSE;
    svc = find_service_by_handle(hServiceStatus);
    if (svc == NULL) return FALSE;
    svc->state = st->dwCurrentState;
    svc->controls_accepted = st->dwControlsAccepted;
    svc->win32_exit_code = st->dwWin32ExitCode;
    svc->service_specific_exit_code = st->dwServiceSpecificExitCode;
    svc->check_point = st->dwCheckPoint;
    svc->wait_hint = st->dwWaitHint;
    return TRUE;
}

HANDLE RegisterServiceCtrlHandlerExA(const char* lpServiceName, SERVICE_HANDLER_EX lpHandlerProc, void* lpContext) {
    scm_service_t* svc;
    if (!g_scm_initialized) scm_init();
    if (lpServiceName == NULL || lpHandlerProc == NULL) return NULL;
    svc = find_service_by_name(lpServiceName);
    if (svc == NULL) return NULL;
    svc->handler = lpHandlerProc;
    svc->user_data = lpContext;
    return (HANDLE)((uint64_t)svc->service_handle);
}

int scm_auto_start(void) {
    uint32_t i;
    int count = 0;
    if (!g_scm_initialized) scm_init();
    for (i = 0; i < SCM_MAX_SERVICES; i++) {
        scm_service_t* svc = &g_services[i];
        if (svc->service_handle == 0) continue;
        if (svc->start_type == SERVICE_BOOT_START ||
            svc->start_type == SERVICE_SYSTEM_START ||
            svc->start_type == SERVICE_AUTO_START) {
            if (svc->state != SERVICE_RUNNING) {
                if (StartServiceA((HANDLE)((uint64_t)svc->service_handle), 0, NULL)) {
                    count++;
                }
            }
        }
    }
    return count;
}
