#include "win32.h"
#include <arch/memory.h>
#include <arch/xposix.h>
#include <string.h>

#define WIN32_MAX_HANDLES  4096
#define WIN32_MAX_PATH     260

typedef struct {
    int       used;
    int       posix_fd;
    uint32_t  type;
    uint32_t  access;
    uint32_t  flags;
    uint32_t  attributes;
} win32_handle_t;

#define WIN32_HANDLE_FILE      1
#define WIN32_HANDLE_PROCESS   2
#define WIN32_HANDLE_THREAD    3
#define WIN32_HANDLE_MUTEX     4
#define WIN32_HANDLE_EVENT     5
#define WIN32_HANDLE_SEMAPHORE 6
#define WIN32_HANDLE_FIND      7

static win32_handle_t win32_handles[WIN32_MAX_HANDLES];
static DWORD win32_last_error = 0;

void win32_init(void)
{
    memset(win32_handles, 0, sizeof(win32_handles));
    win32_handles[0].used = 1;
    win32_handles[0].posix_fd = 0;
    win32_handles[0].type = WIN32_HANDLE_FILE;
    win32_handles[1].used = 1;
    win32_handles[1].posix_fd = 1;
    win32_handles[1].type = WIN32_HANDLE_FILE;
    win32_handles[2].used = 1;
    win32_handles[2].posix_fd = 2;
    win32_handles[2].type = WIN32_HANDLE_FILE;
    win32_last_error = 0;
}

static HANDLE win32_alloc_handle(void)
{
    for (int i = 0; i < WIN32_MAX_HANDLES; i++) {
        if (!win32_handles[i].used) {
            win32_handles[i].used = 1;
            return (HANDLE)i;
        }
    }
    return (HANDLE)-1;
}

static win32_handle_t* win32_get_handle(HANDLE h)
{
    if ((uint32_t)h >= WIN32_MAX_HANDLES) return NULL;
    if (!win32_handles[(uint32_t)h].used) return NULL;
    return &win32_handles[(uint32_t)h];
}

static DWORD win32_creation_to_posix_flags(DWORD creation, DWORD access)
{
    int flags = 0;
    if (access & GENERIC_READ) flags |= 0; /* O_RDONLY */
    if (access & GENERIC_WRITE) flags |= 1; /* O_WRONLY */
    if ((access & GENERIC_READ) && (access & GENERIC_WRITE)) flags |= 2; /* O_RDWR */
    switch (creation) {
    case CREATE_NEW: flags |= 0x100; break;
    case CREATE_ALWAYS: flags |= 0x200; break;
    case OPEN_EXISTING: break;
    case OPEN_ALWAYS: flags |= 0x100; break;
    case TRUNCATE_EXISTING: flags |= 0x400; break;
    }
    return (DWORD)flags;
}

HANDLE win32_CreateFile(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                        DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes)
{
    if (!lpFileName) {
        win32_last_error = 0x57;
        return (HANDLE)-1;
    }
    (void)dwShareMode;

    int posix_flags = (int)win32_creation_to_posix_flags(dwCreationDisposition, dwDesiredAccess);
    int mode = 0644;
    if (dwFlagsAndAttributes & FILE_ATTRIBUTE_READONLY) mode = 0444;

    long fd = xposix_sys_open(lpFileName, posix_flags, mode);
    if (fd < 0) {
        win32_last_error = 0x02;
        return (HANDLE)-1;
    }

    HANDLE h = win32_alloc_handle();
    if (h == (HANDLE)-1) {
        xposix_sys_close((int)fd);
        win32_last_error = 0x18;
        return (HANDLE)-1;
    }

    win32_handle_t* wh = win32_get_handle(h);
    wh->posix_fd = (int)fd;
    wh->type = WIN32_HANDLE_FILE;
    wh->access = dwDesiredAccess;
    wh->flags = dwFlagsAndAttributes;
    wh->attributes = dwFlagsAndAttributes & 0xFFFF;
    return h;
}

BOOL win32_CloseHandle(HANDLE hObject)
{
    win32_handle_t* wh = win32_get_handle(hObject);
    if (!wh) {
        win32_last_error = 0x06;
        return 0;
    }

    switch (wh->type) {
    case WIN32_HANDLE_FILE:
    case WIN32_HANDLE_FIND:
        xposix_sys_close(wh->posix_fd);
        break;
    default:
        break;
    }

    wh->used = 0;
    return 1;
}

