#ifndef KENUX_WIN32_COMPAT_H
#define KENUX_WIN32_COMPAT_H

#include <arch/types.h>

#define STATUS_SUCCESS                   ((int32_t)0x00000000)
#define STATUS_UNSUCCESSFUL              ((int32_t)0xC0000001)
#define STATUS_NOT_IMPLEMENTED           ((int32_t)0xC0000002)
#define STATUS_INVALID_PARAMETER         ((int32_t)0xC000000D)
#define STATUS_ACCESS_DENIED             ((int32_t)0xC0000022)
#define STATUS_OBJECT_NAME_NOT_FOUND     ((int32_t)0xC0000034)
#define STATUS_OBJECT_NAME_COLLISION     ((int32_t)0xC0000035)
#define STATUS_FILE_NOT_FOUND            ((int32_t)0xC0000034)
#define STATUS_SHARING_VIOLATION         ((int32_t)0xC0000043)
#define STATUS_NO_MEMORY                 ((int32_t)0xC0000017)
#define STATUS_BUFFER_TOO_SMALL          ((int32_t)0xC0000023)
#define STATUS_INVALID_HANDLE            ((int32_t)0xC0000008)
#define STATUS_PENDING                   ((int32_t)0x00000103)

#define GENERIC_READ                     0x80000000
#define GENERIC_WRITE                    0x40000000
#define GENERIC_EXECUTE                  0x20000000
#define GENERIC_ALL                      0x10000000

#define FILE_SHARE_READ                  0x00000001
#define FILE_SHARE_WRITE                 0x00000002
#define FILE_SHARE_DELETE                0x00000004

#define CREATE_NEW                       1
#define CREATE_ALWAYS                    2
#define OPEN_EXISTING                    3
#define OPEN_ALWAYS                      4
#define TRUNCATE_EXISTING                5

#define FILE_ATTRIBUTE_READONLY          0x00000001
#define FILE_ATTRIBUTE_HIDDEN            0x00000002
#define FILE_ATTRIBUTE_SYSTEM            0x00000004
#define FILE_ATTRIBUTE_DIRECTORY         0x00000010
#define FILE_ATTRIBUTE_ARCHIVE           0x00000020
#define FILE_ATTRIBUTE_NORMAL            0x00000080
#define FILE_ATTRIBUTE_TEMPORARY         0x00000100
#define FILE_ATTRIBUTE_COMPRESSED        0x00000800

#define FILE_FLAG_OVERLAPPED             0x40000000
#define FILE_FLAG_WRITE_THROUGH          0x80000000

#define STD_INPUT_HANDLE                 ((uint32_t)-10)
#define STD_OUTPUT_HANDLE                ((uint32_t)-11)
#define STD_ERROR_HANDLE                 ((uint32_t)-12)

#define WAIT_OBJECT_0                    0
#define WAIT_TIMEOUT                     258
#define WAIT_FAILED                      ((uint32_t)-1)
#define INFINITE                         ((uint32_t)-1)

#define PAGE_NOACCESS                    0x01
#define PAGE_READONLY                    0x02
#define PAGE_READWRITE                   0x04
#define PAGE_WRITECOPY                   0x08
#define PAGE_EXECUTE                     0x10
#define PAGE_EXECUTE_READ                0x20
#define PAGE_EXECUTE_READWRITE           0x40
#define PAGE_GUARD                       0x100
#define PAGE_NOCACHE                     0x200
#define PAGE_WRITECOMBINE                0x400

#define MEM_COMMIT                       0x1000
#define MEM_RESERVE                      0x2000
#define MEM_DECOMMIT                     0x4000
#define MEM_RELEASE                      0x8000
#define MEM_FREE                         0x10000
#define MEM_PRIVATE                      0x20000
#define MEM_MAPPED                       0x40000
#define MEM_TOP_DOWN                     0x100000

#define PROCESS_ALL_ACCESS               0x001FFFFF
#define PROCESS_TERMINATE                0x0001
#define PROCESS_CREATE_THREAD            0x0002
#define PROCESS_VM_OPERATION             0x0008
#define PROCESS_VM_READ                  0x0010
#define PROCESS_VM_WRITE                 0x0020
#define PROCESS_QUERY_INFORMATION        0x0400

#define THREAD_ALL_ACCESS                0x001FFFFF
#define THREAD_TERMINATE                 0x0001
#define THREAD_SUSPEND_RESUME            0x0002
#define THREAD_GET_CONTEXT               0x0008
#define THREAD_SET_CONTEXT               0x0010
#define THREAD_QUERY_INFORMATION         0x0040

