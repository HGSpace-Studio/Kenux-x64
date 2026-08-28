#ifndef KERNEL_COMPAT_WIN64_H
#define KERNEL_COMPAT_WIN64_H

#include <arch/types.h>

typedef uint64_t win64_handle_t;
typedef uint64_t win64_dword_t;
typedef int64_t  win64_long_t;
typedef uint64_t win64_ulong_t;
typedef int64_t  win64_longlong_t;
typedef uint64_t win64_ulonglong_t;
typedef uint16_t win64_word_t;
typedef int16_t  win64_short_t;
typedef uint8_t  win64_byte_t;
typedef int8_t   win64_char_t;
typedef uint16_t win64_wchar_t;
typedef void*    win64_ptr_t;
typedef uint64_t win64_size_t;
typedef int64_t  win64_ptrdiff_t;
typedef uint64_t win64_lparam_t;
typedef uint64_t win64_wparam_t;
typedef int64_t  win64_bool_t;
typedef uint32_t win64_uint_t;
typedef int32_t  win64_int_t;

#define WIN64_INVALID_HANDLE_VALUE  ((win64_handle_t)-1)
#define WIN64_NULL_HANDLE           ((win64_handle_t)0)

#define WIN64_GENERIC_READ          0x80000000
#define WIN64_GENERIC_WRITE         0x40000000
#define WIN64_GENERIC_EXECUTE       0x20000000
#define WIN64_GENERIC_ALL           0x10000000

#define WIN64_FILE_SHARE_READ       0x00000001
#define WIN64_FILE_SHARE_WRITE      0x00000002
#define WIN64_FILE_SHARE_DELETE     0x00000004

#define WIN64_CREATE_NEW            1
#define WIN64_CREATE_ALWAYS         2
#define WIN64_OPEN_EXISTING         3
#define WIN64_OPEN_ALWAYS           4
#define WIN64_TRUNCATE_EXISTING     5

#define WIN64_FILE_ATTRIBUTE_READONLY   0x00000001
#define WIN64_FILE_ATTRIBUTE_HIDDEN     0x00000002
#define WIN64_FILE_ATTRIBUTE_SYSTEM     0x00000004
#define WIN64_FILE_ATTRIBUTE_DIRECTORY  0x00000010
#define WIN64_FILE_ATTRIBUTE_ARCHIVE    0x00000020
#define WIN64_FILE_ATTRIBUTE_NORMAL     0x00000080
#define WIN64_FILE_ATTRIBUTE_TEMPORARY  0x00000100

#define WIN64_STD_INPUT_HANDLE    ((win64_handle_t)-10)
#define WIN64_STD_OUTPUT_HANDLE   ((win64_handle_t)-11)
#define WIN64_STD_ERROR_HANDLE    ((win64_handle_t)-12)

#define WIN64_PAGE_NOACCESS       0x01
#define WIN64_PAGE_READONLY       0x02
#define WIN64_PAGE_READWRITE      0x04
#define WIN64_PAGE_WRITECOPY      0x08
#define WIN64_PAGE_EXECUTE        0x10
#define WIN64_PAGE_EXECUTE_READ   0x20
#define WIN64_PAGE_EXECUTE_READWRITE 0x40
#define WIN64_PAGE_GUARD          0x100
#define WIN64_PAGE_NOCACHE        0x200
#define WIN64_PAGE_WRITECOMBINE   0x400

#define WIN64_MEM_COMMIT          0x1000
#define WIN64_MEM_RESERVE         0x2000
#define WIN64_MEM_DECOMMIT        0x4000
#define WIN64_MEM_RELEASE         0x8000
#define WIN64_MEM_FREE            0x10000
#define WIN64_MEM_TOP_DOWN        0x100000
#define WIN64_MEM_WRITE_WATCH     0x200000
#define WIN64_MEM_PHYSICAL        0x400000
#define WIN64_MEM_LARGE_PAGES     0x20000000

#define WIN64_PROCESS_ALL_ACCESS      0x001FFFFF
#define WIN64_SYNCHRONIZE             0x00100000
#define WIN64_WAIT_OBJECT_0           0
#define WIN64_WAIT_TIMEOUT            258
#define WIN64_WAIT_FAILED             ((win64_dword_t)-1)
#define WIN64_INFINITE                ((win64_dword_t)-1)

#define WIN64_NTSTATUS_SUCCESS        0x00000000
#define WIN64_NTSTATUS_ERROR          0xC0000001

typedef struct {
    win64_dword_t dwLowDateTime;
    win64_dword_t dwHighDateTime;
} win64_filetime_t;

typedef struct {
    win64_dword_t nLength;
    win64_ptr_t   lpSecurityDescriptor;
    win64_bool_t  bInheritHandle;
} win64_security_attributes_t;

typedef struct {
    win64_dword_t dwOemId;
    win64_word_t  wProcessorArchitecture;
    win64_word_t  wReserved;
    win64_dword_t dwPageSize;
    win64_ptr_t   lpMinimumApplicationAddress;
    win64_ptr_t   lpMaximumApplicationAddress;
    win64_ptr_t   dwActiveProcessorMask;
    win64_dword_t dwNumberOfProcessors;
    win64_dword_t dwProcessorType;
    win64_dword_t dwAllocationGranularity;
    win64_word_t  wProcessorLevel;
    win64_word_t  wProcessorRevision;
} win64_system_info_t;