BOOL win32_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                    LPDWORD lpNumberOfBytesRead)
{
    win32_handle_t* wh = win32_get_handle(hFile);
    if (!wh || wh->type != WIN32_HANDLE_FILE) {
        win32_last_error = 0x06;
        return 0;
    }
    if (!lpBuffer) {
        win32_last_error = 0x57;
        return 0;
    }

    long result = xposix_sys_read(wh->posix_fd, lpBuffer, (size_t)nNumberOfBytesToRead);
    if (result < 0) {
        win32_last_error = 0x1E;
        return 0;
    }

    if (lpNumberOfBytesRead) {
        *((DWORD*)lpNumberOfBytesRead) = (DWORD)result;
    }
    return 1;
}

BOOL win32_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                     LPDWORD lpNumberOfBytesWritten)
{
    win32_handle_t* wh = win32_get_handle(hFile);
    if (!wh || wh->type != WIN32_HANDLE_FILE) {
        win32_last_error = 0x06;
        return 0;
    }
    if (!lpBuffer) {
        win32_last_error = 0x57;
        return 0;
    }

    long result = xposix_sys_write(wh->posix_fd, lpBuffer, (size_t)nNumberOfBytesToWrite);
    if (result < 0) {
        win32_last_error = 0x1E;
        return 0;
    }

    if (lpNumberOfBytesWritten) {
        *((DWORD*)lpNumberOfBytesWritten) = (DWORD)result;
    }
    return 1;
}

DWORD win32_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
    win32_handle_t* wh = win32_get_handle(hFile);
    if (!wh || wh->type != WIN32_HANDLE_FILE) {
        win32_last_error = 0x06;
        return (DWORD)-1;
    }

    long size = xposix_sys_lseek(wh->posix_fd, 0, 2);
    if (size < 0) {
        win32_last_error = 0x1E;
        return (DWORD)-1;
    }

    if (lpFileSizeHigh) *((DWORD*)lpFileSizeHigh) = 0;
    return (DWORD)size;
}

BOOL win32_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh,
                          DWORD dwMoveMethod)
{
    win32_handle_t* wh = win32_get_handle(hFile);
    if (!wh || wh->type != WIN32_HANDLE_FILE) {
        win32_last_error = 0x06;
        return 0;
    }

    int whence;
    switch (dwMoveMethod) {
    case 0: whence = 0; break;
    case 1: whence = 1; break;
    case 2: whence = 2; break;
    default: win32_last_error = 0x57; return 0;
    }

    long result = xposix_sys_lseek(wh->posix_fd, lDistanceToMove, whence);
    if (result < 0) {
        win32_last_error = 0x1E;
        return 0;
    }

    if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = 0;
    return 1;
}

LPVOID win32_VirtualAlloc(LPVOID lpAddress, uint64_t dwSize, DWORD flAllocationType,
                          DWORD flProtect)
{
    (void)lpAddress; (void)flProtect;

    if (dwSize == 0) {
        win32_last_error = 0x57;
        return NULL;
    }

    if (!(flAllocationType & MEM_COMMIT) && !(flAllocationType & MEM_RESERVE)) {
        win32_last_error = 0x57;
        return NULL;
    }

    void* ptr = memory_alloc((uint64_t)dwSize);
    if (!ptr) {
        win32_last_error = 0x0E;
        return NULL;
    }

    if (flAllocationType & MEM_COMMIT) {
        memset(ptr, 0, (size_t)dwSize);
    }

    return ptr;
}

BOOL win32_VirtualFree(LPVOID lpAddress, uint64_t dwSize, DWORD dwFreeType)
{
    if (!lpAddress) {
        win32_last_error = 0x57;
        return 0;
    }

    if (dwFreeType == MEM_RELEASE) {
        memory_free(lpAddress);
        return 1;
    }

    if (dwFreeType == MEM_DECOMMIT) {
        (void)dwSize;
        return 1;
    }

    win32_last_error = 0x57;
    return 0;
}

BOOL win32_VirtualProtect(LPVOID lpAddress, uint64_t dwSize, DWORD flNewProtect,
                          LPDWORD lpflOldProtect)
{
    (void)lpAddress; (void)dwSize; (void)flNewProtect;
    if (lpflOldProtect) *((DWORD*)lpflOldProtect) = PAGE_READWRITE;
    return 1;
}