typedef int32_t NTSTATUS;
typedef uint32_t HANDLE;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int32_t  BOOL;
typedef void*    LPVOID;
typedef const void* LPCVOID;
typedef char*   LPSTR;
typedef const char* LPCSTR;
typedef uint32_t LPDWORD;
typedef int32_t  LONG;
typedef uint32_t ULONG;
typedef int64_t  LONGLONG;
typedef uint64_t ULONGLONG;

typedef struct {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

typedef struct {
    WORD  wYear;
    WORD  wMonth;
    WORD  wDayOfWeek;
    WORD  wDay;
    WORD  wHour;
    WORD  wMinute;
    WORD  wSecond;
    WORD  wMilliseconds;
} SYSTEMTIME;

typedef struct {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL  bInheritHandle;
} SECURITY_ATTRIBUTES;

typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    char  cFileName[260];
    char  cAlternateFileName[14];
} WIN32_FIND_DATA;

typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION;

typedef struct {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD  wShowWindow;
    WORD  cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUP_INFO;

typedef struct {
    HANDLE hObject;
} OBJECT_ATTRIBUTES_COMPAT;

void  win32_init(void);
HANDLE win32_CreateFile(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                        DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes);
BOOL   win32_CloseHandle(HANDLE hObject);
BOOL   win32_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                      LPDWORD lpNumberOfBytesRead);
BOOL   win32_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                       LPDWORD lpNumberOfBytesWritten);
DWORD  win32_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL   win32_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh,
                            DWORD dwMoveMethod);
LPVOID win32_VirtualAlloc(LPVOID lpAddress, uint64_t dwSize, DWORD flAllocationType,
                          DWORD flProtect);
BOOL   win32_VirtualFree(LPVOID lpAddress, uint64_t dwSize, DWORD dwFreeType);
BOOL   win32_VirtualProtect(LPVOID lpAddress, uint64_t dwSize, DWORD flNewProtect,
                            LPDWORD lpflOldProtect);
HANDLE win32_CreateProcess(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                           DWORD dwCreationFlags);
BOOL   win32_TerminateProcess(HANDLE hProcess, DWORD uExitCode);
DWORD  win32_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD  win32_GetLastError(void);
void   win32_SetLastError(DWORD dwErrCode);
BOOL   win32_GetCurrentDirectory(DWORD nBufferLength, LPSTR lpBuffer);
BOOL   win32_SetCurrentDirectory(LPCSTR lpPathName);
HANDLE win32_FindFirstFile(LPCSTR lpFileName, WIN32_FIND_DATA* lpFindFileData);
BOOL   win32_FindNextFile(HANDLE hFindFile, WIN32_FIND_DATA* lpFindFileData);
BOOL   win32_FindClose(HANDLE hFindFile);
BOOL   win32_CreateDirectory(LPCSTR lpPathName);
BOOL   win32_RemoveDirectory(LPCSTR lpPathName);
BOOL   win32_DeleteFile(LPCSTR lpFileName);
BOOL   win32_MoveFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
BOOL   win32_CopyFile(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
DWORD  win32_GetTickCount(void);
void   win32_Sleep(DWORD dwMilliseconds);
BOOL   win32_GetSystemTime(SYSTEMTIME* lpSystemTime);
BOOL   win32_GetLocalTime(SYSTEMTIME* lpLocalTime);
HANDLE win32_CreateMutex(BOOL bInitialOwner, LPCSTR lpName);
HANDLE win32_CreateEvent(BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
BOOL   win32_SetEvent(HANDLE hEvent);
BOOL   win32_ResetEvent(HANDLE hEvent);
DWORD  win32_WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                                    BOOL bWaitAll, DWORD dwMilliseconds);
HANDLE win32_CreateSemaphore(LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
BOOL   win32_ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount);
HANDLE win32_CreateThread(LPVOID lpStartAddress, LPVOID lpParameter, DWORD* lpThreadId);
DWORD  win32_GetCurrentProcessId(void);
DWORD  win32_GetCurrentThreadId(void);
HANDLE win32_GetCurrentProcess(void);
HANDLE win32_GetCurrentThread(void);
BOOL   win32_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
                             HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle,
                             DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
int    win32_WideCharToMultiByte(uint16_t* lpWideCharStr, int cchWideChar,
                                 LPSTR lpMultiByteStr, int cbMultiByte);
int    win32_MultiByteToWideChar(LPCSTR lpMultiByteStr, int cbMultiByte,
                                 uint16_t* lpWideCharStr, int cchWideChar);

#endif