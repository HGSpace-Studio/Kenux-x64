#include <arch/win32.h>
#include <arch/registry.h>
#include <arch/pe.h>
#include <arch/slab.h>
#include <arch/fs.h>
#include <arch/ntfs.h>
#include <string.h>

extern uint64_t timer_get_jiffies(void);
extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);
extern vfs_node_t* vfs_find_path(const char* path);
extern int vfs_unlink(const char* path);
extern int vfs_mkdir(const char* path, int mode);
extern int vfs_rmdir(const char* path);
extern int vfs_rename(const char* oldpath, const char* newpath);

#ifndef HZ
#define HZ 100
#endif

#ifndef ERROR_READ_FAULT
#define ERROR_READ_FAULT          30L
#endif

#ifndef ERROR_BAD_FORMAT
#define ERROR_BAD_FORMAT          11L
#endif

#define WIN32_MAX_HANDLES 256
#define WIN32_INVALID_HANDLE_IDX 0xFFFFFFFFU

typedef struct {
    BOOL     used;
    uint32_t type;
    void*    data;
    uint64_t size;
} win32_handle_entry_t;

#define HANDLE_TYPE_FILE       1
#define HANDLE_TYPE_FIND       2
#define HANDLE_TYPE_THREAD     3
#define HANDLE_TYPE_HEAP       4
#define HANDLE_TYPE_MEM        5
#define HANDLE_TYPE_PROCESS    6

static win32_handle_entry_t g_handle_table[WIN32_MAX_HANDLES];
static DWORD g_last_error = 0;

typedef struct {
    vfs_node_t* node;
    uint64_t    offset;
    uint32_t    access;
    uint32_t    share_mode;
    BOOL        writeable;
} win32_file_t;

typedef struct {
    char        pattern[FS_MAX_NAME];
    vfs_node_t* dir_node;
    int         index;
    char        base_path[FS_MAX_NAME];
} win32_find_t;

typedef struct {
    char name[256];
    char value[512];
} env_entry_t;

#define ENV_MAX_ENTRIES 64
static env_entry_t g_env_table[ENV_MAX_ENTRIES];
static uint32_t g_env_count = 0;

#define GLOBAL_ALLOC_TABLE_SIZE 256
typedef struct {
    void*    ptr;
    uint64_t size;
    BOOL     used;
} global_alloc_entry_t;

static global_alloc_entry_t g_global_alloc_table[GLOBAL_ALLOC_TABLE_SIZE];

static uint32_t handle_table_alloc(void) {
    uint32_t i;
    for (i = 0; i < WIN32_MAX_HANDLES; i++) {
        if (!g_handle_table[i].used) {
            g_handle_table[i].used = TRUE;
            return i;
        }
    }
    return WIN32_INVALID_HANDLE_IDX;
}

static void handle_table_free(uint32_t idx) {
    if (idx < WIN32_MAX_HANDLES) {
        g_handle_table[idx].used = FALSE;
        g_handle_table[idx].type = 0;
        g_handle_table[idx].data = NULL;
        g_handle_table[idx].size = 0;
    }
}

static uint32_t handle_to_idx(HANDLE h) {
    uintptr_t v = (uintptr_t)h;
    if (v == 0 || v >= WIN32_MAX_HANDLES) {
        return WIN32_INVALID_HANDLE_IDX;
    }
    if (!g_handle_table[v].used) {
        return WIN32_INVALID_HANDLE_IDX;
    }
    return (uint32_t)v;
}

static HANDLE idx_to_handle(uint32_t idx) {
    return (HANDLE)(uintptr_t)idx;
}

static int global_alloc_find_slot(void) {
    int i;
    for (i = 0; i < GLOBAL_ALLOC_TABLE_SIZE; i++) {
        if (!g_global_alloc_table[i].used) {
            return i;
        }
    }
    return -1;
}

static int global_alloc_find_by_ptr(void* p) {
    int i;
    for (i = 0; i < GLOBAL_ALLOC_TABLE_SIZE; i++) {
        if (g_global_alloc_table[i].used && g_global_alloc_table[i].ptr == p) {
            return i;
        }
    }
    return -1;
}