HANDLE win32_CreateProcess(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                           DWORD dwCreationFlags)
{
    (void)lpCommandLine; (void)dwCreationFlags;

    if (!lpApplicationName) {
        win32_last_error = 0x57;
        return (HANDLE)-1;
    }

    long pid = xposix_sys_fork();
    if (pid < 0) {
        win32_last_error = 0x0E;
        return (HANDLE)-1;
    }

    if (pid == 0) {
        xposix_sys_execve(lpApplicationName, NULL, NULL);
        xposix_sys_exit(1);
    }

    HANDLE h = win32_alloc_handle();
    if (h == (HANDLE)-1) {
        win32_last_error = 0x18;
        return (HANDLE)-1;
    }

    win32_handle_t* wh = win32_get_handle(h);
    wh->posix_fd = (int)pid;
    wh->type = WIN32_HANDLE_PROCESS;
    return h;
}

BOOL win32_TerminateProcess(HANDLE hProcess, DWORD uExitCode)
{
    win32_handle_t* wh = win32_get_handle(hProcess);
    if (!wh || wh->type != WIN32_HANDLE_PROCESS) {
        win32_last_error = 0x06;
        return 0;
    }

    xposix_sys_kill(wh->posix_fd, 9);
    (void)uExitCode;
    return 1;
}

DWORD win32_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    win32_handle_t* wh = win32_get_handle(hHandle);
    if (!wh) {
        win32_last_error = 0x06;
        return WAIT_FAILED;
    }

    if (wh->type == WIN32_HANDLE_PROCESS) {
        int status = 0;
        xposix_sys_waitpid(wh->posix_fd, &status, 0);
        return WAIT_OBJECT_0;
    }

    (void)dwMilliseconds;
    return WAIT_OBJECT_0;
}

DWORD win32_GetLastError(void) { return win32_last_error; }
void  win32_SetLastError(DWORD dwErrCode) { win32_last_error = dwErrCode; }

BOOL win32_GetCurrentDirectory(DWORD nBufferLength, LPSTR lpBuffer)
{
    if (!lpBuffer || nBufferLength < 2) {
        win32_last_error = 0x57;
        return 0;
    }
    long result = xposix_sys_getcwd(lpBuffer, (size_t)nBufferLength);
    return (result >= 0) ? 1 : 0;
}

BOOL win32_SetCurrentDirectory(LPCSTR lpPathName)
{
    if (!lpPathName) {
        win32_last_error = 0x57;
        return 0;
    }
    long result = xposix_sys_chdir(lpPathName);
    return (result >= 0) ? 1 : 0;
}

HANDLE win32_FindFirstFile(LPCSTR lpFileName, WIN32_FIND_DATA* lpFindFileData)
{
    (void)lpFileName; (void)lpFindFileData;
    win32_last_error = 0x02;
    return (HANDLE)-1;
}

BOOL win32_FindNextFile(HANDLE hFindFile, WIN32_FIND_DATA* lpFindFileData)
{
    (void)hFindFile; (void)lpFindFileData;
    win32_last_error = 0x12;
    return 0;
}

BOOL win32_FindClose(HANDLE hFindFile)
{
    return win32_CloseHandle(hFindFile);
}

BOOL win32_CreateDirectory(LPCSTR lpPathName)
{
    if (!lpPathName) return 0;
    long result = xposix_sys_mkdir(lpPathName, 0755);
    return (result >= 0) ? 1 : 0;
}

BOOL win32_RemoveDirectory(LPCSTR lpPathName)
{
    if (!lpPathName) return 0;
    long result = xposix_sys_rmdir(lpPathName);
    return (result >= 0) ? 1 : 0;
}

BOOL win32_DeleteFile(LPCSTR lpFileName)
{
    if (!lpFileName) return 0;
    long result = xposix_sys_unlink(lpFileName);
    return (result >= 0) ? 1 : 0;
}

BOOL win32_MoveFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
    if (!lpExistingFileName || !lpNewFileName) return 0;
    long result = xposix_sys_rename(lpExistingFileName, lpNewFileName);
    return (result >= 0) ? 1 : 0;
}

