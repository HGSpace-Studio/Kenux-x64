#ifndef ARCH_X86_64_WIN32_PROCESS_H
#define ARCH_X86_64_WIN32_PROCESS_H

#include <arch/win32.h>

#define WIN32_MAX_THREADS_PER_PROCESS 256

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY* Flink;
    struct _LIST_ENTRY* Blink;
} LIST_ENTRY, *PLIST_ENTRY;

typedef struct _PEB_LDR_DATA {
    uint32_t    Length;
    BOOL        Initialized;
    HANDLE      SsHandle;
    LIST_ENTRY  InLoadOrderModuleList;
    LIST_ENTRY  InMemoryOrderModuleList;
    LIST_ENTRY  InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
    uint32_t    MaximumLength;
    uint32_t    Length;
    uint32_t    Flags;
    uint32_t    DebugFlags;
    HANDLE      ConsoleHandle;
    uint32_t    ConsoleFlags;
    HANDLE      StandardInput;
    HANDLE      StandardOutput;
    HANDLE      StandardError;
    UNICODE_STRING CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    uint32_t    EnvironmentSize;
    void*       Environment;
    uint32_t    StartingX;
    uint32_t    StartingY;
    uint32_t    CountX;
    uint32_t    CountY;
    uint32_t    CountCharsX;
    uint32_t    CountCharsY;
    uint32_t    FillAttribute;
    uint32_t    WindowFlags;
    uint32_t    ShowWindowFlags;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef struct _PEB {
    BOOL                    InheritedAddressSpace;
    BOOL                    ReadImageFileExecOptions;
    BOOL                    BeingDebugged;
    BOOL                    BitField;
    uint8_t                 SpareBool[1];
    HANDLE                  Mutant;
    uint64_t                ImageBaseAddress;
    PPEB_LDR_DATA           LoaderData;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    uint32_t                SubSystemData;
    uint32_t                ProcessHeap;
    uint64_t                FastPebLock;
    uint32_t                AtlThunkSListPtr;
    uint32_t                IFEOKey;
    uint32_t                CrossProcessFlags;
    uint32_t                KernelCallbackTable;
    uint32_t                SystemReserved[1];
    uint32_t                AtlThunkSListPtr32;
    uint32_t                ApiSetMap;
    uint32_t                TlsExpansionCounter;
    uint64_t                TlsBitmap;
    uint32_t                TlsBitmapBits[2];
    uint32_t                ReadOnlySharedMemoryBase;
    uint32_t                HotpatchInformation;
    uint32_t                ReadOnlyStaticServerData;
    uint32_t                AnsiCodePageData;
    uint32_t                OemCodePageData;
    uint32_t                UnicodeCaseTableData;
    uint32_t                NumberOfProcessors;
    uint32_t                NtGlobalFlag;
    uint64_t                CriticalSectionTimeout;
    uint64_t                HeapSegmentReserve;
    uint64_t                HeapSegmentCommit;
    uint64_t                HeapDeCommitTotalFreeThreshold;
    uint64_t                HeapDeCommitFreeBlockThreshold;
    uint32_t                NumberOfHeaps;
    uint32_t                MaximumNumberOfHeaps;
    uint32_t                ProcessHeaps;
    uint32_t                GdiSharedHandleTable;
    uint32_t                ProcessStarterHelper;
    uint32_t                GdiDCAttributeList;
    uint32_t                LoaderLock;
    uint32_t                OSMajorVersion;
    uint32_t                OSMinorVersion;
    uint16_t                OSBuildNumber;
    uint16_t                OSCSDVersion;
    uint32_t                OSPlatformId;
    uint32_t                ImageSubsystem;
    uint32_t                ImageSubsystemMajorVersion;
    uint32_t                ImageSubsystemMinorVersion;
    uint64_t                ActiveProcessAffinityMask;
    uint32_t                GdiHandleBuffer[34];
    uint32_t                PostProcessInitRoutine;
    uint32_t                TlsExpansionBitmap;
    uint32_t                TlsExpansionBitmapBits[32];
    uint32_t                SessionId;
    uint64_t                AppCompatFlags;
    uint64_t                AppCompatFlagsUser;
    uint64_t                pShimData;
    uint64_t                AppCompatInfo;
    UNICODE_STRING          CSDVersion;
    uint64_t                ActivationContextData;
    uint64_t                ProcessAssemblyStorageMap;
    uint64_t                SystemDefaultActivationContextData;
    uint64_t                SystemAssemblyStorageMap;
    uint64_t                MinimumStackCommit;
    uint64_t                FlsCallback;
    LIST_ENTRY              FlsListHead;
    uint64_t                FlsBitmap;
    uint32_t                FlsBitmapBits[4];
    uint32_t                FlsHighIndex;
    uint64_t                WerRegistrationData;
    uint64_t                WerShipAssertPtr;
    uint64_t                pUnused;
    uint64_t                pImageHeaderHash;
    uint32_t                TracingFlags;
} PEB, *PPEB;

typedef struct _TEB {
    uint64_t                ExceptionList;
    uint64_t                StackBase;
    uint64_t                StackLimit;
    uint64_t                SubSystemTib;
    uint64_t                FiberData;
    uint64_t                ArbitraryUserPointer;
    struct _TEB*            Self;
    void*                   EnvironmentPointer;
    uint32_t                ClientId_UniqueProcess;
    uint32_t                ClientId_UniqueThread;
    uint32_t                ActiveRpcHandle;
    uint32_t                ThreadLocalStoragePointer;
    PPEB                    Peb;
    uint32_t                LastErrorValue;
    uint32_t                CountOfOwnedCriticalSections;
    uint32_t                CsrClientThread;
    uint32_t                Win32ThreadInfo;
    uint32_t                User32Reserved[26];
    uint32_t                UserReserved[5];
    uint64_t                TlsSlots[64];
    LIST_ENTRY              TlsLinks;
    uint32_t                Vdm;
    uint32_t                ReservedForNtRpc;
    uint32_t                DbgSsReserved[2];
    uint32_t                HardErrorsAreDisabled;
    uint32_t                Instrumentation[16];
    uint64_t                WinSockData;
    uint64_t                GdiBatchCount;
    uint32_t                Spare2;
    uint32_t                Spare3;
    uint32_t                Spare4;
    uint32_t                SecurityPort;
    uint32_t                LastStatusValue;
    uint32_t                StaticUnicodeString_Buffer[4];
    uint16_t                StaticUnicodeString_Length;
    uint16_t                StaticUnicodeString_MaximumLength;
    uint64_t                DeallocationStack;
    uint64_t                TlsSlots_Expansion[1024];
    LIST_ENTRY              TlsExpansionLinks;
} TEB, *PTEB;

typedef struct {
    HANDLE  Handle;
    void*   Object;
    uint32_t ObjectType;
    uint32_t AccessRights;
    uint32_t Flags;
    uint32_t RefCount;
} handle_entry_t;

typedef struct {
    handle_entry_t* entries;
    uint32_t        capacity;
    uint32_t        next_handle_value;
    uint32_t        handle_count;
} handle_table_t;

typedef struct _win32_process_t {
    uint32_t pid;
    uint32_t parent_pid;
    char image_path[256];
    char command_line[1024];
    char current_dir[256];
    PEB peb;
    handle_table_t handle_table;
    TEB* threads[WIN32_MAX_THREADS_PER_PROCESS];
    uint32_t thread_count;
    uint32_t exit_code;
    BOOL     is_running;
    uint32_t creation_time_low;
    uint32_t creation_time_high;
    uint64_t user_time;
    uint64_t kernel_time;
    HMODULE  image_base;
    uint32_t image_size;
    uint32_t priority_class;
    void*    page_directory;
    BOOL     is_32bit_process;
    uint32_t session_id;
    BOOL     wow64_process;
} win32_process_t;

win32_process_t* win32_get_process(uint32_t pid);
win32_process_t* win32_create_process(const char* image_path, const char* command_line, const char* current_dir);
void* win32_handle_to_object(HANDLE h, uint32_t expect_type);

#endif // ARCH_X86_64_WIN32_PROCESS_H