static void env_init_defaults(void) {
    if (g_env_count != 0) {
        return;
    }
    strncpy(g_env_table[0].name, "PATH", sizeof(g_env_table[0].name) - 1);
    g_env_table[0].name[sizeof(g_env_table[0].name) - 1] = '\0';
    strncpy(g_env_table[0].value, "C:\\Windows\\system32;C:\\Windows", sizeof(g_env_table[0].value) - 1);
    g_env_table[0].value[sizeof(g_env_table[0].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[1].name, "TEMP", sizeof(g_env_table[1].name) - 1);
    g_env_table[1].name[sizeof(g_env_table[1].name) - 1] = '\0';
    strncpy(g_env_table[1].value, "C:\\Users\\User\\AppData\\Local\\Temp", sizeof(g_env_table[1].value) - 1);
    g_env_table[1].value[sizeof(g_env_table[1].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[2].name, "TMP", sizeof(g_env_table[2].name) - 1);
    g_env_table[2].name[sizeof(g_env_table[2].name) - 1] = '\0';
    strncpy(g_env_table[2].value, "C:\\Users\\User\\AppData\\Local\\Temp", sizeof(g_env_table[2].value) - 1);
    g_env_table[2].value[sizeof(g_env_table[2].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[3].name, "SYSTEMROOT", sizeof(g_env_table[3].name) - 1);
    g_env_table[3].name[sizeof(g_env_table[3].name) - 1] = '\0';
    strncpy(g_env_table[3].value, "C:\\Windows", sizeof(g_env_table[3].value) - 1);
    g_env_table[3].value[sizeof(g_env_table[3].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[4].name, "SYSTEMDRIVE", sizeof(g_env_table[4].name) - 1);
    g_env_table[4].name[sizeof(g_env_table[4].name) - 1] = '\0';
    strncpy(g_env_table[4].value, "C:", sizeof(g_env_table[4].value) - 1);
    g_env_table[4].value[sizeof(g_env_table[4].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[5].name, "USERNAME", sizeof(g_env_table[5].name) - 1);
    g_env_table[5].name[sizeof(g_env_table[5].name) - 1] = '\0';
    strncpy(g_env_table[5].value, "User", sizeof(g_env_table[5].value) - 1);
    g_env_table[5].value[sizeof(g_env_table[5].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[6].name, "USERPROFILE", sizeof(g_env_table[6].name) - 1);
    g_env_table[6].name[sizeof(g_env_table[6].name) - 1] = '\0';
    strncpy(g_env_table[6].value, "C:\\Users\\User", sizeof(g_env_table[6].value) - 1);
    g_env_table[6].value[sizeof(g_env_table[6].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[7].name, "APPDATA", sizeof(g_env_table[7].name) - 1);
    g_env_table[7].name[sizeof(g_env_table[7].name) - 1] = '\0';
    strncpy(g_env_table[7].value, "C:\\Users\\User\\AppData\\Roaming", sizeof(g_env_table[7].value) - 1);
    g_env_table[7].value[sizeof(g_env_table[7].value) - 1] = '\0';
    g_env_count++;

    strncpy(g_env_table[8].name, "LOCALAPPDATA", sizeof(g_env_table[8].name) - 1);
    g_env_table[8].name[sizeof(g_env_table[8].name) - 1] = '\0';
    strncpy(g_env_table[8].value, "C:\\Users\\User\\AppData\\Local", sizeof(g_env_table[8].value) - 1);
    g_env_table[8].value[sizeof(g_env_table[8].value) - 1] = '\0';
    g_env_count++;
}

static int env_find(const char* name) {
    uint32_t i;
    env_init_defaults();
    for (i = 0; i < g_env_count; i++) {
        if (strcmp(g_env_table[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}





HANDLE CreateThread(void* lpThreadAttributes, uint64_t dwStackSize,
                    uint32_t (*lpStartAddress)(void*), void* lpParameter,
                    uint32_t dwCreationFlags, uint32_t* lpThreadId) {
    HANDLE h;
    uint32_t idx;
    void* mem;

    (void)lpThreadAttributes;
    (void)dwStackSize;
    (void)lpStartAddress;
    (void)lpParameter;
    (void)dwCreationFlags;

    mem = memory_alloc(64);
    if (mem == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    memset(mem, 0, 64);

    idx = handle_table_alloc();
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        memory_free(mem);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    g_handle_table[idx].type = HANDLE_TYPE_THREAD;
    g_handle_table[idx].data = mem;
    g_handle_table[idx].size = 64;
    h = idx_to_handle(idx);

    if (lpThreadId != NULL) {
        *lpThreadId = 0;
    }
    return h;
}

BOOL TerminateThread(HANDLE hThread, uint32_t dwExitCode) {
    (void)dwExitCode;
    if (hThread == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL CloseHandle(HANDLE hObject) {
    uint32_t idx;
    win32_file_t* f;
    win32_find_t* fctx;

    if (hObject == NULL || hObject == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if ((uintptr_t)hObject == 0xFFFFFFFFFFFFFFFFULL ||
        (uintptr_t)hObject == 0xFFFFFFFFFFFFFFFEULL) {
        return TRUE;
    }

    idx = handle_to_idx(hObject);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (g_handle_table[idx].type == HANDLE_TYPE_FILE) {
        f = (win32_file_t*)g_handle_table[idx].data;
        if (f != NULL) {
            memory_free(f);
        }
    } else if (g_handle_table[idx].type == HANDLE_TYPE_FIND) {
        fctx = (win32_find_t*)g_handle_table[idx].data;
        if (fctx != NULL) {
            memory_free(fctx);
        }
    } else if (g_handle_table[idx].data != NULL) {
        memory_free(g_handle_table[idx].data);
    }

    handle_table_free(idx);
    return TRUE;
}


uint32_t GetCurrentProcessId(void) {
    return 1;
}

uint32_t GetCurrentThreadId(void) {
    return 1;
}

HANDLE GetCurrentProcess(void) {
    return (HANDLE)(uintptr_t)0xFFFFFFFFFFFFFFFFULL;
}

HANDLE GetCurrentThread(void) {
    return (HANDLE)(uintptr_t)0xFFFFFFFFFFFFFFFEULL;
}

BOOL SetProcessPriorityBoost(HANDLE hProcess, BOOL bDisablePriorityBoost) {
    (void)bDisablePriorityBoost;
    if (hProcess == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return TRUE;
}

BOOL GetExitCodeProcess(HANDLE hProcess, uint32_t* lpExitCode) {
    if (hProcess == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (lpExitCode != NULL) {
        *lpExitCode = 0;
    }
    return TRUE;
}

typedef struct _WIN32_FIND_DATAA {
    DWORD    dwFileAttributes;
    uint64_t ftCreationTime;
    uint64_t ftLastAccessTime;
    uint64_t ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    CHAR     cFileName[MAX_PATH];
    CHAR     cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

static void win32_path_to_unix(const char* win_path, char* unix_path, uint32_t size) {
    uint32_t i;
    uint32_t j;
    uint32_t len;

    if (win_path == NULL || unix_path == NULL || size == 0) {
        return;
    }

    len = (uint32_t)strlen(win_path);
    j = 0;

    if (len >= 2 && win_path[1] == ':') {
        if (j < size - 1) {
            unix_path[j++] = '/';
        }
        i = 2;
    } else {
        i = 0;
    }

    for (; i < len && j < size - 1; i++) {
        if (win_path[i] == '\\') {
            if (j > 0 && unix_path[j - 1] == '/') {
                continue;
            }
            unix_path[j++] = '/';
        } else {
            unix_path[j++] = win_path[i];
        }
    }
    unix_path[j] = '\0';
}

HANDLE CreateFileA(const char* lpFileName, uint32_t dwDesiredAccess,
                   uint32_t dwShareMode, void* lpSecurityAttributes,
                   uint32_t dwCreationDisposition, uint32_t dwFlagsAndAttributes,
                   HANDLE hTemplateFile) {
    char unix_path[FS_MAX_NAME];
    win32_file_t* f;
    uint32_t idx;
    HANDLE h;
    int vfs_flags;
    int fd;
    int res;
    struct stat st;

    (void)lpSecurityAttributes;
    (void)dwFlagsAndAttributes;
    (void)hTemplateFile;

    if (lpFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    win32_path_to_unix(lpFileName, unix_path, sizeof(unix_path));

    f = (win32_file_t*)memory_alloc(sizeof(win32_file_t));
    if (f == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    memset(f, 0, sizeof(win32_file_t));

    f->access = dwDesiredAccess;
    f->share_mode = dwShareMode;
    f->offset = 0;

    vfs_flags = 0;
    if ((dwDesiredAccess & GENERIC_WRITE) != 0) {
        vfs_flags |= FS_O_RDWR;
        f->writeable = TRUE;
    } else {
        vfs_flags |= FS_O_RDONLY;
        f->writeable = FALSE;
    }

    res = vfs_stat(unix_path, &st);

    switch (dwCreationDisposition) {
        case CREATE_NEW:
            if (res == 0) {
                memory_free(f);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            vfs_flags |= FS_O_CREAT;
            break;
        case CREATE_ALWAYS:
            vfs_flags |= FS_O_CREAT | FS_O_TRUNC;
            break;
        case OPEN_EXISTING:
            if (res != 0) {
                memory_free(f);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            break;
        case OPEN_ALWAYS:
            if (res != 0) {
                vfs_flags |= FS_O_CREAT;
            }
            break;
        case TRUNCATE_EXISTING:
            if (res != 0) {
                memory_free(f);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            vfs_flags |= FS_O_TRUNC;
            break;
        default:
            memory_free(f);
            SetLastError(ERROR_INVALID_PARAMETER);
            return INVALID_HANDLE_VALUE;
    }

    fd = vfs_open(unix_path, vfs_flags, 0);
    if (fd < 0) {
        memory_free(f);
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }

    f->node = vfs_find_path(unix_path);

    idx = handle_table_alloc();
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        vfs_close(fd);
        memory_free(f);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    g_handle_table[idx].type = HANDLE_TYPE_FILE;
    g_handle_table[idx].data = f;
    g_handle_table[idx].size = (uint64_t)fd;
    h = idx_to_handle(idx);

    SetLastError(ERROR_SUCCESS);
    return h;
}

BOOL ReadFile(HANDLE hFile, void* lpBuffer, uint32_t nNumberOfBytesToRead,
              uint32_t* lpNumberOfBytesRead, void* lpOverlapped) {
    int fd;
    int res;
    win32_file_t* f;
    uint32_t idx;

    (void)lpOverlapped;

    if (hFile == INVALID_HANDLE_VALUE || lpBuffer == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        if (lpNumberOfBytesRead != NULL) *lpNumberOfBytesRead = 0;
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        if (lpNumberOfBytesRead != NULL) *lpNumberOfBytesRead = 0;
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        if (lpNumberOfBytesRead != NULL) *lpNumberOfBytesRead = 0;
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;
    fd = (int)g_handle_table[idx].size;

    res = vfs_read(fd, lpBuffer, (uint64_t)nNumberOfBytesToRead);
    if (res < 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        if (lpNumberOfBytesRead != NULL) *lpNumberOfBytesRead = 0;
        return FALSE;
    }
    if (lpNumberOfBytesRead != NULL) {
        *lpNumberOfBytesRead = (uint32_t)res;
    }
    if (f != NULL) {
        f->offset += (uint64_t)res;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, const void* lpBuffer, uint32_t nNumberOfBytesToWrite,
               uint32_t* lpNumberOfBytesWritten, void* lpOverlapped) {
    int fd;
    int res;
    win32_file_t* f;
    uint32_t idx;

    (void)lpOverlapped;

    if (hFile == INVALID_HANDLE_VALUE || lpBuffer == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        if (lpNumberOfBytesWritten != NULL) *lpNumberOfBytesWritten = 0;
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        if (lpNumberOfBytesWritten != NULL) *lpNumberOfBytesWritten = 0;
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        if (lpNumberOfBytesWritten != NULL) *lpNumberOfBytesWritten = 0;
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;
    fd = (int)g_handle_table[idx].size;

    res = vfs_write(fd, lpBuffer, (uint64_t)nNumberOfBytesToWrite);
    if (res < 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        if (lpNumberOfBytesWritten != NULL) *lpNumberOfBytesWritten = 0;
        return FALSE;
    }
    if (lpNumberOfBytesWritten != NULL) {
        *lpNumberOfBytesWritten = (uint32_t)res;
    }
    if (f != NULL) {
        f->offset += (uint64_t)res;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetFilePointer(HANDLE hFile, int32_t lDistanceToMove,
                    int32_t* lpDistanceToMoveHigh, uint32_t dwMoveMethod) {
    int fd;
    int64_t offset;
    int whence;
    int res;
    win32_file_t* f;
    uint32_t idx;
    int64_t hi;

    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;
    fd = (int)g_handle_table[idx].size;

    if (lpDistanceToMoveHigh != NULL) {
        hi = (int64_t)*lpDistanceToMoveHigh;
        offset = (int64_t)((uint64_t)hi << 32) | (uint64_t)(uint32_t)lDistanceToMove;
    } else {
        offset = (int64_t)lDistanceToMove;
    }

    switch (dwMoveMethod) {
        case FILE_BEGIN:
            whence = 0;
            break;
        case FILE_CURRENT:
            whence = 1;
            break;
        case FILE_END:
            whence = 2;
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    res = vfs_lseek(fd, offset, whence);
    if (res < 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (f != NULL) {
        f->offset = (uint64_t)res;
    }
    if (lpDistanceToMoveHigh != NULL) {
        *lpDistanceToMoveHigh = (int32_t)((uint64_t)res >> 32);
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetEndOfFile(HANDLE hFile) {
    int fd;
    win32_file_t* f;
    uint32_t idx;

    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;
    fd = (int)g_handle_table[idx].size;

    if (f == NULL || !f->writeable) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}


DWORD GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh) {
    int fd;
    uint32_t idx;

    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return INVALID_FILE_SIZE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return INVALID_FILE_SIZE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return INVALID_FILE_SIZE;
    }
    fd = (int)g_handle_table[idx].size;

    if (lpFileSizeHigh != NULL) {
        *lpFileSizeHigh = 0;
    }
    SetLastError(ERROR_SUCCESS);
    return 0;
}







BOOL SetThreadPriorityBoost(HANDLE hThread, BOOL bDisablePriorityBoost) {
    (void)hThread;
    (void)bDisablePriorityBoost;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

int GetThreadPriority(HANDLE hThread) {
    (void)hThread;
    SetLastError(ERROR_SUCCESS);
    return THREAD_PRIORITY_NORMAL;
}

BOOL SetThreadPriority(HANDLE hThread, int nPriority) {
    (void)hThread;
    (void)nPriority;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GetExitCodeThread(HANDLE hThread, DWORD* lpExitCode) {
    (void)hThread;
    if (lpExitCode == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *lpExitCode = 0;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL TerminateProcess(HANDLE hProcess, DWORD dwExitCode) {
    (void)hProcess;
    (void)dwExitCode;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    (void)dwProcessId;
    SetLastError(ERROR_ACCESS_DENIED);
    return NULL;
}

BOOL CreateProcessA(const char* lpApplicationName, char* lpCommandLine,
                    void* lpProcessAttributes, void* lpThreadAttributes,
                    BOOL bInheritHandles, DWORD dwCreationFlags,
                    void* lpEnvironment, const char* lpCurrentDirectory,
                    void* lpStartupInfo, void* lpProcessInformation) {
    (void)lpApplicationName;
    (void)lpCommandLine;
    (void)lpProcessAttributes;
    (void)lpThreadAttributes;
    (void)bInheritHandles;
    (void)dwCreationFlags;
    (void)lpEnvironment;
    (void)lpCurrentDirectory;
    (void)lpStartupInfo;
    (void)lpProcessInformation;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    (void)dwMilliseconds;
    if (hHandle == NULL || hHandle == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }
    return WAIT_OBJECT_0;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                            BOOL bWaitAll, DWORD dwMilliseconds) {
    (void)nCount;
    (void)lpHandles;
    (void)bWaitAll;
    (void)dwMilliseconds;
    SetLastError(ERROR_INVALID_HANDLE);
    return WAIT_FAILED;
}

HANDLE CreateMutexA(void* lpMutexAttributes, BOOL bInitialOwner, const char* lpName) {
    (void)lpMutexAttributes;
    (void)bInitialOwner;
    (void)lpName;
    SetLastError(ERROR_SUCCESS);
    return NULL;
}

BOOL ReleaseMutex(HANDLE hMutex) {
    (void)hMutex;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE CreateEventA(void* lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, const char* lpName) {
    (void)lpEventAttributes;
    (void)bManualReset;
    (void)bInitialState;
    (void)lpName;
    SetLastError(ERROR_SUCCESS);
    return NULL;
}

BOOL SetEvent(HANDLE hEvent) {
    (void)hEvent;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL ResetEvent(HANDLE hEvent) {
    (void)hEvent;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}



void GetLocalTime(void* lpSystemTime) {
    (void)lpSystemTime;
}

void GetSystemTime(void* lpSystemTime) {
    (void)lpSystemTime;
}

BOOL SystemTimeToFileTime(const void* lpSystemTime, uint64_t* lpFileTime) {
    (void)lpSystemTime;
    if (lpFileTime != NULL) {
        *lpFileTime = 0;
    }
    return TRUE;
}

BOOL FileTimeToLocalFileTime(const uint64_t* lpFileTime, uint64_t* lpLocalFileTime) {
    if (lpFileTime == NULL || lpLocalFileTime == NULL) {
        return FALSE;
    }
    *lpLocalFileTime = *lpFileTime;
    return TRUE;
}

BOOL FileTimeToSystemTime(const uint64_t* lpFileTime, void* lpSystemTime) {
    (void)lpFileTime;
    (void)lpSystemTime;
    return TRUE;
}

DWORD FormatMessageA(DWORD dwFlags, const void* lpSource,
                    DWORD dwMessageId, DWORD dwLanguageId,
                    char* lpBuffer, DWORD nSize, va_list* Arguments) {
    (void)dwFlags;
    (void)lpSource;
    (void)dwMessageId;
    (void)dwLanguageId;
    (void)Arguments;

    if (lpBuffer == NULL || nSize == 0) {
        return 0;
    }

    strncpy(lpBuffer, "Unknown error", nSize - 1);
    lpBuffer[nSize - 1] = '\0';
    return (DWORD)strlen(lpBuffer);
}






DWORD GetFileType(HANDLE hFile) {
    uint32_t idx;

    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FILE_TYPE_UNKNOWN;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FILE_TYPE_UNKNOWN;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FILE_TYPE_UNKNOWN;
    }

    SetLastError(ERROR_SUCCESS);
    return FILE_TYPE_DISK;
}

BOOL GetFileInformationByHandle(HANDLE hFile, void* lpFileInfo) {
    (void)hFile;
    (void)lpFileInfo;
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

BOOL SetFilePointerEx(HANDLE hFile, int64_t liDistanceToMove,
                      int64_t* lpNewFilePointer, DWORD dwMoveMethod) {
    int fd;
    int64_t offset;
    int whence;
    int res;
    win32_file_t* f;
    uint32_t idx;

    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;
    fd = (int)g_handle_table[idx].size;

    offset = liDistanceToMove;

    switch (dwMoveMethod) {
        case FILE_BEGIN:
            whence = 0;
            break;
        case FILE_CURRENT:
            whence = 1;
            break;
        case FILE_END:
            whence = 2;
            break;
        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }

    res = vfs_lseek(fd, offset, whence);
    if (res < 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (f != NULL) {
        f->offset = (uint64_t)res;
    }
    if (lpNewFilePointer != NULL) {
        *lpNewFilePointer = (int64_t)res;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}









BOOL GetOverlappedResult(HANDLE hFile, void* lpOverlapped,
                        DWORD* lpNumberOfBytesTransferred, BOOL bWait) {
    (void)hFile;
    (void)lpOverlapped;
    if (lpNumberOfBytesTransferred != NULL) {
        *lpNumberOfBytesTransferred = 0;
    }
    (void)bWait;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

























BOOL GetSystemTimeAsFileTime(uint64_t* lpSystemTimeAsFileTime) {
    if (lpSystemTimeAsFileTime == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *lpSystemTimeAsFileTime = 0ULL;
    return TRUE;
}

void GetSystemTimeAsFileTimeRaw(uint64_t* lpSystemTimeAsFileTime) {
    if (lpSystemTimeAsFileTime != NULL) {
        *lpSystemTimeAsFileTime = 0ULL;
    }
}

BOOL FileTimeToDosDateTime(const uint64_t* lpFileTime, WORD* lpFatDate, WORD* lpFatTime) {
    if (lpFatDate != NULL) *lpFatDate = 0;
    if (lpFatTime != NULL) *lpFatTime = 0;
    return TRUE;
}

BOOL DosDateTimeToFileTime(WORD wFatDate, WORD wFatTime, uint64_t* lpFileTime) {
    if (lpFileTime != NULL) *lpFileTime = 0;
    return TRUE;
}

HANDLE CreateSemaphoreA(void* lpSemaphoreAttributes, LONG lInitialCount,
                        LONG lMaximumCount, const char* lpName) {
    (void)lpSemaphoreAttributes;
    (void)lInitialCount;
    (void)lMaximumCount;
    (void)lpName;
    SetLastError(ERROR_SUCCESS);
    return NULL;
}

BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount) {
    (void)hSemaphore;
    (void)lReleaseCount;
    if (lpPreviousCount != NULL) {
        *lpPreviousCount = 0;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE CreateNamedPipeA(const char* lpName, DWORD dwOpenMode,
                         DWORD dwPipeMode, DWORD nMaxInstances,
                         DWORD nOutBufferSize, DWORD nInBufferSize,
                         DWORD nDefaultTimeOut, void* lpSecurityAttributes) {
    (void)lpName;
    (void)dwOpenMode;
    (void)dwPipeMode;
    (void)nMaxInstances;
    (void)nOutBufferSize;
    (void)nInBufferSize;
    (void)nDefaultTimeOut;
    (void)lpSecurityAttributes;
    SetLastError(ERROR_SUCCESS);
    return NULL;
}

BOOL ConnectNamedPipe(HANDLE hNamedPipe, void* lpOverlapped) {
    (void)hNamedPipe;
    (void)lpOverlapped;
    SetLastError(ERROR_PIPE_NOT_CONNECTED);
    return FALSE;
}

BOOL DisconnectNamedPipe(HANDLE hNamedPipe) {
    (void)hNamedPipe;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE GetStdHandle(DWORD nStdHandle) {
    switch (nStdHandle) {
        case STD_INPUT_HANDLE:
            return (HANDLE)(uintptr_t)0xFFFFFFF0ULL;
        case STD_OUTPUT_HANDLE:
            return (HANDLE)(uintptr_t)0xFFFFFFF1ULL;
        case STD_ERROR_HANDLE:
            return (HANDLE)(uintptr_t)0xFFFFFFF2ULL;
        default:
            return INVALID_HANDLE_VALUE;
    }
}

BOOL SetStdHandle(DWORD nStdHandle, HANDLE hHandle) {
    (void)nStdHandle;
    (void)hHandle;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL CreatePipe(HANDLE* hReadPipe, HANDLE* hWritePipe,
                void* lpPipeAttributes, DWORD nSize) {
    (void)hReadPipe;
    (void)hWritePipe;
    (void)lpPipeAttributes;
    (void)nSize;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL PeekNamedPipe(HANDLE hNamedPipe, void* lpBuffer, DWORD nBufferSize,
                   DWORD* lpBytesRead, DWORD* lpTotalBytesAvail,
                   DWORD* lpBytesLeftThisMessage) {
    (void)hNamedPipe;
    (void)lpBuffer;
    (void)nBufferSize;
    if (lpBytesRead != NULL) *lpBytesRead = 0;
    if (lpTotalBytesAvail != NULL) *lpTotalBytesAvail = 0;
    if (lpBytesLeftThisMessage != NULL) *lpBytesLeftThisMessage = 0;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GetNamedPipeInfo(HANDLE hNamedPipe, DWORD* lpFlags, DWORD* lpOutBufferSize,
                      DWORD* lpInBufferSize, DWORD* lpMaxInstances) {
    (void)hNamedPipe;
    if (lpFlags != NULL) *lpFlags = 0;
    if (lpOutBufferSize != NULL) *lpOutBufferSize = 0;
    if (lpInBufferSize != NULL) *lpInBufferSize = 0;
    if (lpMaxInstances != NULL) *lpMaxInstances = 0;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetNamedPipeHandleState(HANDLE hNamedPipe, DWORD* lpMode,
                             DWORD* lpMaxCollectionCount,
                             DWORD* lpCollectDataTimeout) {
    (void)hNamedPipe;
    (void)lpMode;
    (void)lpMaxCollectionCount;
    (void)lpCollectDataTimeout;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}


DWORD GetFullPathNameW(const WCHAR* lpFileName, DWORD nBufferLength,
                       WCHAR* lpBuffer, WCHAR** lpFilePart) {
    DWORD len;

    if (lpFileName == NULL) {
        return 0;
    }

    len = 0;
    while (lpFileName[len] != 0) len++;

    if (lpBuffer == NULL || nBufferLength == 0) {
        return len + 1;
    }

    if (nBufferLength <= len) {
        return len + 1;
    }

    for (DWORD i = 0; i < len && i < nBufferLength - 1; i++) {
        lpBuffer[i] = lpFileName[i];
    }
    lpBuffer[len < nBufferLength ? len : nBufferLength - 1] = '\0';

    if (lpFilePart != NULL) {
        *lpFilePart = (WCHAR*)lpBuffer;
    }

    return len;
}

char* GetCommandLineA(void) {
    static char cmdline[] = "";
    return cmdline;
}

WCHAR* GetCommandLineW(void) {
    static WCHAR cmdline[] = L"";
    return cmdline;
}

const char* GetEnvStr(const char* name) {
    int idx = env_find(name);
    if (idx < 0) {
        return NULL;
    }
    return g_env_table[idx].value;
}

int GetEnvCount(void) {
    env_init_defaults();
    return (int)g_env_count;
}

const char* GetEnvByIndex(int idx) {
    env_init_defaults();
    if (idx < 0 || idx >= (int)g_env_count) {
        return NULL;
    }
    return g_env_table[idx].value;
}

const char* GetEnvNameByIndex(int idx) {
    env_init_defaults();
    if (idx < 0 || idx >= (int)g_env_count) {
        return NULL;
    }
    return g_env_table[idx].name;
}

void** __imp_GetModuleHandleA = NULL;
void** __imp_GetProcAddress = NULL;
void** __imp_LoadLibraryA = NULL;

BOOL FlushFileBuffers(HANDLE hFile) {
    (void)hFile;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE FindFirstFileA(const char* lpFileName, void* lpFindFileData) {
    char dir_path[FS_MAX_NAME];
    char pattern[FS_MAX_NAME];
    uint32_t len;
    uint32_t last_slash;
    uint32_t i;
    win32_find_t* ctx;
    WIN32_FIND_DATAA* fd;
    uint32_t idx;
    HANDLE h;
    char unix_path[FS_MAX_NAME];

    if (lpFileName == NULL || lpFindFileData == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    fd = (WIN32_FIND_DATAA*)lpFindFileData;

    len = (uint32_t)strlen(lpFileName);
    last_slash = 0;
    for (i = 0; i < len; i++) {
        if (lpFileName[i] == '\\' || lpFileName[i] == '/') {
            last_slash = i + 1;
        }
    }

    memset(dir_path, 0, sizeof(dir_path));
    if (last_slash > 0) {
        if (last_slash - 1 < sizeof(dir_path) - 1) {
            memcpy(dir_path, lpFileName, (size_t)(last_slash - 1));
            dir_path[last_slash - 1] = '\0';
        }
    } else {
        strncpy(dir_path, ".", sizeof(dir_path) - 1);
        dir_path[sizeof(dir_path) - 1] = '\0';
    }

    memset(pattern, 0, sizeof(pattern));
    strncpy(pattern, lpFileName + last_slash, sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = '\0';

    ctx = (win32_find_t*)memory_alloc(sizeof(win32_find_t));
    if (ctx == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    memset(ctx, 0, sizeof(win32_find_t));

    strncpy(ctx->pattern, pattern, sizeof(ctx->pattern) - 1);
    ctx->pattern[sizeof(ctx->pattern) - 1] = '\0';
    strncpy(ctx->base_path, dir_path, sizeof(ctx->base_path) - 1);
    ctx->base_path[sizeof(ctx->base_path) - 1] = '\0';
    ctx->index = 0;

    win32_path_to_unix(dir_path, unix_path, sizeof(unix_path));
    ctx->dir_node = NULL;

    idx = handle_table_alloc();
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        memory_free(ctx);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }
    g_handle_table[idx].type = HANDLE_TYPE_FIND;
    g_handle_table[idx].data = ctx;
    g_handle_table[idx].size = 0;
    h = idx_to_handle(idx);

    memset(fd, 0, sizeof(*fd));
    fd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    strncpy(fd->cFileName, ".", sizeof(fd->cFileName) - 1);
    fd->cFileName[sizeof(fd->cFileName) - 1] = '\0';

    SetLastError(ERROR_SUCCESS);
    return h;
}

BOOL FindNextFileA(HANDLE hFindFile, void* lpFindFileData) {
    WIN32_FIND_DATAA* fd;
    win32_find_t* ctx;
    uint32_t idx;
    char name_buf[FS_MAX_NAME];
    int res;

    if (hFindFile == INVALID_HANDLE_VALUE || lpFindFileData == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    idx = handle_to_idx(hFindFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FIND) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    ctx = (win32_find_t*)g_handle_table[idx].data;
    if (ctx == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    fd = (WIN32_FIND_DATAA*)lpFindFileData;

    ctx->index++;

    if (ctx->index == 1) {
        memset(fd, 0, sizeof(*fd));
        fd->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        strncpy(fd->cFileName, "..", sizeof(fd->cFileName) - 1);
        fd->cFileName[sizeof(fd->cFileName) - 1] = '\0';
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }

    if (ctx->dir_node != NULL && ctx->dir_node->readdir != NULL) {
        memset(name_buf, 0, sizeof(name_buf));
        res = ctx->dir_node->readdir(ctx->dir_node, ctx->index - 2,
                                     name_buf, sizeof(name_buf) - 1);
        if (res == 0) {
            memset(fd, 0, sizeof(*fd));
            fd->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            strncpy(fd->cFileName, name_buf, sizeof(fd->cFileName) - 1);
            fd->cFileName[sizeof(fd->cFileName) - 1] = '\0';
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
    }

    SetLastError(ERROR_NO_MORE_ITEMS);
    return FALSE;
}

BOOL FindClose(HANDLE hFindFile) {
    return CloseHandle(hFindFile);
}

BOOL CreateDirectoryA(const char* lpPathName, void* lpSecurityAttributes) {
    char unix_path[FS_MAX_NAME];
    int res;

    (void)lpSecurityAttributes;

    if (lpPathName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    win32_path_to_unix(lpPathName, unix_path, sizeof(unix_path));
    res = vfs_mkdir(unix_path, 0755);
    if (res != 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL RemoveDirectoryA(const char* lpPathName) {
    char unix_path[FS_MAX_NAME];
    int res;

    if (lpPathName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    win32_path_to_unix(lpPathName, unix_path, sizeof(unix_path));
    res = vfs_rmdir(unix_path);
    if (res != 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL DeleteFileA(const char* lpFileName) {
    char unix_path[FS_MAX_NAME];
    int res;

    if (lpFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    win32_path_to_unix(lpFileName, unix_path, sizeof(unix_path));
    res = vfs_unlink(unix_path);
    if (res != 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL MoveFileA(const char* lpExistingFileName, const char* lpNewFileName) {
    (void)lpExistingFileName;
    (void)lpNewFileName;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL CopyFileA(const char* lpExistingFileName, const char* lpNewFileName, BOOL bFailIfExists) {
    (void)lpExistingFileName;
    (void)lpNewFileName;
    (void)bFailIfExists;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

uint32_t GetFileAttributesA(const char* lpFileName) {
    char unix_path[FS_MAX_NAME];
    struct stat st;
    int res;
    uint32_t attrs;

    if (lpFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0xFFFFFFFFU;
    }

    win32_path_to_unix(lpFileName, unix_path, sizeof(unix_path));
    memset(&st, 0, sizeof(st));
    res = vfs_stat(unix_path, &st);
    if (res != 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return 0xFFFFFFFFU;
    }

    attrs = FILE_ATTRIBUTE_NORMAL;
    if ((st.st_mode & 0xF000ULL) == 0x4000ULL) {
        attrs = FILE_ATTRIBUTE_DIRECTORY;
    }
    SetLastError(ERROR_SUCCESS);
    return attrs;
}

BOOL GetFileSizeEx(HANDLE hFile, int64_t* lpFileSize) {
    int fd;
    struct stat st;
    uint32_t idx;
    win32_file_t* f;
    char path_buf[FS_MAX_NAME];

    if (hFile == INVALID_HANDLE_VALUE || lpFileSize == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;
    fd = (int)g_handle_table[idx].size;

    (void)fd;
    (void)path_buf;

    if (f != NULL && f->node != NULL) {
        *lpFileSize = (int64_t)f->node->size;
    } else {
        memset(&st, 0, sizeof(st));
        *lpFileSize = (int64_t)st.st_size;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

uint32_t GetLastError(void) {
    return g_last_error;
}

void SetLastError(uint32_t dwErrCode) {
    g_last_error = dwErrCode;
}

typedef struct {
    HMODULE base;
    const char* name;
} known_module_t;

#define KNOWN_MODULE_COUNT 5

static const known_module_t g_known_modules[KNOWN_MODULE_COUNT] = {
    { (HMODULE)(uintptr_t)0x00400000ULL, NULL },
    { (HMODULE)(uintptr_t)0x40000000ULL, "kernel32.dll" },
    { (HMODULE)(uintptr_t)0x50000000ULL, "user32.dll" },
    { (HMODULE)(uintptr_t)0x60000000ULL, "gdi32.dll" },
    { (HMODULE)(uintptr_t)0x70000000ULL, "ntdll.dll" }
};

typedef struct {
    const char* name;
    FARPROC addr;
} proc_entry_t;

extern uint32_t GetModuleFileNameA(HMODULE hModule, char* lpFilename, uint32_t nSize);
extern BOOL SetVolumeLabelA(const char* lpRootPathName, const char* lpVolumeName);

static const proc_entry_t g_kernel32_procs[] = {
    { "GetTickCount", (FARPROC)GetTickCount },
    { "GetTickCount64", (FARPROC)GetTickCount64 },
    { "Sleep", (FARPROC)Sleep },
    { "CreateThread", (FARPROC)CreateThread },
    { "TerminateThread", (FARPROC)TerminateThread },
    { "CloseHandle", (FARPROC)CloseHandle },
    { "WaitForSingleObject", (FARPROC)WaitForSingleObject },
    { "GetLastError", (FARPROC)GetLastError },
    { "SetLastError", (FARPROC)SetLastError },
    { "GetCurrentProcessId", (FARPROC)GetCurrentProcessId },
    { "GetCurrentThreadId", (FARPROC)GetCurrentThreadId },
    { "GetCurrentProcess", (FARPROC)GetCurrentProcess },
    { "GetCurrentThread", (FARPROC)GetCurrentThread },
    { "SetProcessPriorityBoost", (FARPROC)SetProcessPriorityBoost },
    { "GetExitCodeProcess", (FARPROC)GetExitCodeProcess },
    { "ReadFile", (FARPROC)ReadFile },
    { "WriteFile", (FARPROC)WriteFile },
    { "CreateFileA", (FARPROC)CreateFileA },
    { "SetFilePointer", (FARPROC)SetFilePointer },
    { "SetEndOfFile", (FARPROC)SetEndOfFile },
    { "FlushFileBuffers", (FARPROC)FlushFileBuffers },
    { "FindFirstFileA", (FARPROC)FindFirstFileA },
    { "FindNextFileA", (FARPROC)FindNextFileA },
    { "FindClose", (FARPROC)FindClose },
    { "CreateDirectoryA", (FARPROC)CreateDirectoryA },
    { "RemoveDirectoryA", (FARPROC)RemoveDirectoryA },
    { "DeleteFileA", (FARPROC)DeleteFileA },
    { "MoveFileA", (FARPROC)MoveFileA },
    { "CopyFileA", (FARPROC)CopyFileA },
    { "GetFileAttributesA", (FARPROC)GetFileAttributesA },
    { "GetFileSizeEx", (FARPROC)GetFileSizeEx },
    { "GetModuleFileNameA", (FARPROC)GetModuleFileNameA },
    { "GetModuleHandleA", (FARPROC)GetModuleHandleA },
    { "GetProcAddress", (FARPROC)GetProcAddress },
    { "LoadLibraryA", (FARPROC)LoadLibraryA },
    { "FreeLibrary", (FARPROC)FreeLibrary },
    { "GlobalAlloc", (FARPROC)GlobalAlloc },
    { "GlobalFree", (FARPROC)GlobalFree },
    { "GlobalLock", (FARPROC)GlobalLock },
    { "GlobalUnlock", (FARPROC)GlobalUnlock },
    { "GlobalSize", (FARPROC)GlobalSize },
    { "LocalAlloc", (FARPROC)LocalAlloc },
    { "LocalFree", (FARPROC)LocalFree },
    { "LocalLock", (FARPROC)LocalLock },
    { "LocalUnlock", (FARPROC)LocalUnlock },
    { "LocalSize", (FARPROC)LocalSize },
    { "VirtualAlloc", (FARPROC)VirtualAlloc },
    { "VirtualFree", (FARPROC)VirtualFree },
    { "VirtualProtect", (FARPROC)VirtualProtect },
    { "VirtualQuery", (FARPROC)VirtualQuery },
    { "HeapCreate", (FARPROC)HeapCreate },
    { "HeapDestroy", (FARPROC)HeapDestroy },
    { "HeapAlloc", (FARPROC)HeapAlloc },
    { "HeapFree", (FARPROC)HeapFree },
    { "GetProcessHeap", (FARPROC)GetProcessHeap },
    { "GetSystemInfo", (FARPROC)GetSystemInfo },
    { "GetVersion", (FARPROC)GetVersion },
    { "GetVersionExA", (FARPROC)GetVersionExA },
    { "GetSystemDirectoryA", (FARPROC)GetSystemDirectoryA },
    { "GetWindowsDirectoryA", (FARPROC)GetWindowsDirectoryA },
    { "GetTempPathA", (FARPROC)GetTempPathA },
    { "GetEnvironmentVariableA", (FARPROC)GetEnvironmentVariableA },
    { "SetEnvironmentVariableA", (FARPROC)SetEnvironmentVariableA },
    { "ExpandEnvironmentStringsA", (FARPROC)ExpandEnvironmentStringsA },
    { "GetComputerNameA", (FARPROC)GetComputerNameA },
    { "GetUserNameA", (FARPROC)GetUserNameA },
    { "lstrlenA", (FARPROC)lstrlenA },
    { "lstrcpyA", (FARPROC)lstrcpyA },
    { "lstrcpynA", (FARPROC)lstrcpynA },
    { "lstrcmpA", (FARPROC)lstrcmpA },
    { "lstrcmpiA", (FARPROC)lstrcmpiA },
    { "lstrcatA", (FARPROC)lstrcatA },
    { "CharNextA", (FARPROC)CharNextA },
    { "CharPrevA", (FARPROC)CharPrevA },
    { "IsCharAlphaA", (FARPROC)IsCharAlphaA },
    { "IsCharAlphaNumericA", (FARPROC)IsCharAlphaNumericA },
    { "IsCharUpperA", (FARPROC)IsCharUpperA },
    { "IsCharLowerA", (FARPROC)IsCharLowerA },
    { "CharUpperA", (FARPROC)CharUpperA },
    { "CharLowerA", (FARPROC)CharLowerA },
    { "wvsprintfA", (FARPROC)wvsprintfA },
    { "RegOpenKeyExA", (FARPROC)RegOpenKeyExA },
    { "RegCreateKeyExA", (FARPROC)RegCreateKeyExA },
    { "RegCloseKey", (FARPROC)RegCloseKey },
    { "RegQueryValueExA", (FARPROC)RegQueryValueExA },
    { "RegSetValueExA", (FARPROC)RegSetValueExA },
    { "RegEnumKeyExA", (FARPROC)RegEnumKeyExA },
    { "RegEnumValueA", (FARPROC)RegEnumValueA },
    { "RegDeleteKeyA", (FARPROC)RegDeleteKeyA },
    { "RegDeleteValueA", (FARPROC)RegDeleteValueA },
    { "RegFlushKey", (FARPROC)RegFlushKey },
    { "LockFile", (FARPROC)LockFile },
    { "UnlockFile", (FARPROC)UnlockFile },
    { "LockFileEx", (FARPROC)LockFileEx },
    { "UnlockFileEx", (FARPROC)UnlockFileEx },
    { "SetVolumeLabelA", (FARPROC)SetVolumeLabelA },
    { "GetLogicalDriveStringsA", (FARPROC)GetLogicalDriveStringsA },
    { "GetDriveTypeA", (FARPROC)GetDriveTypeA },
    { "GetDiskFreeSpaceA", (FARPROC)GetDiskFreeSpaceA },
    { "GetDiskFreeSpaceExA", (FARPROC)GetDiskFreeSpaceExA },
    { "GetFileTime", (FARPROC)GetFileTime },
    { "SetFileTime", (FARPROC)SetFileTime },
    { "GetFileAttributesExA", (FARPROC)GetFileAttributesExA },
    { "SetFileAttributesA", (FARPROC)SetFileAttributesA },
    { "GetFullPathNameA", (FARPROC)GetFullPathNameA },
    { "GetShortPathNameA", (FARPROC)GetShortPathNameA },
    { "GetLongPathNameA", (FARPROC)GetLongPathNameA },
    { "SearchPathA", (FARPROC)SearchPathA },
    { "CreateFileMappingA", (FARPROC)CreateFileMappingA },
    { "MapViewOfFile", (FARPROC)MapViewOfFile },
    { "UnmapViewOfFile", (FARPROC)UnmapViewOfFile },
    { "QueryPerformanceCounter", (FARPROC)QueryPerformanceCounter },
    { "QueryPerformanceFrequency", (FARPROC)QueryPerformanceFrequency },
    { "SleepEx", (FARPROC)SleepEx },
    { "IsProcessorFeaturePresent", (FARPROC)IsProcessorFeaturePresent },
    { "GetSystemPowerStatus", (FARPROC)GetSystemPowerStatus },
    { "GetSystemMetrics", (FARPROC)GetSystemMetrics },
    { "GetSystemDefaultLangID", (FARPROC)GetSystemDefaultLangID },
    { "GetUserDefaultLangID", (FARPROC)GetUserDefaultLangID },
    { "GetSystemDefaultLCID", (FARPROC)GetSystemDefaultLCID },
    { "GetUserDefaultLCID", (FARPROC)GetUserDefaultLCID },
    { "IsValidLocale", (FARPROC)IsValidLocale },
    { "EnumSystemLocalesA", (FARPROC)EnumSystemLocalesA },
    { "CompareStringA", (FARPROC)CompareStringA },
    { "LCMapStringA", (FARPROC)LCMapStringA },
    { "GetStringTypeA", (FARPROC)GetStringTypeA },
    { "ReadProcessMemory", (FARPROC)ReadProcessMemory },
    { "WriteProcessMemory", (FARPROC)WriteProcessMemory },
    { "GetModuleHandleW", (FARPROC)GetModuleHandleW },
    { "GetModuleFileNameW", (FARPROC)GetModuleFileNameW },
    { "LoadLibraryW", (FARPROC)LoadLibraryW },
    { "LoadLibraryExA", (FARPROC)LoadLibraryExA },
    { "LoadLibraryExW", (FARPROC)LoadLibraryExW },
    { "FreeLibraryAndExitThread", (FARPROC)FreeLibraryAndExitThread },
    { "GetDllDirectoryA", (FARPROC)GetDllDirectoryA },
    { "SetDllDirectoryA", (FARPROC)SetDllDirectoryA },
    { "VirtualQueryEx", (FARPROC)VirtualQueryEx },
    { "HeapReAlloc", (FARPROC)HeapReAlloc },
    { "HeapSize", (FARPROC)HeapSize },
    { "HeapValidate", (FARPROC)HeapValidate },
    { "HeapCompact", (FARPROC)HeapCompact },
    { "HeapSetInformation", (FARPROC)HeapSetInformation },
    { "HeapQueryInformation", (FARPROC)HeapQueryInformation },
    { "InitializeCriticalSection", (FARPROC)InitializeCriticalSection },
    { "EnterCriticalSection", (FARPROC)EnterCriticalSection },
    { "TryEnterCriticalSection", (FARPROC)TryEnterCriticalSection },
    { "LeaveCriticalSection", (FARPROC)LeaveCriticalSection },
    { "DeleteCriticalSection", (FARPROC)DeleteCriticalSection },
    { "InitializeCriticalSectionAndSpinCount", (FARPROC)InitializeCriticalSectionAndSpinCount },
    { "InterlockedCompareExchange", (FARPROC)InterlockedCompareExchange },
    { "InterlockedIncrement", (FARPROC)InterlockedIncrement },
    { "InterlockedDecrement", (FARPROC)InterlockedDecrement },
    { "InterlockedExchange", (FARPROC)InterlockedExchange },
    { "InterlockedExchangeAdd", (FARPROC)InterlockedExchangeAdd },
    { "InterlockedCompareExchangePointer", (FARPROC)InterlockedCompareExchangePointer },
    { "InterlockedExchangePointer", (FARPROC)InterlockedExchangePointer },
    { "OutputDebugStringA", (FARPROC)OutputDebugStringA },
    { "DebugBreak", (FARPROC)DebugBreak },
    { "IsDebuggerPresent", (FARPROC)IsDebuggerPresent },
    { "FatalExit", (FARPROC)FatalExit },
    { "RaiseException", (FARPROC)RaiseException },
    { "SetErrorMode", (FARPROC)SetErrorMode },
    { "Beep", (FARPROC)Beep },
    { "GenerateConsoleCtrlEvent", (FARPROC)GenerateConsoleCtrlEvent },
    { "AllocConsole", (FARPROC)AllocConsole },
    { "FreeConsole", (FARPROC)FreeConsole },
    { "GetConsoleWindow", (FARPROC)GetConsoleWindow },
    { "SetConsoleTitleA", (FARPROC)SetConsoleTitleA },
    { "GetConsoleTitleA", (FARPROC)GetConsoleTitleA },
    { "SetConsoleTextAttribute", (FARPROC)SetConsoleTextAttribute },
    { "GetConsoleScreenBufferInfo", (FARPROC)GetConsoleScreenBufferInfo },
    { "SetConsoleCursorPosition", (FARPROC)SetConsoleCursorPosition },
    { "SetConsoleCursorInfo", (FARPROC)SetConsoleCursorInfo },
    { "FillConsoleOutputCharacterA", (FARPROC)FillConsoleOutputCharacterA },
    { "FillConsoleOutputAttribute", (FARPROC)FillConsoleOutputAttribute },
    { "ScrollConsoleScreenBufferA", (FARPROC)ScrollConsoleScreenBufferA },
    { "WriteConsoleA", (FARPROC)WriteConsoleA },
    { "ReadConsoleA", (FARPROC)ReadConsoleA },
    { NULL, NULL }
};

HMODULE GetModuleHandleA(const char* lpModuleName) {
    int i;

    if (lpModuleName == NULL) {
        return g_known_modules[0].base;
    }

    for (i = 1; i < KNOWN_MODULE_COUNT; i++) {
        if (g_known_modules[i].name != NULL &&
            strcmp(g_known_modules[i].name, lpModuleName) == 0) {
            return g_known_modules[i].base;
        }
    }

    SetLastError(ERROR_FILE_NOT_FOUND);
    return NULL;
}

static void* pe_import_resolver(const char* dll, const char* name, uint16_t ordinal) {
    FARPROC result;
    uint32_t i;
    (void)ordinal;

    result = NULL;

    if (dll != NULL && (strcmp(dll, "kernel32.dll") == 0 ||
                        strcmp(dll, "KERNEL32.DLL") == 0)) {
        for (i = 0; g_kernel32_procs[i].name != NULL; i++) {
            if (name != NULL && strcmp(g_kernel32_procs[i].name, name) == 0) {
                result = g_kernel32_procs[i].addr;
                break;
            }
        }
    }

    return (void*)result;
}

HMODULE LoadLibraryA(const char* lpLibFileName) {
    char unix_path[FS_MAX_NAME];
    HMODULE result;
    int fd;
    struct stat st;
    void* file_buf;
    void* image_buf;
    int res;
    uint32_t img_size;

    if (lpLibFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    result = GetModuleHandleA(lpLibFileName);
    if (result != NULL) {
        SetLastError(ERROR_SUCCESS);
        return result;
    }

    win32_path_to_unix(lpLibFileName, unix_path, sizeof(unix_path));

    memset(&st, 0, sizeof(st));
    res = vfs_stat(unix_path, &st);
    if (res != 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return NULL;
    }

    fd = vfs_open(unix_path, FS_O_RDONLY, 0);
    if (fd < 0) {
        SetLastError(ERROR_ACCESS_DENIED);
        return NULL;
    }

    file_buf = memory_alloc(st.st_size + 16ULL);
    if (file_buf == NULL) {
        vfs_close(fd);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    memset(file_buf, 0, st.st_size + 16ULL);

    res = vfs_read(fd, file_buf, st.st_size);
    vfs_close(fd);
    if (res <= 0) {
        memory_free(file_buf);
        SetLastError(ERROR_READ_FAULT);
        return NULL;
    }

    res = pe_validate((const uint8_t*)file_buf, (uint64_t)res);
    if (res != 0) {
        memory_free(file_buf);
        SetLastError(ERROR_BAD_FORMAT);
        return NULL;
    }

    img_size = pe_get_image_size((const uint8_t*)file_buf);
    if (img_size == 0) {
        memory_free(file_buf);
        SetLastError(ERROR_BAD_FORMAT);
        return NULL;
    }

    image_buf = memory_alloc((uint64_t)img_size + 4096ULL);
    if (image_buf == NULL) {
        memory_free(file_buf);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    memset(image_buf, 0, (uint64_t)img_size + 4096ULL);

    res = pe_load_sections((const uint8_t*)file_buf, (uint8_t*)image_buf);
    if (res != 0) {
        memory_free(image_buf);
        memory_free(file_buf);
        SetLastError(ERROR_BAD_FORMAT);
        return NULL;
    }

    (void)pe_get_image_base((const uint8_t*)file_buf);
    (void)pe_apply_relocations((const uint8_t*)file_buf, (uint8_t*)image_buf,
                               (uint64_t)(uintptr_t)image_buf);

    (void)pe_resolve_imports((const uint8_t*)file_buf, (uint8_t*)image_buf,
                             pe_import_resolver);

    memory_free(file_buf);

    result = (HMODULE)image_buf;
    SetLastError(ERROR_SUCCESS);
    return result;
}

BOOL FreeLibrary(HMODULE hLibModule) {
    (void)hLibModule;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

FARPROC GetProcAddress(HMODULE hModule, const char* lpProcName) {
    uint32_t i;
    uintptr_t base_val;

    if (hModule == NULL || lpProcName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    base_val = (uintptr_t)hModule;

    if (base_val == 0x40000000ULL) {
        for (i = 0; g_kernel32_procs[i].name != NULL; i++) {
            if (strcmp(g_kernel32_procs[i].name, lpProcName) == 0) {
                SetLastError(ERROR_SUCCESS);
                return g_kernel32_procs[i].addr;
            }
        }
    }

    SetLastError(ERROR_PROC_NOT_FOUND);
    return NULL;
}

uint32_t GetModuleFileNameA(HMODULE hModule, char* lpFilename, uint32_t nSize) {
    const char* name;
    uint32_t len;
    uintptr_t base_val;
    int i;

    if (lpFilename == NULL || nSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    base_val = (uintptr_t)hModule;
    name = NULL;

    if (hModule == NULL || base_val == 0x00400000ULL) {
        name = "C:\\Windows\\system32\\app.exe";
    } else {
        for (i = 1; i < KNOWN_MODULE_COUNT; i++) {
            if ((uintptr_t)g_known_modules[i].base == base_val) {
                name = g_known_modules[i].name;
                break;
            }
        }
    }

    if (name == NULL) {
        name = "C:\\Windows\\system32\\unknown.dll";
    }

    len = (uint32_t)strlen(name);
    if (len >= nSize) {
        len = nSize - 1;
    }
    memcpy(lpFilename, name, len);
    lpFilename[len] = '\0';
    SetLastError(ERROR_SUCCESS);
    return len;
}

typedef struct _SYSTEM_INFO {
    union {
        DWORD dwOemId;
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        };
    };
    DWORD     dwPageSize;
    LPVOID    lpMinimumApplicationAddress;
    LPVOID    lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD     dwNumberOfProcessors;
    DWORD     dwProcessorType;
    DWORD     dwAllocationGranularity;
    WORD      wProcessorLevel;
    WORD      wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

#define PROCESSOR_ARCHITECTURE_AMD64 9

void GetSystemInfo(void* lpSystemInfo) {
    SYSTEM_INFO* si;

    if (lpSystemInfo == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return;
    }
    si = (SYSTEM_INFO*)lpSystemInfo;
    memset(si, 0, sizeof(*si));

    si->wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    si->dwPageSize = 4096;
    si->lpMinimumApplicationAddress = (LPVOID)(uintptr_t)0x00010000ULL;
    si->lpMaximumApplicationAddress = (LPVOID)(uintptr_t)0x7FFFFFFEFFFFULL;
    si->dwActiveProcessorMask = 1ULL;
    si->dwNumberOfProcessors = 1;
    si->dwProcessorType = 8664;
    si->dwAllocationGranularity = 65536;
    si->wProcessorLevel = 6;
    si->wProcessorRevision = 0;

    SetLastError(ERROR_SUCCESS);
}

void GetNativeSystemInfo(void* lpSystemInfo) {
    GetSystemInfo(lpSystemInfo);
}

DWORD GetVersion(void) {
    return 0x0A000005UL;
}

typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR  szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;

#define VER_PLATFORM_WIN32_NT 2

BOOL GetVersionExA(void* lpVersionInformation) {
    OSVERSIONINFOA* vi;

    if (lpVersionInformation == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    vi = (OSVERSIONINFOA*)lpVersionInformation;

    if (vi->dwOSVersionInfoSize < sizeof(OSVERSIONINFOA)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    vi->dwMajorVersion = 10;
    vi->dwMinorVersion = 0;
    vi->dwBuildNumber = 5;
    vi->dwPlatformId = VER_PLATFORM_WIN32_NT;
    memset(vi->szCSDVersion, 0, sizeof(vi->szCSDVersion));
    strncpy(vi->szCSDVersion, "Service Pack 0", sizeof(vi->szCSDVersion) - 1);

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL VerifyVersionInfoA(void* lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask) {
    (void)lpVersionInformation;
    (void)dwTypeMask;
    (void)dwlConditionMask;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

UINT GetSystemDirectoryA(char* lpBuffer, UINT uSize) {
    const char* path = "C:\\Windows\\System32";
    uint32_t len;

    if (lpBuffer == NULL || uSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(path);
    if (len >= uSize) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    memcpy(lpBuffer, path, len + 1);
    SetLastError(ERROR_SUCCESS);
    return len;
}

UINT GetWindowsDirectoryA(char* lpBuffer, UINT uSize) {
    const char* path = "C:\\Windows";
    uint32_t len;

    if (lpBuffer == NULL || uSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(path);
    if (len >= uSize) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    memcpy(lpBuffer, path, len + 1);
    SetLastError(ERROR_SUCCESS);
    return len;
}

UINT GetTempPathA(uint32_t nBufferLength, char* lpBuffer) {
    const char* path = "C:\\Users\\User\\AppData\\Local\\Temp\\";
    uint32_t len;

    if (lpBuffer == NULL || nBufferLength == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(path);
    if (len >= nBufferLength) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    memcpy(lpBuffer, path, len + 1);
    SetLastError(ERROR_SUCCESS);
    return len;
}

DWORD GetEnvironmentVariableA(const char* lpName, char* lpBuffer, DWORD nSize) {
    int idx;
    uint32_t len;

    if (lpName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    idx = env_find(lpName);
    if (idx < 0) {
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }

    len = (uint32_t)strlen(g_env_table[idx].value);
    if (lpBuffer == NULL || nSize == 0) {
        SetLastError(ERROR_SUCCESS);
        return len + 1;
    }
    if (len >= nSize) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    memcpy(lpBuffer, g_env_table[idx].value, len + 1);
    SetLastError(ERROR_SUCCESS);
    return len;
}

#ifndef ERROR_ENVVAR_NOT_FOUND
#define ERROR_ENVVAR_NOT_FOUND 203
#endif

BOOL SetEnvironmentVariableA(const char* lpName, const char* lpValue) {
    int idx;

    if (lpName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    idx = env_find(lpName);

    if (lpValue == NULL) {
        if (idx >= 0) {
            uint32_t j;
            for (j = (uint32_t)idx; j < g_env_count - 1; j++) {
                g_env_table[j] = g_env_table[j + 1];
            }
            g_env_count--;
            memset(&g_env_table[g_env_count], 0, sizeof(env_entry_t));
        }
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }

    if (idx >= 0) {
        strncpy(g_env_table[idx].value, lpValue, sizeof(g_env_table[idx].value) - 1);
        g_env_table[idx].value[sizeof(g_env_table[idx].value) - 1] = '\0';
    } else {
        if (g_env_count >= ENV_MAX_ENTRIES) {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
        strncpy(g_env_table[g_env_count].name, lpName, sizeof(g_env_table[g_env_count].name) - 1);
        g_env_table[g_env_count].name[sizeof(g_env_table[g_env_count].name) - 1] = '\0';
        strncpy(g_env_table[g_env_count].value, lpValue, sizeof(g_env_table[g_env_count].value) - 1);
        g_env_table[g_env_count].value[sizeof(g_env_table[g_env_count].value) - 1] = '\0';
        g_env_count++;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

DWORD ExpandEnvironmentStringsA(const char* lpSrc, char* lpDst, DWORD nSize) {
    uint32_t src_len;
    uint32_t dst_pos;
    uint32_t i;
    uint32_t j;
    char var_name[256];
    int var_idx;
    const char* var_val;
    uint32_t var_len;

    if (lpSrc == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    src_len = (uint32_t)strlen(lpSrc);
    dst_pos = 0;
    i = 0;

    while (i < src_len) {
        if (lpSrc[i] == '%') {
            j = i + 1;
            while (j < src_len && lpSrc[j] != '%' && (j - i - 1) < sizeof(var_name) - 1) {
                j++;
            }
            if (j < src_len && lpSrc[j] == '%') {
                memset(var_name, 0, sizeof(var_name));
                if (j > i + 1) {
                    memcpy(var_name, lpSrc + i + 1, (size_t)(j - i - 1));
                }
                var_name[sizeof(var_name) - 1] = '\0';
                var_idx = env_find(var_name);
                if (var_idx >= 0) {
                    var_val = g_env_table[var_idx].value;
                } else {
                    var_val = "";
                }
                var_len = (uint32_t)strlen(var_val);
                if (lpDst != NULL && dst_pos + var_len < nSize) {
                    memcpy(lpDst + dst_pos, var_val, var_len);
                }
                dst_pos += var_len;
                i = j + 1;
                continue;
            }
        }
        if (lpDst != NULL && dst_pos + 1 < nSize) {
            lpDst[dst_pos] = lpSrc[i];
        }
        dst_pos++;
        i++;
    }

    if (lpDst != NULL && nSize > 0) {
        if (dst_pos < nSize) {
            lpDst[dst_pos] = '\0';
        } else {
            lpDst[nSize - 1] = '\0';
        }
    }

    SetLastError(ERROR_SUCCESS);
    return dst_pos + 1;
}

BOOL GetComputerNameA(char* lpBuffer, uint32_t* nSize) {
    const char* name = "KENUX-PC";
    uint32_t len;

    if (lpBuffer == NULL || nSize == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    len = (uint32_t)strlen(name);
    if (*nSize <= len) {
        *nSize = len + 1;
        SetLastError(ERROR_BUFFER_OVERFLOW);
        return FALSE;
    }

    memcpy(lpBuffer, name, len + 1);
    *nSize = len;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetComputerNameA(const char* lpComputerName) {
    (void)lpComputerName;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL GetUserNameA(char* lpBuffer, uint32_t* pcbBuffer) {
    const char* name = "User";
    uint32_t len;

    if (lpBuffer == NULL || pcbBuffer == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    len = (uint32_t)strlen(name);
    if (*pcbBuffer <= len) {
        *pcbBuffer = len + 1;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    memcpy(lpBuffer, name, len + 1);
    *pcbBuffer = len;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

/* ERROR_BUFFER_OVERFLOW already defined in win32.h */

HANDLE GlobalAlloc(UINT uFlags, uint64_t dwBytes) {
    void* p;
    int slot;

    if (dwBytes == 0) {
        dwBytes = 1;
    }

    p = memory_alloc(dwBytes);
    if (p == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    if ((uFlags & GMEM_ZEROINIT) != 0) {
        memset(p, 0, dwBytes);
    }

    slot = global_alloc_find_slot();
    if (slot >= 0) {
        g_global_alloc_table[slot].used = TRUE;
        g_global_alloc_table[slot].ptr = p;
        g_global_alloc_table[slot].size = dwBytes;
    }

    SetLastError(ERROR_SUCCESS);
    return (HANDLE)p;
}

HGLOBAL GlobalFree(HGLOBAL hMem) {
    int slot;

    if (hMem == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return (HGLOBAL)(uintptr_t)0;
    }

    slot = global_alloc_find_by_ptr((void*)hMem);
    if (slot >= 0) {
        g_global_alloc_table[slot].used = FALSE;
        g_global_alloc_table[slot].ptr = NULL;
        g_global_alloc_table[slot].size = 0;
    }

    memory_free((void*)hMem);
    SetLastError(ERROR_SUCCESS);
    return (HGLOBAL)(uintptr_t)0;
}

LPVOID GlobalLock(HGLOBAL hMem) {
    if (hMem == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }
    SetLastError(ERROR_SUCCESS);
    return (LPVOID)hMem;
}

BOOL GlobalUnlock(HGLOBAL hMem) {
    if (hMem == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

uint64_t GlobalSize(HGLOBAL hMem) {
    int slot;

    if (hMem == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }

    slot = global_alloc_find_by_ptr((void*)hMem);
    if (slot >= 0) {
        SetLastError(ERROR_SUCCESS);
        return g_global_alloc_table[slot].size;
    }

    SetLastError(ERROR_SUCCESS);
    return 0;
}

HGLOBAL GlobalReAlloc(HGLOBAL hMem, uint64_t dwBytes, UINT uFlags) {
    (void)uFlags;
    if (hMem == NULL) {
        return GlobalAlloc(0, dwBytes);
    }
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

HANDLE LocalAlloc(UINT uFlags, uint64_t uBytes) {
    return GlobalAlloc(uFlags, uBytes);
}

HLOCAL LocalFree(HLOCAL hMem) {
    return (HLOCAL)(uintptr_t)GlobalFree((HGLOBAL)hMem);
}

LPVOID LocalLock(HLOCAL hMem) {
    return GlobalLock((HGLOBAL)hMem);
}

BOOL LocalUnlock(HLOCAL hMem) {
    return GlobalUnlock((HGLOBAL)hMem);
}

uint64_t LocalSize(HLOCAL hMem) {
    return GlobalSize((HGLOBAL)hMem);
}

LPVOID VirtualAlloc(LPVOID lpAddress, uint64_t dwSize, DWORD flAllocationType, DWORD flProtect) {
    void* p;

    (void)lpAddress;
    (void)flAllocationType;
    (void)flProtect;

    if (dwSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    p = memory_alloc(dwSize);
    if (p == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    memset(p, 0, dwSize);
    SetLastError(ERROR_SUCCESS);
    return (LPVOID)p;
}

BOOL VirtualFree(LPVOID lpAddress, uint64_t dwSize, DWORD dwFreeType) {
    (void)dwSize;
    (void)dwFreeType;

    if (lpAddress == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    memory_free(lpAddress);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL VirtualProtect(LPVOID lpAddress, uint64_t dwSize, DWORD flNewProtect, PDWORD lpflOldProtect) {
    (void)lpAddress;
    (void)dwSize;
    if (lpflOldProtect != NULL) {
        *lpflOldProtect = flNewProtect;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL VirtualQuery(LPCVOID lpAddress, void* lpBuffer, uint64_t dwLength) {
    (void)lpAddress;
    (void)lpBuffer;
    (void)dwLength;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

static HANDLE g_process_heap = (HANDLE)(uintptr_t)0x00AA0000ULL;

HANDLE HeapCreate(DWORD flOptions, uint64_t dwInitialSize, uint64_t dwMaximumSize) {
    uint32_t idx;
    HANDLE h;
    void* mem;

    (void)flOptions;
    (void)dwInitialSize;
    (void)dwMaximumSize;

    mem = memory_alloc(256);
    if (mem == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    memset(mem, 0, 256);

    idx = handle_table_alloc();
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        memory_free(mem);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    g_handle_table[idx].type = HANDLE_TYPE_HEAP;
    g_handle_table[idx].data = mem;
    g_handle_table[idx].size = 0;
    h = idx_to_handle(idx);

    SetLastError(ERROR_SUCCESS);
    return h;
}

BOOL HeapDestroy(HANDLE hHeap) {
    uint32_t idx;
    void* mem;

    if (hHeap == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    idx = handle_to_idx(hHeap);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_HEAP) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    mem = g_handle_table[idx].data;
    if (mem != NULL) {
        memory_free(mem);
    }
    handle_table_free(idx);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, uint64_t dwBytes) {
    void* p;

    if (hHeap == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    p = memory_alloc(dwBytes);
    if (p == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    if ((dwFlags & HEAP_ZERO_MEMORY) != 0) {
        memset(p, 0, dwBytes);
    }
    SetLastError(ERROR_SUCCESS);
    return p;
}

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    (void)dwFlags;
    if (hHeap == NULL || lpMem == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    memory_free(lpMem);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HANDLE GetProcessHeap(void) {
    SetLastError(ERROR_SUCCESS);
    return g_process_heap;
}

int lstrlenA(const char* lpString) {
    if (lpString == NULL) {
        return 0;
    }
    return (int)strlen(lpString);
}

char* lstrcpyA(char* lpString1, const char* lpString2) {
    if (lpString1 == NULL || lpString2 == NULL) {
        return lpString1;
    }
    return strcpy(lpString1, lpString2);
}

char* lstrcpynA(char* lpString1, const char* lpString2, int iMaxLength) {
    size_t len;
    if (lpString1 == NULL || lpString2 == NULL || iMaxLength <= 0) {
        return lpString1;
    }
    len = strlen(lpString2);
    if (len >= (size_t)iMaxLength) {
        memcpy(lpString1, lpString2, (size_t)(iMaxLength - 1));
        lpString1[iMaxLength - 1] = '\0';
    } else {
        memcpy(lpString1, lpString2, len + 1);
    }
    return lpString1;
}

int lstrcmpA(const char* lpString1, const char* lpString2) {
    if (lpString1 == NULL && lpString2 == NULL) return 0;
    if (lpString1 == NULL) return -1;
    if (lpString2 == NULL) return 1;
    return strcmp(lpString1, lpString2);
}

static int strcasecmp_local(const char* a, const char* b) {
    char ca;
    char cb;
    while (1) {
        ca = *a;
        cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
        if (ca == '\0') {
            break;
        }
        a++;
        b++;
    }
    return 0;
}

int lstrcmpiA(const char* lpString1, const char* lpString2) {
    if (lpString1 == NULL && lpString2 == NULL) return 0;
    if (lpString1 == NULL) return -1;
    if (lpString2 == NULL) return 1;
    return strcasecmp_local(lpString1, lpString2);
}

char* lstrcatA(char* lpString1, const char* lpString2) {
    if (lpString1 == NULL || lpString2 == NULL) {
        return lpString1;
    }
    return strcat(lpString1, lpString2);
}

LPSTR CharNextA(LPCSTR pch) {
    if (pch == NULL) {
        return NULL;
    }
    if (*pch == '\0') {
        return (LPSTR)pch;
    }
    return (LPSTR)(pch + 1);
}

LPSTR CharPrevA(LPCSTR lpStart, LPCSTR lpCurrent) {
    if (lpStart == NULL || lpCurrent == NULL) {
        return NULL;
    }
    if (lpCurrent <= lpStart) {
        return (LPSTR)lpStart;
    }
    return (LPSTR)(lpCurrent - 1);
}

BOOL IsCharAlphaA(CHAR ch) {
    unsigned char c = (unsigned char)ch;
    return (BOOL)((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}

BOOL IsCharAlphaNumericA(CHAR ch) {
    unsigned char c = (unsigned char)ch;
    return (BOOL)((c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9'));
}

BOOL IsCharUpperA(CHAR ch) {
    unsigned char c = (unsigned char)ch;
    return (BOOL)(c >= 'A' && c <= 'Z');
}

BOOL IsCharLowerA(CHAR ch) {
    unsigned char c = (unsigned char)ch;
    return (BOOL)(c >= 'a' && c <= 'z');
}

CHAR CharUpperA(CHAR ch) {
    unsigned char c = (unsigned char)ch;
    if (c >= 'a' && c <= 'z') {
        return (CHAR)(c - (unsigned char)('a' - 'A'));
    }
    return ch;
}

CHAR CharLowerA(CHAR ch) {
    unsigned char c = (unsigned char)ch;
    if (c >= 'A' && c <= 'Z') {
        return (CHAR)(c + (unsigned char)('a' - 'A'));
    }
    return ch;
}

#ifndef ERROR_PROC_NOT_FOUND
#define ERROR_PROC_NOT_FOUND 127
#endif

typedef unsigned char* va_list_ptr;
#define VA_SIZE(type) (sizeof(type) < sizeof(uint64_t) ? sizeof(uint64_t) : sizeof(type))
#define VA_ARG(ap, type) (*(type*)((ap) += VA_SIZE(type), (ap) - VA_SIZE(type)))

int wvsprintfA(char* lpOutput, const char* lpFmt, void* argptr) {
    va_list_ptr ap;
    int count;
    const char* p;
    char* out;
    char num_buf[32];
    int num_len;
    int i;
    char ch;
    const char* s;
    uint64_t uval;
    int64_t sval;
    uint32_t base;
    int width;
    int neg;
    char c;

    if (lpOutput == NULL || lpFmt == NULL) {
        return 0;
    }

    ap = (va_list_ptr)argptr;
    out = lpOutput;
    count = 0;

    for (p = lpFmt; *p != '\0'; p++) {
        if (*p != '%') {
            *out++ = *p;
            count++;
            continue;
        }
        p++;
        if (*p == '\0') {
            break;
        }

        width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        ch = *p;
        neg = 0;

        switch (ch) {
            case 'c':
                c = (char)VA_ARG(ap, int);
                *out++ = c;
                count++;
                break;
            case 's':
                s = VA_ARG(ap, const char*);
                if (s == NULL) {
                    s = "(null)";
                }
                while (*s != '\0') {
                    *out++ = *s++;
                    count++;
                }
                break;
            case 'd':
            case 'i':
                sval = (int64_t)VA_ARG(ap, int);
                if (sval < 0) {
                    neg = 1;
                    uval = (uint64_t)(-(sval + 1)) + 1ULL;
                } else {
                    uval = (uint64_t)sval;
                }
                base = 10;
                goto fmt_num;
            case 'u':
                uval = (uint64_t)VA_ARG(ap, unsigned int);
                base = 10;
                goto fmt_num;
            case 'x':
            case 'X':
                uval = (uint64_t)VA_ARG(ap, unsigned int);
                base = 16;
fmt_num:
                num_len = 0;
                if (uval == 0) {
                    num_buf[num_len++] = '0';
                } else {
                    while (uval > 0 && num_len < (int)(sizeof(num_buf) - 1)) {
                        uint32_t digit;
                        digit = (uint32_t)(uval % base);
                        if (digit < 10) {
                            num_buf[num_len++] = (char)('0' + digit);
                        } else {
                            if (ch == 'X') {
                                num_buf[num_len++] = (char)('A' + (digit - 10));
                            } else {
                                num_buf[num_len++] = (char)('a' + (digit - 10));
                            }
                        }
                        uval /= base;
                    }
                }
                if (neg) {
                    num_buf[num_len++] = '-';
                }
                while (num_len < width && num_len < (int)(sizeof(num_buf) - 1)) {
                    num_buf[num_len++] = ' ';
                }
                for (i = num_len - 1; i >= 0; i--) {
                    *out++ = num_buf[i];
                    count++;
                }
                break;
            case '%':
                *out++ = '%';
                count++;
                break;
            default:
                *out++ = ch;
                count++;
                break;
        }
    }

    *out = '\0';
    return count;
}

BOOL LockFile(HANDLE hFile, uint32_t dwFileOffsetLow,
              uint32_t dwFileOffsetHigh, uint32_t nNumberOfBytesToLockLow,
              uint32_t nNumberOfBytesToLockHigh) {
    (void)hFile;
    (void)dwFileOffsetLow;
    (void)dwFileOffsetHigh;
    (void)nNumberOfBytesToLockLow;
    (void)nNumberOfBytesToLockHigh;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL UnlockFile(HANDLE hFile, uint32_t dwFileOffsetLow,
                uint32_t dwFileOffsetHigh, uint32_t nNumberOfBytesToUnlockLow,
                uint32_t nNumberOfBytesToUnlockHigh) {
    (void)hFile;
    (void)dwFileOffsetLow;
    (void)dwFileOffsetHigh;
    (void)nNumberOfBytesToUnlockLow;
    (void)nNumberOfBytesToUnlockHigh;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, uint32_t dwReserved,
                uint32_t nNumberOfBytesToLockLow, uint32_t nNumberOfBytesToLockHigh,
                void* lpOverlapped) {
    (void)hFile;
    (void)dwFlags;
    (void)dwReserved;
    (void)nNumberOfBytesToLockLow;
    (void)nNumberOfBytesToLockHigh;
    (void)lpOverlapped;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved,
                  uint32_t nNumberOfBytesToUnlockLow, uint32_t nNumberOfBytesToUnlockHigh,
                  void* lpOverlapped) {
    (void)hFile;
    (void)dwReserved;
    (void)nNumberOfBytesToUnlockLow;
    (void)nNumberOfBytesToUnlockHigh;
    (void)lpOverlapped;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetVolumeLabelA(const char* lpRootPathName, const char* lpVolumeName) {
    (void)lpRootPathName;
    (void)lpVolumeName;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

DWORD GetLogicalDriveStringsA(uint32_t nBufferLength, char* lpBuffer) {
    const char* drives = "C:\\\0";
    uint32_t len;

    if (nBufferLength == 0 || lpBuffer == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(drives) + 1;
    if (len > nBufferLength) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    memcpy(lpBuffer, drives, len);
    lpBuffer[len] = '\0';
    SetLastError(ERROR_SUCCESS);
    return len;
}

UINT GetDriveTypeA(const char* lpRootPathName) {
    if (lpRootPathName == NULL) {
        return DRIVE_UNKNOWN;
    }
    if (strlen(lpRootPathName) >= 1 && lpRootPathName[0] == 'C') {
        return DRIVE_FIXED;
    }
    return DRIVE_NO_ROOT_DIR;
}

BOOL GetDiskFreeSpaceA(const char* lpRootPathName, uint32_t* lpSectorsPerCluster,
                       uint32_t* lpBytesPerSector, uint32_t* lpNumberOfFreeClusters,
                       uint32_t* lpTotalNumberOfClusters) {
    (void)lpRootPathName;

    if (lpSectorsPerCluster != NULL) *lpSectorsPerCluster = 8;
    if (lpBytesPerSector != NULL) *lpBytesPerSector = 512;
    if (lpNumberOfFreeClusters != NULL) *lpNumberOfFreeClusters = 1000000;
    if (lpTotalNumberOfClusters != NULL) *lpTotalNumberOfClusters = 2000000;

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GetDiskFreeSpaceExA(const char* lpDirectoryName,
                         uint64_t* lpFreeBytesAvailableToCaller,
                         uint64_t* lpTotalNumberOfBytes,
                         uint64_t* lpTotalNumberOfFreeBytes) {
    (void)lpDirectoryName;

    if (lpFreeBytesAvailableToCaller != NULL)
        *lpFreeBytesAvailableToCaller = 107374182400ULL;
    if (lpTotalNumberOfBytes != NULL)
        *lpTotalNumberOfBytes = 214748364800ULL;
    if (lpTotalNumberOfFreeBytes != NULL)
        *lpTotalNumberOfFreeBytes = 107374182400ULL;

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GetFileTime(HANDLE hFile, int64_t* lpCreationTime,
                 int64_t* lpLastAccessTime, int64_t* lpLastWriteTime) {
    win32_file_t* f;
    uint32_t idx;

    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    idx = handle_to_idx(hFile);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_FILE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    f = (win32_file_t*)g_handle_table[idx].data;

    if (f != NULL && f->node != NULL) {
        if (lpCreationTime != NULL)
            *lpCreationTime = 132537600000000000LL;
        if (lpLastAccessTime != NULL)
            *lpLastAccessTime = 132537600000000000LL;
        if (lpLastWriteTime != NULL)
            *lpLastWriteTime = 132537600000000000LL;
    } else {
        if (lpCreationTime != NULL)
            *lpCreationTime = 0;
        if (lpLastAccessTime != NULL)
            *lpLastAccessTime = 0;
        if (lpLastWriteTime != NULL)
            *lpLastWriteTime = 0;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetFileTime(HANDLE hFile, const int64_t* lpCreationTime,
                 const int64_t* lpLastAccessTime,
                 const int64_t* lpLastWriteTime) {
    (void)lpCreationTime;
    (void)lpLastAccessTime;
    (void)lpLastWriteTime;

    if (hFile == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GetFileAttributesExA(const char* lpFileName, uint32_t fInfoLevelId,
                          void* lpFileInformation) {
    char unix_path[FS_MAX_NAME];
    struct stat st;
    int res;
    uint32_t attrs;

    if (lpFileName == NULL || lpFileInformation == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    win32_path_to_unix(lpFileName, unix_path, sizeof(unix_path));
    memset(&st, 0, sizeof(st));
    res = vfs_stat(unix_path, &st);
    if (res != 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    attrs = FILE_ATTRIBUTE_NORMAL;
    if ((st.st_mode & 0xF000ULL) == 0x4000ULL) {
        attrs = FILE_ATTRIBUTE_DIRECTORY;
    }

    memset(lpFileInformation, 0, 320);

    *(uint32_t*)lpFileInformation = attrs;
    *(uint32_t*)((char*)lpFileInformation + 4) = (uint32_t)(st.st_size & 0xFFFFFFFFUL);
    *(uint32_t*)((char*)lpFileInformation + 8) = (uint32_t)((st.st_size >> 32) & 0xFFFFFFFFUL);
    *(int64_t*)((char*)lpFileInformation + 16) = 132537600000000000LL;
    *(int64_t*)((char*)lpFileInformation + 24) = 132537600000000000LL;
    *(int64_t*)((char*)lpFileInformation + 32) = 132537600000000000LL;

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetFileAttributesA(const char* lpFileName, uint32_t dwFileAttributes) {
    (void)lpFileName;
    (void)dwFileAttributes;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL GetFullPathNameA(const char* lpFileName, uint32_t nBufferLength,
                      char* lpBuffer, char** lpFilePart) {
    uint32_t len;
    const char* last_sep;

    if (lpFileName == NULL || lpBuffer == NULL || nBufferLength == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    last_sep = strrchr(lpFileName, '\\');
    if (last_sep == NULL) {
        last_sep = strrchr(lpFileName, '/');
    }

    if (last_sep != NULL) {
        size_t dir_len = (size_t)(last_sep - lpFileName + 1);
        if (dir_len >= nBufferLength) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return dir_len + 1;
        }
        memcpy(lpBuffer, lpFileName, dir_len);
        lpBuffer[dir_len] = '\0';
        if (lpFilePart != NULL) {
            *lpFilePart = lpBuffer + (int)dir_len;
        }
        return (uint32_t)(dir_len + strlen(last_sep + 1));
    } else {
        len = (uint32_t)strlen(lpFileName);
        if (len >= nBufferLength) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return len + 1;
        }
        strcpy(lpBuffer, lpFileName);
        if (lpFilePart != NULL) {
            *lpFilePart = lpBuffer;
        }
        return len;
    }
}

BOOL GetShortPathNameA(const char* lpszLongPath, char* lpszShortPath,
                       uint32_t cchBuffer) {
    uint32_t i;
    uint32_t j;
    uint32_t len;

    if (lpszLongPath == NULL || lpszShortPath == NULL || cchBuffer == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(lpszLongPath);
    j = 0;
    for (i = 0; i < len && j < cchBuffer - 1; i++) {
        if ((unsigned char)lpszLongPath[i] >= 128) {
            lpszShortPath[j++] = '~';
        } else {
            lpszShortPath[j++] = lpszLongPath[i];
        }
    }
    lpszShortPath[j] = '\0';

    SetLastError(ERROR_SUCCESS);
    return j;
}

BOOL GetLongPathNameA(const char* lpszShortPath, char* lpszLongPath,
                      uint32_t cchBuffer) {
    uint32_t len;

    if (lpszShortPath == NULL || lpszLongPath == NULL || cchBuffer == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(lpszShortPath);
    if (len >= cchBuffer) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    strcpy(lpszLongPath, lpszShortPath);
    SetLastError(ERROR_SUCCESS);
    return len;
}

DWORD SearchPathA(const char* lpPath, const char* lpFileName,
                  const char* lpExtension, uint32_t nBufferLength,
                  char* lpBuffer, char** lpFilePart) {
    char unix_path[FS_MAX_NAME];
    struct stat st;
    int res;
    uint32_t len;

    (void)lpPath;
    (void)lpExtension;

    if (lpFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    win32_path_to_unix(lpFileName, unix_path, sizeof(unix_path));
    res = vfs_stat(unix_path, &st);
    if (res != 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return 0;
    }

    if (lpBuffer != NULL && nBufferLength > 0) {
        len = (uint32_t)strlen(lpFileName);
        if (len >= nBufferLength) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return len + 1;
        }
        strcpy(lpBuffer, lpFileName);
        if (lpFilePart != NULL) {
            *lpFilePart = strrchr(lpBuffer, '\\');
            if (*lpFilePart == NULL) {
                *lpFilePart = strrchr(lpBuffer, '/');
            }
            if (*lpFilePart != NULL) {
                (*lpFilePart)++;
            } else {
                *lpFilePart = lpBuffer;
            }
        }
    }

    SetLastError(ERROR_SUCCESS);
    return (uint32_t)strlen(lpFileName);
}

HANDLE CreateFileMappingA(HANDLE hFile, void* lpFileMappingAttributes,
                          uint32_t flProtect, uint32_t dwMaximumSizeHigh,
                          uint32_t dwMaximumSizeLow, const char* lpName) {
    void* p;
    uint64_t size;
    uint32_t idx;
    HANDLE h;

    (void)lpFileMappingAttributes;
    (void)flProtect;
    (void)lpName;

    if (hFile != INVALID_HANDLE_VALUE && hFile != NULL) {
        idx = handle_to_idx(hFile);
        if (idx != WIN32_INVALID_HANDLE_IDX &&
            g_handle_table[idx].type == HANDLE_TYPE_FILE) {
            win32_file_t* f = (win32_file_t*)g_handle_table[idx].data;
            if (f != NULL && f->node != NULL) {
                size = f->node->size;
            } else {
                size = 4096;
            }
        } else {
            size = 4096;
        }
    } else {
        size = ((uint64_t)dwMaximumSizeHigh << 32) | (uint64_t)dwMaximumSizeLow;
        if (size == 0) size = 4096;
    }

    p = memory_alloc(size);
    if (p == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    memset(p, 0, size);

    idx = handle_table_alloc();
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        memory_free(p);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    g_handle_table[idx].type = HANDLE_TYPE_MEM;
    g_handle_table[idx].data = p;
    g_handle_table[idx].size = size;
    h = idx_to_handle(idx);

    SetLastError(ERROR_SUCCESS);
    return h;
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject, uint32_t dwDesiredAccess,
                     uint32_t dwFileOffsetHigh, uint32_t dwFileOffsetLow,
                     uint64_t dwNumberOfBytesToMap) {
    uint32_t idx;
    void* base;

    (void)dwDesiredAccess;
    (void)dwFileOffsetHigh;
    (void)dwFileOffsetLow;
    (void)dwNumberOfBytesToMap;

    if (hFileMappingObject == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    idx = handle_to_idx(hFileMappingObject);
    if (idx == WIN32_INVALID_HANDLE_IDX) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }
    if (g_handle_table[idx].type != HANDLE_TYPE_MEM) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    base = g_handle_table[idx].data;
    if (base == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return NULL;
    }

    SetLastError(ERROR_SUCCESS);
    return base;
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress) {
    (void)lpBaseAddress;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL FlushViewOfFile(const void* lpBaseAddress, uint64_t dwNumberOfBytesToFlush) {
    (void)lpBaseAddress;
    (void)dwNumberOfBytesToFlush;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL QueryPerformanceCounter(int64_t* lpPerformanceCount) {
    extern uint64_t timer_get_jiffies(void);
    static uint64_t start_jiffies = 0;
    uint64_t current_jiffies;

    if (lpPerformanceCount == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    current_jiffies = timer_get_jiffies();

    if (start_jiffies == 0) {
        start_jiffies = current_jiffies;
    }

    *lpPerformanceCount = (int64_t)((current_jiffies - start_jiffies) * 10000ULL);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL QueryPerformanceFrequency(int64_t* lpFrequency) {
    if (lpFrequency == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *lpFrequency = 10000000LL;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

uint32_t GetTickCount(void) {
    extern uint64_t timer_get_jiffies(void);
    return (uint32_t)(timer_get_jiffies() * 10ULL);
}

uint64_t GetTickCount64(void) {
    extern uint64_t timer_get_jiffies(void);
    return timer_get_jiffies() * 10ULL;
}

void Sleep(uint32_t dwMilliseconds) {
    extern void msleep(uint64_t ms);
    msleep((uint64_t)dwMilliseconds);
}

void SleepEx(uint32_t dwMilliseconds, BOOL bAlertable) {
    (void)bAlertable;
    Sleep(dwMilliseconds);
}

BOOL IsProcessorFeaturePresent(uint32_t ProcessorFeature) {
    switch (ProcessorFeature) {
        case PF_FLOATING_POINT_PRECISION_ERRATA:
        case PF_FLOATING_POINT_EMULATED:
        case PF_COMPARE_EXCHANGE_DOUBLE:
        case PF_MMX_INSTRUCTIONS_AVAILABLE:
        case PF_XMMI_INSTRUCTIONS_AVAILABLE:
        case PF_3DNOW_INSTRUCTIONS_AVAILABLE:
        case PF_RDTSC_INSTRUCTION_AVAILABLE:
        case PF_PAE_ENABLED:
        case PF_XMMI64_INSTRUCTIONS_AVAILABLE:
        case PF_SSE3_INSTRUCTIONS_AVAILABLE:
        case PF_COMPARE_EXCHANGE128:
        case PF_COMPARE64EXCHANGE128:
        case PF_CHANNELS_ENABLED:
        case PF_XSAVE_ENABLED:
        case PF_VIRT_FIRMWARE_ENABLED:
        case PF_RDWRFSGSBASE_AVAILABLE:
        case PF_FASTFAIL_AVAILABLE:
        case PF_ARM_VFP_32_REGISTERS_AVAILABLE:
        case PF_ARM_NEON_INSTRUCTIONS_AVAILABLE:
        case PF_SECOND_LEVEL_ADDRESS_TRANSLATION:
        case PF_VIRT_FIRMWARE_ENFORCED:
        case PF_RDSEED_INSTRUCTION_AVAILABLE:
        case PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE:
        case PF_ARM_64BIT_LOADSTORE_ATOMIC:
        case PF_ARM_EXTERNAL_CACHE_AVAILABLE:
        case PF_ARM_FPCRT_INSTANCES:
        case PF_SSSE3_INSTRUCTIONS_AVAILABLE:
        case PF_SSE41_INSTRUCTIONS_AVAILABLE:
        case PF_SSE42_INSTRUCTIONS_AVAILABLE:
        case PF_AVX_INSTRUCTIONS_AVAILABLE:
        case PF_AVX2_AVAILABLE:
        case PF_AVX512F_INSTRUCTIONS_AVAILABLE:
        case PF_ARM_DMB_INSTRUCTIONS_AVAILABLE:
            return TRUE;
        default:
            return FALSE;
    }
}

BOOL GetSystemPowerStatus(void* lpSystemPowerStatus) {
    (void)lpSystemPowerStatus;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL GetSystemMetrics(int nIndex) {
    (void)nIndex;
    return 0;
}

int GetSystemDefaultLangID(void) {
    return 0x0409;
}

int GetUserDefaultLangID(void) {
    return 0x0409;
}

LCID GetSystemDefaultLCID(void) {
    return 0x0409;
}

LCID GetUserDefaultLCID(void) {
    return 0x0409;
}

BOOL IsValidLocale(uint32_t Locale, uint32_t dwFlags) {
    (void)Locale;
    (void)dwFlags;
    return TRUE;
}

BOOL EnumSystemLocalesA(LOCALE_ENUMPROC lpLocaleEnumProc, uint32_t dwFlags) {
    (void)lpLocaleEnumProc;
    (void)dwFlags;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

int CompareStringA(uint32_t Locale, uint32_t dwCmpFlags,
                   const char* lpString1, int cchCount1,
                   const char* lpString2, int cchCount2) {
    (void)Locale;
    (void)dwCmpFlags;
    (void)cchCount1;
    (void)cchCount2;

    if (lpString1 == NULL && lpString2 == NULL) return CSTR_EQUAL;
    if (lpString1 == NULL) return CSTR_LESS_THAN;
    if (lpString2 == NULL) return CSTR_GREATER_THAN;

    int cmp = strcmp(lpString1, lpString2);
    if (cmp < 0) return CSTR_LESS_THAN;
    if (cmp > 0) return CSTR_GREATER_THAN;
    return CSTR_EQUAL;
}

int LCMapStringA(uint32_t Locale, uint32_t dwMapFlags,
                 const char* lpSrcStr, int cchSrc,
                 char* lpDestStr, int cchDest) {
    (void)Locale;
    (void)dwMapFlags;
    (void)lpDestStr;
    (void)cchDest;

    if (lpSrcStr == NULL) return 0;

    int len = (cchSrc < 0) ? (int)strlen(lpSrcStr) : cchSrc;
    return len;
}

int GetStringTypeA(uint32_t Locale, uint32_t dwInfoType,
                   const char* lpSrcStr, int cchSrc,
                   uint16_t* lpCharType) {
    int i;
    int len;
    unsigned char ch;
    uint16_t type;

    (void)Locale;

    if (lpSrcStr == NULL && cchSrc <= 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (cchSrc < 0) ? (int)strlen(lpSrcStr) : cchSrc;

    for (i = 0; i < len; i++) {
        if (lpCharType != NULL) {
            ch = (unsigned char)lpSrcStr[i];
            type = 0;
            if (ch >= 'a' && ch <= 'z') type |= C1_LOWER;
            if (ch >= 'A' && ch <= 'Z') type |= C1_UPPER;
            if (ch >= '0' && ch <= '9') type |= C1_DIGIT;
            if (ch == ' ' || ch == '\t') type |= C1_SPACE;
            if (ch >= 0x21 && ch <= 0x7E) type |= C1_PUNCT | C1_ALPHA;
            if (ch < 0x20 || ch == 0x7F) type |= C1_CNTRL;
            lpCharType[i] = type;
        }
    }

    return len;
}

BOOL ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress,
                       LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead) {
    (void)hProcess;
    (void)lpBaseAddress;

    if (lpBuffer == NULL || nSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    memset(lpBuffer, 0, nSize);
    if (lpNumberOfBytesRead != NULL) {
        *lpNumberOfBytesRead = nSize;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress,
                        LPCVOID lpBuffer, SIZE_T nSize,
                        SIZE_T* lpNumberOfBytesWritten) {
    (void)hProcess;
    (void)lpBaseAddress;

    if (lpBuffer == NULL || nSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (lpNumberOfBytesWritten != NULL) {
        *lpNumberOfBytesWritten = nSize;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HMODULE GetModuleHandleW(const wchar_t* lpModuleName) {
    (void)lpModuleName;
    SetLastError(ERROR_SUCCESS);
    return (HMODULE)1;
}

DWORD GetModuleFileNameW(HMODULE hModule, wchar_t* lpFilename, DWORD nSize) {
    (void)hModule;
    if (lpFilename == NULL || nSize == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (nSize >= 1) {
        lpFilename[0] = L'\0';
    }
    return 0;
}

HMODULE LoadLibraryW(const wchar_t* lpLibFileName) {
    (void)lpLibFileName;
    SetLastError(ERROR_FILE_NOT_FOUND);
    return NULL;
}

HMODULE LoadLibraryExA(const char* lpLibFileName, HANDLE hFile, uint32_t dwFlags) {
    (void)hFile;
    (void)dwFlags;
    return LoadLibraryA(lpLibFileName);
}

HMODULE LoadLibraryExW(const wchar_t* lpLibFileName, HANDLE hFile, uint32_t dwFlags) {
    (void)lpLibFileName;
    (void)hFile;
    (void)dwFlags;
    SetLastError(ERROR_FILE_NOT_FOUND);
    return NULL;
}

BOOL FreeLibraryAndExitThread(HMODULE hLibModule, uint32_t dwExitCode) {
    (void)dwExitCode;
    FreeLibrary(hLibModule);
    return TRUE;
}

DWORD GetDllDirectoryA(uint32_t nBufferLength, char* lpBuffer) {
    const char* path = "C:\\Windows\\System32";
    uint32_t len;

    if (lpBuffer == NULL || nBufferLength == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    len = (uint32_t)strlen(path);
    if (len >= nBufferLength) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return len + 1;
    }
    memcpy(lpBuffer, path, len + 1);
    SetLastError(ERROR_SUCCESS);
    return len;
}

BOOL SetDllDirectoryA(const char* lpPathName) {
    (void)lpPathName;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress;
    PVOID AllocationBase;
    uint32_t AllocationProtect;
    size_t RegionSize;
    uint32_t State;
    uint32_t Protect;
    uint32_t Type;
} MEMORY_BASIC_INFORMATION;

SIZE_T VirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress,
                      void* lpBuffer, SIZE_T dwLength) {
    MEMORY_BASIC_INFORMATION* mbi = (MEMORY_BASIC_INFORMATION*)lpBuffer;
    (void)hProcess;
    (void)lpAddress;

    if (lpBuffer == NULL || dwLength < sizeof(MEMORY_BASIC_INFORMATION)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    memset(mbi, 0, sizeof(MEMORY_BASIC_INFORMATION));
    mbi->RegionSize = 4096;
    mbi->State = MEM_COMMIT;
    mbi->Protect = PAGE_READWRITE;
    mbi->Type = MEM_PRIVATE;

    SetLastError(ERROR_SUCCESS);
    return sizeof(MEMORY_BASIC_INFORMATION);
}

LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, uint64_t dwBytes) {
    void* new_mem;

    (void)hHeap;
    (void)dwFlags;

    if (lpMem == NULL) {
        return HeapAlloc(hHeap, dwFlags, dwBytes);
    }

    if (dwBytes == 0) {
        HeapFree(hHeap, dwFlags, lpMem);
        return NULL;
    }

    new_mem = memory_alloc(dwBytes);
    if (new_mem == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    memcpy(new_mem, lpMem, dwBytes);
    memory_free(lpMem);

    SetLastError(ERROR_SUCCESS);
    return new_mem;
}

SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) {
    (void)hHeap;
    (void)dwFlags;
    (void)lpMem;
    SetLastError(ERROR_SUCCESS);
    return 256;
}

BOOL HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem) {
    (void)hHeap;
    (void)dwFlags;
    (void)lpMem;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

uint32_t HeapCompact(HANDLE hHeap, uint32_t dwFlags) {
    (void)hHeap;
    (void)dwFlags;
    SetLastError(ERROR_SUCCESS);
    return 0;
}

BOOL HeapSetInformation(HANDLE hHeap, uint32_t HeapInformationClass,
                         void* HeapInformation, SIZE_T HeapInformationLength) {
    (void)hHeap;
    (void)HeapInformationClass;
    (void)HeapInformation;
    (void)HeapInformationLength;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL HeapQueryInformation(HANDLE hHeap, uint32_t HeapInformationClass,
                           void* HeapInformation, SIZE_T HeapInformationLength,
                           SIZE_T* ReturnLength) {
    (void)hHeap;
    (void)HeapInformationClass;
    (void)HeapInformation;
    (void)HeapInformationLength;

    if (ReturnLength != NULL) {
        *ReturnLength = 0;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL InitializeCriticalSection(void* lpCriticalSection) {
    if (lpCriticalSection == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    memset(lpCriticalSection, 0, 24);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

void EnterCriticalSection(void* lpCriticalSection) {
    (void)lpCriticalSection;
}

BOOL TryEnterCriticalSection(void* lpCriticalSection) {
    (void)lpCriticalSection;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

void LeaveCriticalSection(void* lpCriticalSection) {
    (void)lpCriticalSection;
}

void DeleteCriticalSection(void* lpCriticalSection) {
    if (lpCriticalSection != NULL) {
        memset(lpCriticalSection, 0, 24);
    }
}

BOOL InitializeCriticalSectionAndSpinCount(void* lpCriticalSection, uint32_t dwSpinCount) {
    (void)dwSpinCount;
    return InitializeCriticalSection(lpCriticalSection);
}

void SetCriticalSectionSpinCount(void* lpCriticalSection, uint32_t dwSpinCount) {
    (void)lpCriticalSection;
    (void)dwSpinCount;
}

BOOL InterlockedCompareExchange(volatile LONG* Destination, LONG Exchange, LONG Comparand) {
    LONG original;
    original = *Destination;
    if (*Destination == Comparand) {
        *Destination = Exchange;
    }
    return original;
}

LONG InterlockedIncrement(volatile LONG* Addend) {
    return __sync_add_and_fetch(Addend, 1);
}

LONG InterlockedDecrement(volatile LONG* Addend) {
    return __sync_sub_and_fetch(Addend, 1);
}

LONG InterlockedExchange(volatile LONG* Target, LONG Value) {
    LONG original;
    original = *Target;
    *Target = Value;
    return original;
}

LONG InterlockedExchangeAdd(volatile LONG* Addend, LONG Value) {
    return __sync_add_and_fetch(Addend, Value);
}

PVOID InterlockedCompareExchangePointer(volatile PVOID* Destination,
                                        PVOID Exchange, PVOID Comparand) {
    PVOID original;
    original = *Destination;
    if (*Destination == Comparand) {
        *Destination = Exchange;
    }
    return original;
}

PVOID InterlockedExchangePointer(volatile PVOID* Target, PVOID Value) {
    PVOID original;
    original = *Target;
    *Target = Value;
    return original;
}

void OutputDebugStringA(const char* lpOutputString) {
    (void)lpOutputString;
}

void DebugBreak(void) {
}

BOOL IsDebuggerPresent(void) {
    return FALSE;
}

void FatalExit(int ExitCode) {
    (void)ExitCode;
    while(1) {}
}

void RaiseException(uint32_t dwExceptionCode, uint32_t dwExceptionFlags,
                    uint32_t nNumberOfArguments, const uint32_t* lpArguments) {
    (void)dwExceptionCode;
    (void)dwExceptionFlags;
    (void)nNumberOfArguments;
    (void)lpArguments;
}

uint32_t SetErrorMode(uint32_t uMode) {
    (void)uMode;
    return 0;
}

BOOL Beep(uint32_t dwFreq, uint32_t dwDuration) {
    (void)dwFreq;
    (void)dwDuration;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GenerateConsoleCtrlEvent(uint32_t dwCtrlEvent, uint32_t dwProcessGroupId) {
    (void)dwCtrlEvent;
    (void)dwProcessGroupId;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL AllocConsole(void) {
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL FreeConsole(void) {
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

HWND GetConsoleWindow(void) {
    return NULL;
}

BOOL SetConsoleTitleA(const char* lpConsoleTitle) {
    (void)lpConsoleTitle;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

DWORD GetConsoleTitleA(char* lpConsoleTitle, uint32_t nSize) {
    if (lpConsoleTitle != NULL && nSize > 0) {
        lpConsoleTitle[0] = '\0';
    }
    return 0;
}

BOOL SetConsoleTextAttribute(HANDLE hConsoleOutput, uint16_t wAttributes) {
    (void)hConsoleOutput;
    (void)wAttributes;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput, void* lpConsoleScreenBufferInfo) {
    (void)hConsoleOutput;
    (void)lpConsoleScreenBufferInfo;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

BOOL SetConsoleCursorPosition(HANDLE hConsoleOutput, COORD dwCursorPosition) {
    (void)hConsoleOutput;
    (void)dwCursorPosition;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SetConsoleCursorInfo(HANDLE hConsoleOutput, const void* lpConsoleCursorInfo) {
    (void)hConsoleOutput;
    (void)lpConsoleCursorInfo;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL FillConsoleOutputCharacterA(HANDLE hConsoleOutput, CHAR cCharacter,
                                 DWORD nLength, COORD dwWriteCoord,
                                 DWORD* lpNumberOfCharsWritten) {
    (void)hConsoleOutput;
    (void)cCharacter;
    (void)nLength;
    (void)dwWriteCoord;
    (void)lpNumberOfCharsWritten;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL FillConsoleOutputAttribute(HANDLE hConsoleOutput, WORD wAttribute,
                                DWORD nLength, COORD dwWriteCoord,
                                DWORD* lpNumberOfAttrsWritten) {
    (void)hConsoleOutput;
    (void)wAttribute;
    (void)nLength;
    (void)dwWriteCoord;
    (void)lpNumberOfAttrsWritten;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL ScrollConsoleScreenBufferA(HANDLE hConsoleOutput, const SMALL_RECT* lpScrollRectangle,
                               const SMALL_RECT* lpClipRectangle, COORD dwDestinationOrigin,
                               const CHAR_INFO* lpFill) {
    (void)hConsoleOutput;
    (void)lpScrollRectangle;
    (void)lpClipRectangle;
    (void)dwDestinationOrigin;
    (void)lpFill;
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WriteConsoleA(HANDLE hConsoleOutput, const void* lpBuffer,
                   DWORD nNumberOfCharsToWrite, DWORD* lpNumberOfCharsWritten,
                   void* lpReserved) {
    (void)hConsoleOutput;
    (void)lpBuffer;
    (void)lpReserved;

    if (lpNumberOfCharsWritten != NULL) {
        *lpNumberOfCharsWritten = nNumberOfCharsToWrite;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL ReadConsoleA(HANDLE hConsoleInput, void* lpBuffer,
                  DWORD nNumberOfCharsToRead, DWORD* lpNumberOfCharsRead,
                  void* lpReserved) {
    (void)hConsoleInput;
    (void)lpBuffer;
    (void)lpReserved;

    if (lpNumberOfCharsRead != NULL) {
        *lpNumberOfCharsRead = nNumberOfCharsToRead;
    }

    SetLastError(ERROR_SUCCESS);
    return TRUE;
}