BOOL win32_CopyFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
{
    (void)bFailIfExists;
    if (!lpExistingFileName || !lpNewFileName) return 0;

    long src_fd = xposix_sys_open(lpExistingFileName, 0, 0);
    if (src_fd < 0) return 0;

    long dst_fd = xposix_sys_open(lpNewFileName, 0x41 | 0x100, 0644);
    if (dst_fd < 0) {
        xposix_sys_close((int)src_fd);
        return 0;
    }

    char buf[4096];
    long n;
    while ((n = xposix_sys_read((int)src_fd, buf, sizeof(buf))) > 0) {
        xposix_sys_write((int)dst_fd, buf, (size_t)n);
    }

    xposix_sys_close((int)src_fd);
    xposix_sys_close((int)dst_fd);
    return 1;
}

DWORD win32_GetTickCount(void)
{
    return (DWORD)xposix_sys_gettimeofday(NULL, NULL);
}

void win32_Sleep(DWORD dwMilliseconds)
{
    xposix_sys_nanosleep((long)dwMilliseconds * 1000000L);
}

BOOL win32_GetSystemTime(SYSTEMTIME* lpSystemTime)
{
    if (!lpSystemTime) return 0;
    memset(lpSystemTime, 0, sizeof(SYSTEMTIME));
    long t = xposix_sys_gettimeofday(NULL, NULL);
    uint64_t secs = (uint64_t)t;
    uint64_t days = secs / 86400;
    secs %= 86400;
    lpSystemTime->wHour = (WORD)(secs / 3600);
    lpSystemTime->wMinute = (WORD)((secs % 3600) / 60);
    lpSystemTime->wSecond = (WORD)(secs % 60);
    lpSystemTime->wYear = 1970;
    lpSystemTime->wMonth = 1;
    lpSystemTime->wDay = 1;
    while (days > 0) {
        uint64_t days_in_year = 365;
        if ((lpSystemTime->wYear % 4 == 0 && lpSystemTime->wYear % 100 != 0) ||
            lpSystemTime->wYear % 400 == 0) days_in_year = 366;
        if (days >= days_in_year) {
            days -= days_in_year;
            lpSystemTime->wYear++;
        } else {
            break;
        }
    }
    return 1;
}

BOOL win32_GetLocalTime(SYSTEMTIME* lpLocalTime)
{
    return win32_GetSystemTime(lpLocalTime);
}

HANDLE win32_CreateMutex(BOOL bInitialOwner, LPCSTR lpName)
{
    (void)bInitialOwner; (void)lpName;
    HANDLE h = win32_alloc_handle();
    if (h != (HANDLE)-1) {
        win32_handle_t* wh = win32_get_handle(h);
        wh->type = WIN32_HANDLE_MUTEX;
    }
    return h;
}

HANDLE win32_CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
    (void)bManualReset; (void)bInitialState; (void)lpName;
    HANDLE h = win32_alloc_handle();
    if (h != (HANDLE)-1) {
        win32_handle_t* wh = win32_get_handle(h);
        wh->type = WIN32_HANDLE_EVENT;
    }
    return h;
}

BOOL win32_SetEvent(HANDLE hEvent)
{
    win32_handle_t* wh = win32_get_handle(hEvent);
    if (!wh || wh->type != WIN32_HANDLE_EVENT) return 0;
    wh->flags |= 0x01;
    return 1;
}

BOOL win32_ResetEvent(HANDLE hEvent)
{
    win32_handle_t* wh = win32_get_handle(hEvent);
    if (!wh || wh->type != WIN32_HANDLE_EVENT) return 0;
    wh->flags &= ~0x01;
    return 1;
}

DWORD win32_WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                                    BOOL bWaitAll, DWORD dwMilliseconds)
{
    (void)nCount; (void)lpHandles; (void)bWaitAll; (void)dwMilliseconds;
    return WAIT_OBJECT_0;
}

HANDLE win32_CreateSemaphore(LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
{
    (void)lInitialCount; (void)lMaximumCount; (void)lpName;
    HANDLE h = win32_alloc_handle();
    if (h != (HANDLE)-1) {
        win32_handle_t* wh = win32_get_handle(h);
        wh->type = WIN32_HANDLE_SEMAPHORE;
    }
    return h;
}

BOOL win32_ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount)
{
    (void)hSemaphore; (void)lReleaseCount; (void)lpPreviousCount;
    return 1;
}