typedef struct {
    win64_dword_t dwMajorVersion;
    win64_dword_t dwMinorVersion;
    win64_dword_t dwBuildNumber;
    win64_dword_t dwPlatformId;
    win64_wchar_t szCSDVersion[128];
    win64_word_t  wServicePackMajor;
    win64_word_t  wServicePackMinor;
    win64_word_t  wSuiteMask;
    win64_byte_t  wProductType;
} win64_osversioninfoex_t;

typedef struct {
    win64_ulong_t dwFileAttributes;
    win64_filetime_t ftCreationTime;
    win64_filetime_t ftLastAccessTime;
    win64_filetime_t ftLastWriteTime;
    win64_filetime_t ftChangeTime;
    win64_ulonglong_t nFileSize;
    win64_ulonglong_t nAllocationSize;
    win64_dword_t dwFileType;
    win64_dword_t dwDeviceType;
    win64_dword_t dwNumberOfLinks;
    win64_ulonglong_t nFileIndex;
} win64_file_basic_info_t;

typedef struct {
    win64_longlong_t QuadPart;
} win64_large_integer_t;

typedef struct {
    int type;
    win64_handle_t handle;
    int posix_fd;
    int ref_count;
} win64_handle_entry_t;

void           win64_init(void);
win64_handle_t win64_CreateFileW(const win64_wchar_t* lpFileName, win64_dword_t dwDesiredAccess,
                                  win64_dword_t dwShareMode, win64_security_attributes_t* lpSecurityAttributes,
                                  win64_dword_t dwCreationDisposition, win64_dword_t dwFlagsAndAttributes,
                                  win64_handle_t hTemplateFile);
win64_bool_t   win64_WriteFile(win64_handle_t hFile, const void* lpBuffer,
                                win64_dword_t nNumberOfBytesToWrite,
                                win64_dword_t* lpNumberOfBytesWritten, void* lpOverlapped);
win64_bool_t   win64_ReadFile(win64_handle_t hFile, void* lpBuffer,
                               win64_dword_t nNumberOfBytesToRead,
                               win64_dword_t* lpNumberOfBytesRead, void* lpOverlapped);
win64_bool_t   win64_CloseHandle(win64_handle_t hObject);
win64_ptr_t    win64_VirtualAlloc(win64_ptr_t lpAddress, win64_size_t dwSize,
                                   win64_dword_t flAllocationType, win64_dword_t flProtect);
win64_bool_t   win64_VirtualFree(win64_ptr_t lpAddress, win64_size_t dwSize,
                                  win64_dword_t dwFreeType);
win64_ptr_t    win64_VirtualProtect(win64_ptr_t lpAddress, win64_size_t dwSize,
                                     win64_dword_t flNewProtect, win64_dword_t* lpflOldProtect);
win64_handle_t win64_CreateProcessW(const win64_wchar_t* lpApplicationName, win64_wchar_t* lpCommandLine,
                                     win64_security_attributes_t* lpProcessAttributes,
                                     win64_security_attributes_t* lpThreadAttributes,
                                     win64_bool_t bInheritHandles, win64_dword_t dwCreationFlags,
                                     void* lpEnvironment, const win64_wchar_t* lpCurrentDirectory,
                                     void* lpStartupInfo, void* lpProcessInformation);
win64_dword_t  win64_WaitForSingleObject(win64_handle_t hHandle, win64_dword_t dwMilliseconds);
win64_handle_t win64_CreateThread(win64_security_attributes_t* lpThreadAttributes,
                                   win64_size_t dwStackSize, win64_ptr_t lpStartAddress,
                                   win64_ptr_t lpParameter, win64_dword_t dwCreationFlags,
                                   win64_dword_t* lpThreadId);
void           win64_ExitProcess(win64_uint_t uExitCode);
void           win64_ExitThread(win64_dword_t dwExitCode);
win64_dword_t  win64_GetLastError(void);
void           win64_SetLastError(win64_dword_t dwErrCode);
void           win64_GetSystemInfo(win64_system_info_t* lpSystemInfo);
win64_bool_t   win64_GetVersionExW(win64_osversioninfoex_t* lpVersionInformation);
win64_ptr_t    win64_GetProcAddress(win64_handle_t hModule, const char* lpProcName);
win64_handle_t win64_LoadLibraryW(const win64_wchar_t* lpLibFileName);
win64_bool_t   win64_FreeLibrary(win64_handle_t hModule);
win64_handle_t win64_CreateFileMappingW(win64_handle_t hFile, win64_security_attributes_t* lpAttributes,
                                         win64_dword_t flProtect, win64_dword_t dwMaximumSizeHigh,
                                         win64_dword_t dwMaximumSizeLow, const win64_wchar_t* lpName);
win64_ptr_t    win64_MapViewOfFile(win64_handle_t hFileMappingObject, win64_dword_t dwDesiredAccess,
                                    win64_dword_t dwFileOffsetHigh, win64_dword_t dwFileOffsetLow,
                                    win64_size_t dwNumberOfBytesToMap);
win64_bool_t   win64_UnmapViewOfFile(win64_ptr_t lpBaseAddress);
win64_dword_t  win64_Sleep(win64_dword_t dwMilliseconds);

#endif