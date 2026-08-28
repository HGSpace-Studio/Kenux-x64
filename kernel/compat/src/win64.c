#include "win64.h"
#include <arch/memory.h>
#include <string.h>

#define WIN64_MAX_HANDLES 4096

static win64_handle_entry_t win64_handles[WIN64_MAX_HANDLES];
static win64_dword_t win64_last_error = 0;
static int win64_initialized = 0;

void win64_init(void)
{
    if (win64_initialized) return;
    memset(win64_handles, 0, sizeof(win64_handles));
    win64_last_error = 0;
    win64_initialized = 1;
}

static win64_handle_t win64_alloc_handle(int type, int posix_fd)
{
    for (int i = 0; i < WIN64_MAX_HANDLES; i++) {
        if (win64_handles[i].type == 0) {
            win64_handles[i].type = type;
            win64_handles[i].handle = (win64_handle_t)i;
            win64_handles[i].posix_fd = posix_fd;
            win64_handles[i].ref_count = 1;
            return (win64_handle_t)i;
        }
    }
    return WIN64_INVALID_HANDLE_VALUE;
}

static win64_handle_entry_t* win64_get_handle(win64_handle_t h)
{
    if (h == WIN64_INVALID_HANDLE_VALUE || h >= WIN64_MAX_HANDLES) return NULL;
    if (win64_handles[h].type == 0) return NULL;
    return &win64_handles[h];
}

static int win64_creation_to_posix(win64_dword_t creation, win64_dword_t access)
{
    int flags = 0;
    if (access & WIN64_GENERIC_READ) flags |= 0; 
    if (access & WIN64_GENERIC_WRITE) flags |= 1; 
    if (creation == WIN64_CREATE_NEW) flags |= 0x40 | 0x100;
    else if (creation == WIN64_CREATE_ALWAYS) flags |= 0x40 | 0x200;
    else if (creation == WIN64_OPEN_EXISTING) flags |= 0;
    else if (creation == WIN64_OPEN_ALWAYS) flags |= 0x40;
    else if (creation == WIN64_TRUNCATE_EXISTING) flags |= 0x200;
    return flags;
}

win64_handle_t win64_CreateFileW(const win64_wchar_t* lpFileName, win64_dword_t dwDesiredAccess,
                                  win64_dword_t dwShareMode, win64_security_attributes_t* lpSecurityAttributes,
                                  win64_dword_t dwCreationDisposition, win64_dword_t dwFlagsAndAttributes,
                                  win64_handle_t hTemplateFile)
{
    (void)dwShareMode; (void)lpSecurityAttributes; (void)dwFlagsAndAttributes; (void)hTemplateFile;
    if (!lpFileName) { win64_last_error = 0x03; return WIN64_INVALID_HANDLE_VALUE; }

    char path[512];
    int i = 0;
    while (lpFileName[i] && i < 511) { path[i] = (char)lpFileName[i]; i++; }
    path[i] = '\0';

    int posix_flags = win64_creation_to_posix(dwCreationDisposition, dwDesiredAccess);
    long fd = xposix_sys_open(path, posix_flags, 0644);
    if (fd < 0) { win64_last_error = 0x02; return WIN64_INVALID_HANDLE_VALUE; }

    return win64_alloc_handle(1, (int)fd);
}

win64_bool_t win64_WriteFile(win64_handle_t hFile, const void* lpBuffer,
                              win64_dword_t nNumberOfBytesToWrite,
                              win64_dword_t* lpNumberOfBytesWritten, void* lpOverlapped)
{
    (void)lpOverlapped;
    win64_handle_entry_t* h = win64_get_handle(hFile);
    if (!h || !lpBuffer) { win64_last_error = 0x06; return 0; }

    long written = xposix_sys_write(h->posix_fd, lpBuffer, nNumberOfBytesToWrite);
    if (written < 0) { win64_last_error = 0x1D; return 0; }
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (win64_dword_t)written;
    return 1;
}

win64_bool_t win64_ReadFile(win64_handle_t hFile, void* lpBuffer,
                             win64_dword_t nNumberOfBytesToRead,
                             win64_dword_t* lpNumberOfBytesRead, void* lpOverlapped)
{
    (void)lpOverlapped;
    win64_handle_entry_t* h = win64_get_handle(hFile);
    if (!h || !lpBuffer) { win64_last_error = 0x06; return 0; }

    long read = xposix_sys_read(h->posix_fd, lpBuffer, nNumberOfBytesToRead);
    if (read < 0) { win64_last_error = 0x1D; return 0; }
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (win64_dword_t)read;
    return 1;
}