HANDLE win32_CreateThread(LPVOID lpStartAddress, LPVOID lpParameter, DWORD* lpThreadId)
{
    (void)lpStartAddress; (void)lpParameter;
    long tid = xposix_sys_clone(0, NULL, NULL, NULL, 0);
    if (tid < 0) return (HANDLE)-1;
    if (lpThreadId) *lpThreadId = (DWORD)tid;
    HANDLE h = win32_alloc_handle();
    if (h != (HANDLE)-1) {
        win32_handle_t* wh = win32_get_handle(h);
        wh->posix_fd = (int)tid;
        wh->type = WIN32_HANDLE_THREAD;
    }
    return h;
}

DWORD win32_GetCurrentProcessId(void) { return (DWORD)xposix_sys_getpid(); }
DWORD win32_GetCurrentThreadId(void) { return (DWORD)xposix_sys_getpid(); }
HANDLE win32_GetCurrentProcess(void) { return (HANDLE)0xFFFFFFFE; }
HANDLE win32_GetCurrentThread(void) { return (HANDLE)0xFFFFFFFD; }

BOOL win32_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
                           HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle,
                           DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
    (void)hSourceProcessHandle; (void)hTargetProcessHandle;
    (void)dwDesiredAccess; (void)bInheritHandle; (void)dwOptions;
    if (!lpTargetHandle) return 0;
    *lpTargetHandle = hSourceHandle;
    return 1;
}

int win32_WideCharToMultiByte(uint16_t* lpWideCharStr, int cchWideChar,
                               LPSTR lpMultiByteStr, int cbMultiByte)
{
    if (!lpWideCharStr || !lpMultiByteStr) return 0;
    int count = 0;
    int max_src = (cchWideChar == -1) ? 0x7FFFFFFF : cchWideChar;
    for (int i = 0; i < max_src && lpWideCharStr[i]; i++) {
        if (count + 1 >= cbMultiByte) break;
        if (lpWideCharStr[i] < 0x80) {
            lpMultiByteStr[count++] = (char)lpWideCharStr[i];
        } else if (lpWideCharStr[i] < 0x800) {
            if (count + 2 >= cbMultiByte) break;
            lpMultiByteStr[count++] = (char)(0xC0 | (lpWideCharStr[i] >> 6));
            lpMultiByteStr[count++] = (char)(0x80 | (lpWideCharStr[i] & 0x3F));
        } else {
            if (count + 3 >= cbMultiByte) break;
            lpMultiByteStr[count++] = (char)(0xE0 | (lpWideCharStr[i] >> 12));
            lpMultiByteStr[count++] = (char)(0x80 | ((lpWideCharStr[i] >> 6) & 0x3F));
            lpMultiByteStr[count++] = (char)(0x80 | (lpWideCharStr[i] & 0x3F));
        }
    }
    lpMultiByteStr[count] = '\0';
    return count;
}

int win32_MultiByteToWideChar(LPCSTR lpMultiByteStr, int cbMultiByte,
                               uint16_t* lpWideCharStr, int cchWideChar)
{
    if (!lpMultiByteStr || !lpWideCharStr) return 0;
    int count = 0;
    int max_src = (cbMultiByte == -1) ? 0x7FFFFFFF : cbMultiByte;
    for (int i = 0; i < max_src && lpMultiByteStr[i]; ) {
        if (count >= cchWideChar) break;
        uint8_t b = (uint8_t)lpMultiByteStr[i];
        if (b < 0x80) {
            lpWideCharStr[count++] = (uint16_t)b;
            i++;
        } else if ((b & 0xE0) == 0xC0) {
            uint16_t ch = ((uint16_t)(b & 0x1F) << 6) | ((uint8_t)lpMultiByteStr[i+1] & 0x3F);
            lpWideCharStr[count++] = ch;
            i += 2;
        } else if ((b & 0xF0) == 0xE0) {
            uint16_t ch = ((uint16_t)(b & 0x0F) << 12) |
                          ((uint16_t)((uint8_t)lpMultiByteStr[i+1] & 0x3F) << 6) |
                          ((uint8_t)lpMultiByteStr[i+2] & 0x3F);
            lpWideCharStr[count++] = ch;
            i += 3;
        } else {
            lpWideCharStr[count++] = (uint16_t)'?';
            i++;
        }
    }
    return count;
}