win64_bool_t win64_CloseHandle(win64_handle_t hObject)
{
    win64_handle_entry_t* h = win64_get_handle(hObject);
    if (!h) { win64_last_error = 0x06; return 0; }

    h->ref_count--;
    if (h->ref_count <= 0) {
        if (h->posix_fd >= 0) xposix_sys_close(h->posix_fd);
        h->type = 0;
        h->handle = 0;
        h->posix_fd = -1;
    }
    return 1;
}

win64_ptr_t win64_VirtualAlloc(win64_ptr_t lpAddress, win64_size_t dwSize,
                                win64_dword_t flAllocationType, win64_dword_t flProtect)
{
    (void)flProtect;
    if (dwSize == 0) { win64_last_error = 0x57; return NULL; }

    if (flAllocationType & WIN64_MEM_COMMIT) {
        void* addr;
        if (lpAddress) {
            addr = lpAddress;
        } else {
            addr = memory_alloc((uint32_t)dwSize);
        }
        if (!addr) { win64_last_error = 0x08; return NULL; }
        return (win64_ptr_t)addr;
    }
    return NULL;
}

win64_bool_t win64_VirtualFree(win64_ptr_t lpAddress, win64_size_t dwSize,
                                win64_dword_t dwFreeType)
{
    if (!lpAddress) { win64_last_error = 0x57; return 0; }
    if (dwFreeType & WIN64_MEM_RELEASE) {
        memory_free(lpAddress);
        return 1;
    }
    return 1;
}

win64_ptr_t win64_VirtualProtect(win64_ptr_t lpAddress, win64_size_t dwSize,
                                  win64_dword_t flNewProtect, win64_dword_t* lpflOldProtect)
{
    (void)lpAddress; (void)dwSize; (void)flNewProtect;
    if (lpflOldProtect) *lpflOldProtect = WIN64_PAGE_READWRITE;
    return (win64_ptr_t)1;
}

win64_handle_t win64_CreateProcessW(const win64_wchar_t* lpApplicationName, win64_wchar_t* lpCommandLine,
                                     win64_security_attributes_t* lpProcessAttributes,
                                     win64_security_attributes_t* lpThreadAttributes,
                                     win64_bool_t bInheritHandles, win64_dword_t dwCreationFlags,
                                     void* lpEnvironment, const win64_wchar_t* lpCurrentDirectory,
                                     void* lpStartupInfo, void* lpProcessInformation)
{
    (void)lpCommandLine; (void)lpProcessAttributes; (void)lpThreadAttributes;
    (void)bInheritHandles; (void)dwCreationFlags; (void)lpEnvironment;
    (void)lpCurrentDirectory; (void)lpStartupInfo; (void)lpProcessInformation;

    if (!lpApplicationName) { win64_last_error = 0x02; return WIN64_INVALID_HANDLE_VALUE; }

    char path[512];
    int i = 0;
    while (lpApplicationName[i] && i < 511) { path[i] = (char)lpApplicationName[i]; i++; }
    path[i] = '\0';

    long pid = xposix_sys_fork();
    if (pid == 0) {
        xposix_sys_execve(path, NULL, NULL);
        xposix_sys_exit(1);
    }
    if (pid < 0) { win64_last_error = 0x08; return WIN64_INVALID_HANDLE_VALUE; }

    return win64_alloc_handle(2, (int)pid);
}

win64_dword_t win64_WaitForSingleObject(win64_handle_t hHandle, win64_dword_t dwMilliseconds)
{
    win64_handle_entry_t* h = win64_get_handle(hHandle);
    if (!h) return WIN64_WAIT_FAILED;

    if (h->type == 2) {
        int status;
        xposix_sys_waitpid(h->posix_fd, &status, 0);
        return WIN64_WAIT_OBJECT_0;
    }

    (void)dwMilliseconds;
    return WIN64_WAIT_OBJECT_0;
}

win64_handle_t win64_CreateThread(win64_security_attributes_t* lpThreadAttributes,
                                   win64_size_t dwStackSize, win64_ptr_t lpStartAddress,
                                   win64_ptr_t lpParameter, win64_dword_t dwCreationFlags,
                                   win64_dword_t* lpThreadId)
{
    (void)lpThreadAttributes; (void)dwStackSize; (void)lpParameter; (void)dwCreationFlags;

    long tid = xposix_sys_clone(0x00000100, NULL, NULL, NULL, 0);
    if (tid < 0) { win64_last_error = 0x08; return WIN64_INVALID_HANDLE_VALUE; }

    if (tid == 0) {
        void (*thread_fn)(void*) = (void(*)(void*))lpStartAddress;
        thread_fn(NULL);
        xposix_sys_exit(0);
    }

    if (lpThreadId) *lpThreadId = (win64_dword_t)tid;
    return win64_alloc_handle(3, (int)tid);
}

void win64_ExitProcess(win64_uint_t uExitCode)
{
    xposix_sys_exit((int)uExitCode);
}

void win64_ExitThread(win64_dword_t dwExitCode)
{
    xposix_sys_exit((int)dwExitCode);
}

win64_dword_t win64_GetLastError(void)
{
    return win64_last_error;
}

void win64_SetLastError(win64_dword_t dwErrCode)
{
    win64_last_error = dwErrCode;
}

void win64_GetSystemInfo(win64_system_info_t* lpSystemInfo)
{
    if (!lpSystemInfo) return;
    memset(lpSystemInfo, 0, sizeof(win64_system_info_t));
    lpSystemInfo->wProcessorArchitecture = 9; 
    lpSystemInfo->dwPageSize = 4096;
    lpSystemInfo->lpMinimumApplicationAddress = (win64_ptr_t)0x10000;
    lpSystemInfo->lpMaximumApplicationAddress = (win64_ptr_t)0x7FFFFFFFFFFF;
    lpSystemInfo->dwActiveProcessorMask = 1;
    lpSystemInfo->dwNumberOfProcessors = 1;
    lpSystemInfo->dwAllocationGranularity = 65536;
}

win64_bool_t win64_GetVersionExW(win64_osversioninfoex_t* lpVersionInformation)
{
    if (!lpVersionInformation) return 0;
    lpVersionInformation->dwMajorVersion = 10;
    lpVersionInformation->dwMinorVersion = 0;
    lpVersionInformation->dwBuildNumber = 19041;
    lpVersionInformation->dwPlatformId = 2;
    lpVersionInformation->wServicePackMajor = 0;
    lpVersionInformation->wServicePackMinor = 0;
    lpVersionInformation->wSuiteMask = 0;
    lpVersionInformation->wProductType = 1;
    return 1;
}

win64_ptr_t win64_GetProcAddress(win64_handle_t hModule, const char* lpProcName)
{
    (void)hModule; (void)lpProcName;
    return NULL;
}

win64_handle_t win64_LoadLibraryW(const win64_wchar_t* lpLibFileName)
{
    (void)lpLibFileName;
    return WIN64_INVALID_HANDLE_VALUE;
}

win64_bool_t win64_FreeLibrary(win64_handle_t hModule)
{
    (void)hModule;
    return 1;
}

win64_handle_t win64_CreateFileMappingW(win64_handle_t hFile, win64_security_attributes_t* lpAttributes,
                                         win64_dword_t flProtect, win64_dword_t dwMaximumSizeHigh,
                                         win64_dword_t dwMaximumSizeLow, const win64_wchar_t* lpName)
{
    (void)hFile; (void)lpAttributes; (void)flProtect;
    (void)dwMaximumSizeHigh; (void)dwMaximumSizeLow; (void)lpName;
    return WIN64_INVALID_HANDLE_VALUE;
}

win64_ptr_t win64_MapViewOfFile(win64_handle_t hFileMappingObject, win64_dword_t dwDesiredAccess,
                                 win64_dword_t dwFileOffsetHigh, win64_dword_t dwFileOffsetLow,
                                 win64_size_t dwNumberOfBytesToMap)
{
    (void)hFileMappingObject; (void)dwDesiredAccess;
    (void)dwFileOffsetHigh; (void)dwFileOffsetLow; (void)dwNumberOfBytesToMap;
    return NULL;
}

win64_bool_t win64_UnmapViewOfFile(win64_ptr_t lpBaseAddress)
{
    (void)lpBaseAddress;
    return 1;
}

win64_dword_t win64_Sleep(win64_dword_t dwMilliseconds)
{
    struct timespec ts = { .tv_sec = (long)(dwMilliseconds / 1000),
                           .tv_nsec = (long)((dwMilliseconds % 1000) * 1000000) };
    xposix_sys_nanosleep(&ts, NULL);
    return 0;
}