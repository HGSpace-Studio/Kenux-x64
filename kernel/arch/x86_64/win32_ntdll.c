#include <arch/win32.h>
#include <string.h>

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* p);
extern void* win32_handle_to_object(HANDLE h, uint32_t expect_type);
extern BOOL  win32_handle_close(HANDLE h);

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR  Buffer;
} STRING, ANSI_STRING, *PSTRING, *PANSI_STRING;

/* ================================================================
 * RtlXXX - Runtime Library functions
 * ================================================================ */

void RtlInitUnicodeString(PUNICODE_STRING s, PCWSTR p) {
    if (s == NULL) {
        return;
    }
    if (p == NULL) {
        s->Length = 0;
        s->MaximumLength = 0;
        s->Buffer = NULL;
        return;
    }
    size_t len = 0;
    while (p[len] != 0) {
        len++;
    }
    s->Length = (USHORT)(len * sizeof(WCHAR));
    s->MaximumLength = (USHORT)((len + 1) * sizeof(WCHAR));
    s->Buffer = (PWSTR)p;
}

NTSTATUS RtlAnsiStringToUnicodeString(PUNICODE_STRING dst,
                                       const void* src, BOOL alloc) {
    PANSI_STRING ansi;
    size_t i;
    size_t ansi_len;
    PWSTR buf;

    if (dst == NULL || src == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    ansi = (PANSI_STRING)src;
    ansi_len = ansi->Length;

    if (alloc) {
        buf = (PWSTR)memory_alloc((ansi_len + 1) * sizeof(WCHAR));
        if (buf == NULL) {
            return STATUS_NO_MEMORY;
        }
    } else {
        if (dst->Buffer == NULL || dst->MaximumLength < (ansi_len + 1) * sizeof(WCHAR)) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        buf = dst->Buffer;
    }

    for (i = 0; i < ansi_len; i++) {
        buf[i] = (WCHAR)((unsigned char)ansi->Buffer[i]);
    }
    buf[ansi_len] = 0;

    dst->Length = (USHORT)(ansi_len * sizeof(WCHAR));
    if (alloc) {
        dst->MaximumLength = (USHORT)((ansi_len + 1) * sizeof(WCHAR));
        dst->Buffer = buf;
    }

    return STATUS_SUCCESS;
}

NTSTATUS RtlUnicodeStringToAnsiString(void* dst,
                                       PUNICODE_STRING src, BOOL alloc) {
    PANSI_STRING ansi;
    size_t i;
    size_t unicode_len;
    PCHAR buf;

    if (dst == NULL || src == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    ansi = (PANSI_STRING)dst;
    unicode_len = src->Length / sizeof(WCHAR);

    if (alloc) {
        buf = (PCHAR)memory_alloc(unicode_len + 1);
        if (buf == NULL) {
            return STATUS_NO_MEMORY;
        }
    } else {
        if (ansi->Buffer == NULL || ansi->MaximumLength < unicode_len + 1) {
            return STATUS_BUFFER_TOO_SMALL;
        }
        buf = ansi->Buffer;
    }

    for (i = 0; i < unicode_len; i++) {
        WCHAR c = src->Buffer[i];
        if (c > 0xFF) {
            buf[i] = '?';
        } else {
            buf[i] = (CHAR)c;
        }
    }
    buf[unicode_len] = 0;

    ansi->Length = (USHORT)unicode_len;
    if (alloc) {
        ansi->MaximumLength = (USHORT)(unicode_len + 1);
        ansi->Buffer = buf;
    }

    return STATUS_SUCCESS;
}

LONG RtlCompareUnicodeString(PUNICODE_STRING a, PUNICODE_STRING b, BOOL cs) {
    size_t len_a;
    size_t len_b;
    size_t min_len;
    size_t i;

    if (a == NULL || b == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    len_a = a->Length / sizeof(WCHAR);
    len_b = b->Length / sizeof(WCHAR);
    min_len = len_a < len_b ? len_a : len_b;

    for (i = 0; i < min_len; i++) {
        WCHAR ca = a->Buffer[i];
        WCHAR cb = b->Buffer[i];

        if (!cs) {
            if (ca >= 'A' && ca <= 'Z') ca = (WCHAR)(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = (WCHAR)(cb - 'A' + 'a');
        }

        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
    }

    if (len_a == len_b) return 0;
    return (len_a < len_b) ? -1 : 1;
}

VOID RtlCopyUnicodeString(PUNICODE_STRING dst, PUNICODE_STRING src) {
    if (dst == NULL || src == NULL) {
        return;
    }

    USHORT copy_len = src->Length;
    if (copy_len > dst->MaximumLength) {
        copy_len = dst->MaximumLength;
    }

    if (copy_len > 0 && dst->Buffer != NULL && src->Buffer != NULL) {
        memcpy(dst->Buffer, src->Buffer, copy_len);
    }

    dst->Length = copy_len;
    if (dst->Length < dst->MaximumLength && dst->Buffer != NULL) {
        dst->Buffer[dst->Length / sizeof(WCHAR)] = 0;
    }
}

VOID RtlFreeUnicodeString(PUNICODE_STRING s) {
    if (s == NULL) {
        return;
    }
    if (s->Buffer != NULL) {
        memory_free(s->Buffer);
        s->Buffer = NULL;
    }
    s->Length = 0;
    s->MaximumLength = 0;
}

NTSTATUS RtlIntegerToUnicodeString(ULONG val, ULONG base, PUNICODE_STRING s) {
    WCHAR buf[32];
    ULONG i = 0;
    ULONG len;
    ULONG remainder;
    WCHAR digits[] = L"0123456789ABCDEF";

    if (s == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (base < 2 || base > 16) {
        return STATUS_INVALID_PARAMETER;
    }

    if (val == 0) {
        buf[i++] = L'0';
    } else {
        while (val > 0) {
            remainder = val % base;
            buf[i++] = digits[remainder];
            val = val / base;
        }
    }
    len = i;

    if (s->Buffer == NULL || s->MaximumLength < (len + 1) * sizeof(WCHAR)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    for (ULONG j = 0; j < len; j++) {
        s->Buffer[j] = buf[len - 1 - j];
    }
    s->Buffer[len] = 0;
    s->Length = (USHORT)(len * sizeof(WCHAR));

    return STATUS_SUCCESS;
}

NTSTATUS RtlUnicodeStringToInteger(PUNICODE_STRING s, ULONG base, PULONG val) {
    ULONG result = 0;
    size_t len;
    size_t i;

    if (s == NULL || val == NULL || s->Buffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    len = s->Length / sizeof(WCHAR);

    for (i = 0; i < len; i++) {
        WCHAR c = s->Buffer[i];
        ULONG digit;

        if (c >= L'0' && c <= L'9') {
            digit = c - L'0';
        } else if (c >= L'A' && c <= L'F') {
            digit = 10 + (c - L'A');
        } else if (c >= L'a' && c <= L'f') {
            digit = 10 + (c - L'a');
        } else {
            break;
        }

        if (base == 0) {
            base = (digit == 0 && (i + 1 < len && (s->Buffer[i+1] == L'x' || s->Buffer[i+1] == L'X'))) ? 16 : 10;
            if (digit == 0 && i + 1 < len && (s->Buffer[i+1] == L'x' || s->Buffer[i+1] == L'X')) {
                i++;
                continue;
            }
        }

        if (digit >= base) {
            break;
        }

        result = result * base + digit;
    }

    *val = result;
    return STATUS_SUCCESS;
}

ULONG RtlHashUnicodeString(PUNICODE_STRING s, BOOLEAN cs, ULONG hc) {
    size_t len;
    size_t i;
    ULONG hash = hc;

    if (s == NULL || s->Buffer == NULL) {
        return hash;
    }

    len = s->Length / sizeof(WCHAR);

    for (i = 0; i < len; i++) {
        WCHAR c = s->Buffer[i];
        if (!cs) {
            if (c >= L'A' && c <= L'Z') {
                c = (WCHAR)(c - L'A' + L'a');
            }
        }
        hash = (hash * 65599) + c;
    }

    return hash;
}

BOOLEAN RtlEqualMemory(const void* a, const void* b, SIZE_T n) {
    if (n == 0) return TRUE;
    if (a == NULL || b == NULL) return FALSE;
    return memcmp(a, b, n) == 0 ? TRUE : FALSE;
}

VOID RtlCopyMemory(PVOID dst, const VOID* src, SIZE_T n) {
    if (n == 0) return;
    if (dst == NULL || src == NULL) return;
    memcpy(dst, src, n);
}

VOID RtlFillMemory(PVOID dst, SIZE_T n, INT c) {
    if (n == 0) return;
    if (dst == NULL) return;
    memset(dst, c, n);
}

VOID RtlZeroMemory(PVOID dst, SIZE_T n) {
    if (n == 0) return;
    if (dst == NULL) return;
    memset(dst, 0, n);
}

SIZE_T RtlCompareMemory(const VOID* a, const VOID* b, SIZE_T n) {
    SIZE_T i;
    const unsigned char* pa;
    const unsigned char* pb;

    if (a == NULL || b == NULL) return 0;

    pa = (const unsigned char*)a;
    pb = (const unsigned char*)b;

    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            break;
        }
    }
    return i;
}

VOID RtlMoveMemory(PVOID dst, const VOID* src, SIZE_T n) {
    if (n == 0) return;
    if (dst == NULL || src == NULL) return;
    memmove(dst, src, n);
}

ULONG RtlUniform(PULONG seed) {
    if (seed == NULL) {
        return 0;
    }
    *seed = (*seed * 1103515245UL + 12345UL) & 0x7FFFFFFFUL;
    return *seed;
}

ULONG RtlRandomEx(PULONG seed) {
    if (seed == NULL) {
        return 0;
    }
    *seed = (*seed * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
    return *seed;
}

NTSTATUS RtlCreateUnicodeStringFromAsciiz(PUNICODE_STRING dst, const char* s) {
    size_t len;
    size_t i;
    PWSTR buf;

    if (dst == NULL || s == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    len = strlen(s);
    buf = (PWSTR)memory_alloc((len + 1) * sizeof(WCHAR));
    if (buf == NULL) {
        dst->Length = 0;
        dst->MaximumLength = 0;
        dst->Buffer = NULL;
        return STATUS_NO_MEMORY;
    }

    for (i = 0; i < len; i++) {
        buf[i] = (WCHAR)((unsigned char)s[i]);
    }
    buf[len] = 0;

    dst->Length = (USHORT)(len * sizeof(WCHAR));
    dst->MaximumLength = (USHORT)((len + 1) * sizeof(WCHAR));
    dst->Buffer = buf;

    return STATUS_SUCCESS;
}

/* ================================================================
 * NtXXX / ZwXXX - Native syscall stubs
 * ================================================================ */

NTSTATUS NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
                      POBJECT_ATTRIBUTES ObjectAttributes,
                      PIO_STATUS_BLOCK IoStatusBlock,
                      PLARGE_INTEGER AllocationSize,
                      ULONG FileAttributes, ULONG ShareAccess,
                      ULONG CreateDisposition, ULONG CreateOptions,
                      PVOID EaBuffer, ULONG EaLength) {
    (void)ObjectAttributes;
    (void)IoStatusBlock;
    (void)AllocationSize;
    (void)FileAttributes;
    (void)ShareAccess;
    (void)CreateDisposition;
    (void)CreateOptions;
    (void)EaBuffer;
    (void)EaLength;

    if (FileHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess,
                    POBJECT_ATTRIBUTES ObjectAttributes,
                    PIO_STATUS_BLOCK IoStatusBlock,
                    ULONG ShareAccess, ULONG OpenOptions) {
    (void)ObjectAttributes;
    (void)IoStatusBlock;
    (void)ShareAccess;
    (void)OpenOptions;
    (void)DesiredAccess;

    if (FileHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtReadFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
                    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
                    PVOID Buffer, ULONG Length,
                    PLARGE_INTEGER ByteOffset, PULONG Key) {
    (void)Event;
    (void)ApcRoutine;
    (void)ApcContext;
    (void)ByteOffset;
    (void)Key;

    if (FileHandle == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (win32_handle_to_object(FileHandle, 0) == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (Buffer != NULL && Length > 0) {
        memset(Buffer, 0, Length);
    }
    if (IoStatusBlock != NULL) {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtWriteFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
                     PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
                     const VOID* Buffer, ULONG Length,
                     PLARGE_INTEGER ByteOffset, PULONG Key) {
    (void)Event;
    (void)ApcRoutine;
    (void)ApcContext;
    (void)Buffer;
    (void)Length;
    (void)ByteOffset;
    (void)Key;

    if (FileHandle == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (win32_handle_to_object(FileHandle, 0) == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (IoStatusBlock != NULL) {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtClose(HANDLE Handle) {
    if (Handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (!win32_handle_close(Handle)) {
        return STATUS_INVALID_HANDLE;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryInformationFile(HANDLE FileHandle,
                                PIO_STATUS_BLOCK IoStatusBlock,
                                PVOID FileInformation,
                                ULONG Length, ULONG FileInformationClass) {
    (void)FileInformationClass;

    if (FileHandle == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (win32_handle_to_object(FileHandle, 0) == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (IoStatusBlock != NULL) {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = 0;
    }
    if (FileInformation == NULL) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(FileInformation, 0, Length);
    return STATUS_SUCCESS;
}

NTSTATUS NtSetInformationFile(HANDLE FileHandle,
                              PIO_STATUS_BLOCK IoStatusBlock,
                              PVOID FileInformation,
                              ULONG Length, ULONG FileInformationClass) {
    (void)FileInformationClass;
    (void)FileInformation;
    (void)Length;

    if (FileHandle == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (win32_handle_to_object(FileHandle, 0) == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (IoStatusBlock != NULL) {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = 0;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtCreateSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess,
                         POBJECT_ATTRIBUTES ObjectAttributes,
                         PLARGE_INTEGER MaximumSize,
                         ULONG SectionPageProtection,
                         ULONG AllocationAttributes,
                         HANDLE FileHandle) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)MaximumSize;
    (void)SectionPageProtection;
    (void)AllocationAttributes;
    (void)FileHandle;

    if (SectionHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle,
                            PVOID* BaseAddress,
                            ULONG_PTR ZeroBits,
                            SIZE_T CommitSize,
                            PLARGE_INTEGER SectionOffset,
                            PSIZE_T ViewSize,
                            ULONG InheritDisposition,
                            ULONG AllocationType,
                            ULONG Win32Protect) {
    (void)SectionHandle;
    (void)ProcessHandle;
    (void)ZeroBits;
    (void)CommitSize;
    (void)SectionOffset;
    (void)InheritDisposition;
    (void)AllocationType;
    (void)Win32Protect;

    if (BaseAddress == NULL || ViewSize == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (*BaseAddress == NULL) {
        *BaseAddress = memory_alloc(*ViewSize);
        if (*BaseAddress == NULL) {
            return STATUS_NO_MEMORY;
        }
        memset(*BaseAddress, 0, *ViewSize);
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtUnmapViewOfSection(HANDLE ProcessHandle, PVOID BaseAddress) {
    (void)ProcessHandle;

    if (BaseAddress == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    memory_free(BaseAddress);
    return STATUS_SUCCESS;
}

NTSTATUS NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
                                 ULONG_PTR ZeroBits, PSIZE_T RegionSize,
                                 ULONG AllocationType, ULONG Protect) {
    (void)ProcessHandle;
    (void)ZeroBits;
    (void)AllocationType;
    (void)Protect;

    if (BaseAddress == NULL || RegionSize == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (*BaseAddress == NULL) {
        *BaseAddress = memory_alloc(*RegionSize);
        if (*BaseAddress == NULL) {
            return STATUS_NO_MEMORY;
        }
        memset(*BaseAddress, 0, *RegionSize);
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtFreeVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
                             PSIZE_T RegionSize, ULONG FreeType) {
    (void)ProcessHandle;
    (void)FreeType;
    (void)RegionSize;

    if (BaseAddress == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (*BaseAddress != NULL) {
        memory_free(*BaseAddress);
        *BaseAddress = NULL;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress,
                                PSIZE_T RegionSize, ULONG NewProtect,
                                PULONG OldProtect) {
    (void)ProcessHandle;
    (void)BaseAddress;
    (void)RegionSize;
    (void)NewProtect;

    if (BaseAddress == NULL || RegionSize == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (OldProtect != NULL) {
        *OldProtect = PAGE_READWRITE;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryVirtualMemory(HANDLE ProcessHandle, LPCVOID Address,
                              ULONG MemoryInformationClass,
                              PVOID Buffer, SIZE_T Length, PSIZE_T Result) {
    (void)ProcessHandle;
    (void)Address;
    (void)MemoryInformationClass;

    if (Buffer == NULL) {
        if (Result != NULL) {
            *Result = 0;
        }
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (Length == 0) {
        if (Result != NULL) {
            *Result = 0;
        }
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(Buffer, 0, Length);
    if (Result != NULL) {
        *Result = Length;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtCreateProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess,
                         POBJECT_ATTRIBUTES ObjectAttributes,
                         HANDLE ParentProcess,
                         BOOLEAN InheritObjectTable,
                         HANDLE SectionHandle,
                         HANDLE DebugPort, HANDLE ExceptionPort) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)ParentProcess;
    (void)InheritObjectTable;
    (void)SectionHandle;
    (void)DebugPort;
    (void)ExceptionPort;

    if (ProcessHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtCreateThreadEx(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess,
                          POBJECT_ATTRIBUTES ObjectAttributes,
                          HANDLE ProcessHandle, PVOID StartRoutine,
                          PVOID Argument, ULONG CreateFlags,
                          ULONG_PTR ZeroBits, SIZE_T StackSize,
                          SIZE_T MaximumStackSize, PVOID AttributeList) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)ProcessHandle;
    (void)StartRoutine;
    (void)Argument;
    (void)CreateFlags;
    (void)ZeroBits;
    (void)StackSize;
    (void)MaximumStackSize;
    (void)AttributeList;

    if (ThreadHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtTerminateProcess(HANDLE ProcessHandle, NTSTATUS ExitStatus) {
    (void)ProcessHandle;
    (void)ExitStatus;
    return STATUS_SUCCESS;
}

NTSTATUS NtTerminateThread(HANDLE ThreadHandle, NTSTATUS ExitStatus) {
    (void)ThreadHandle;
    (void)ExitStatus;
    return STATUS_SUCCESS;
}

NTSTATUS NtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable,
                               const LARGE_INTEGER* Timeout) {
    (void)Alertable;
    (void)Timeout;

    if (Handle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtWaitForMultipleObjects(ULONG Count, const HANDLE* Handles,
                                  ULONG WaitType, BOOLEAN Alertable,
                                  const LARGE_INTEGER* Timeout) {
    (void)WaitType;
    (void)Alertable;
    (void)Timeout;

    if (Count == 0 || Handles == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    for (ULONG i = 0; i < Count; i++) {
        if (Handles[i] == NULL) {
            return STATUS_INVALID_PARAMETER;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtQuerySystemInformation(ULONG SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength,
                                   PULONG ReturnLength) {
    (void)SystemInformationClass;

    if (ReturnLength != NULL) {
        *ReturnLength = 0;
    }
    if (SystemInformation == NULL) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (SystemInformationLength == 0) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(SystemInformation, 0, SystemInformationLength);
    if (ReturnLength != NULL) {
        *ReturnLength = SystemInformationLength;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtSetSystemInformation(ULONG SystemInformationClass,
                                 PVOID SystemInformation,
                                 ULONG SystemInformationLength) {
    (void)SystemInformationClass;
    (void)SystemInformation;
    (void)SystemInformationLength;

    if (SystemInformation == NULL && SystemInformationLength > 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtQueryInformationProcess(HANDLE ProcessHandle,
                                   ULONG ProcessInformationClass,
                                   PVOID ProcessInformation,
                                   ULONG ProcessInformationLength,
                                   PULONG ReturnLength) {
    (void)ProcessHandle;
    (void)ProcessInformationClass;

    if (ReturnLength != NULL) {
        *ReturnLength = 0;
    }
    if (ProcessInformation == NULL) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (ProcessInformationLength < 4) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(ProcessInformation, 0, ProcessInformationLength);
    if (ReturnLength != NULL) {
        *ReturnLength = ProcessInformationLength;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryInformationThread(HANDLE ThreadHandle,
                                  ULONG ThreadInformationClass,
                                  PVOID ThreadInformation,
                                  ULONG ThreadInformationLength,
                                  PULONG ReturnLength) {
    (void)ThreadHandle;
    (void)ThreadInformationClass;

    if (ReturnLength != NULL) {
        *ReturnLength = 0;
    }
    if (ThreadInformation == NULL) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (ThreadInformationLength < 4) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(ThreadInformation, 0, ThreadInformationLength);
    if (ReturnLength != NULL) {
        *ReturnLength = ThreadInformationLength;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtDuplicateObject(HANDLE SourceProcessHandle, HANDLE SourceHandle,
                           HANDLE TargetProcessHandle, PHANDLE TargetHandle,
                           ACCESS_MASK DesiredAccess, ULONG HandleAttributes,
                           ULONG Options) {
    (void)SourceProcessHandle;
    (void)SourceHandle;
    (void)TargetProcessHandle;
    (void)DesiredAccess;
    (void)HandleAttributes;
    (void)Options;

    if (TargetHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (SourceHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *TargetHandle = SourceHandle;
    return STATUS_SUCCESS;
}

NTSTATUS NtLoadDriver(PUNICODE_STRING DriverServiceName) {
    if (DriverServiceName == NULL || DriverServiceName->Buffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (DriverServiceName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtUnloadDriver(PUNICODE_STRING DriverServiceName) {
    if (DriverServiceName == NULL || DriverServiceName->Buffer == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (DriverServiceName->Length == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event,
                               PVOID ApcRoutine, PVOID ApcContext,
                               PIO_STATUS_BLOCK IoStatusBlock,
                               ULONG IoControlCode,
                               PVOID InputBuffer, ULONG InputBufferLength,
                               PVOID OutputBuffer, ULONG OutputBufferLength) {
    (void)Event;
    (void)ApcRoutine;
    (void)ApcContext;
    (void)IoControlCode;
    (void)InputBuffer;
    (void)InputBufferLength;
    (void)OutputBuffer;
    (void)OutputBufferLength;

    if (FileHandle == NULL) {
        return STATUS_INVALID_HANDLE;
    }
    if (IoStatusBlock == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    IoStatusBlock->Status = STATUS_SUCCESS;
    IoStatusBlock->Information = 0;
    return STATUS_SUCCESS;
}

NTSTATUS NtCreateEvent(PHANDLE EventHandle, ACCESS_MASK DesiredAccess,
                       POBJECT_ATTRIBUTES ObjectAttributes,
                       ULONG EventType, ULONG EventAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)EventType;
    (void)EventAttributes;

    if (EventHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *EventHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenEvent(PHANDLE EventHandle, ACCESS_MASK DesiredAccess,
                     POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (EventHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *EventHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtSetEvent(HANDLE EventHandle, ULONG* PreviousState) {
    (void)EventHandle;
    if (PreviousState != NULL) {
        *PreviousState = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtResetEvent(HANDLE EventHandle, ULONG* PreviousState) {
    (void)EventHandle;
    if (PreviousState != NULL) {
        *PreviousState = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtClearEvent(HANDLE EventHandle) {
    (void)EventHandle;
    return STATUS_SUCCESS;
}

NTSTATUS NtPulseEvent(HANDLE EventHandle, ULONG* PreviousState) {
    (void)EventHandle;
    if (PreviousState != NULL) {
        *PreviousState = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtCreateMutant(PHANDLE MutantHandle, ACCESS_MASK DesiredAccess,
                        POBJECT_ATTRIBUTES ObjectAttributes,
                        BOOLEAN InitialOwner) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)InitialOwner;

    if (MutantHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *MutantHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenMutant(PHANDLE MutantHandle, ACCESS_MASK DesiredAccess,
                      POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (MutantHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *MutantHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtReleaseMutant(HANDLE MutantHandle, LONG* PreviousCount) {
    (void)MutantHandle;
    if (PreviousCount != NULL) {
        *PreviousCount = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtCreateSemaphore(PHANDLE SemaphoreHandle, ACCESS_MASK DesiredAccess,
                           POBJECT_ATTRIBUTES ObjectAttributes,
                           LONG InitialCount, LONG MaximumCount) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (SemaphoreHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    if (MaximumCount <= 0 || InitialCount < 0 || InitialCount > MaximumCount) {
        return STATUS_INVALID_PARAMETER;
    }
    *SemaphoreHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenSemaphore(PHANDLE SemaphoreHandle, ACCESS_MASK DesiredAccess,
                         POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (SemaphoreHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *SemaphoreHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtReleaseSemaphore(HANDLE SemaphoreHandle, LONG ReleaseCount,
                             LONG* PreviousCount) {
    (void)SemaphoreHandle;
    (void)ReleaseCount;
    if (PreviousCount != NULL) {
        *PreviousCount = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtCreateTimer(PHANDLE TimerHandle, ACCESS_MASK DesiredAccess,
                       POBJECT_ATTRIBUTES ObjectAttributes, ULONG TimerType) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)TimerType;

    if (TimerHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *TimerHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenTimer(PHANDLE TimerHandle, ACCESS_MASK DesiredAccess,
                     POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (TimerHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *TimerHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtSetTimer(HANDLE TimerHandle, PLARGE_INTEGER DueTime,
                     PTIMER_APC_ROUTINE TimerApcRoutine,
                     PVOID TimerContext, ULONG wakeMask,
                     ULONG StartFlags, ULONG* PreviousState) {
    (void)TimerHandle;
    (void)DueTime;
    (void)TimerApcRoutine;
    (void)TimerContext;
    (void)wakeMask;
    (void)StartFlags;
    if (PreviousState != NULL) {
        *PreviousState = 0;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtCancelTimer(HANDLE TimerHandle, BOOLEAN* CurrentState) {
    (void)TimerHandle;
    if (CurrentState != NULL) {
        *CurrentState = FALSE;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryTimer(HANDLE TimerHandle, ULONG TimerInformationClass,
                      PVOID TimerInformation, ULONG TimerInformationLength,
                      PULONG ReturnLength) {
    (void)TimerHandle;
    (void)TimerInformationClass;

    if (ReturnLength != NULL) {
        *ReturnLength = 0;
    }
    if (TimerInformation == NULL) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(TimerInformation, 0, TimerInformationLength);
    if (ReturnLength != NULL) {
        *ReturnLength = TimerInformationLength;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtCreateKey(PHANDLE KeyHandle, ACCESS_MASK DesiredAccess,
                     POBJECT_ATTRIBUTES ObjectAttributes,
                     ULONG TitleIndex, void* Class,
                     ULONG CreateOptions, ULONG* Disposition) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)TitleIndex;
    (void)Class;
    (void)CreateOptions;

    if (KeyHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *KeyHandle = NULL;
    if (Disposition != NULL) {
        *Disposition = 0;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenKey(PHANDLE KeyHandle, ACCESS_MASK DesiredAccess,
                   POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (KeyHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *KeyHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtDeleteKey(HANDLE KeyHandle) {
    (void)KeyHandle;
    return STATUS_SUCCESS;
}

NTSTATUS NtDeleteValueKey(HANDLE KeyHandle, PUNICODE_STRING ValueName) {
    (void)KeyHandle;
    (void)ValueName;
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryKey(HANDLE KeyHandle, ULONG KeyInformationClass,
                   PVOID KeyInformation, ULONG Length, PULONG Result) {
    (void)KeyHandle;
    (void)KeyInformationClass;

    if (Result != NULL) {
        *Result = 0;
    }
    if (KeyInformation == NULL && Length > 0) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (KeyInformation != NULL && Length > 0) {
        memset(KeyInformation, 0, Length);
    }
    if (Result != NULL) {
        *Result = Length;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtSetValueKey(HANDLE KeyHandle, PUNICODE_STRING ValueName,
                       ULONG TitleIndex, ULONG Type,
                       const void* Data, ULONG DataSize) {
    (void)KeyHandle;
    (void)ValueName;
    (void)TitleIndex;
    (void)Type;
    (void)Data;
    (void)DataSize;
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryValueKey(HANDLE KeyHandle, PUNICODE_STRING ValueName,
                        KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
                        PVOID KeyValueInformation, ULONG Length,
                        PULONG Result) {
    (void)KeyHandle;
    (void)ValueName;
    (void)KeyValueInformationClass;

    if (Result != NULL) {
        *Result = 0;
    }
    if (KeyValueInformation == NULL && Length > 0) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (KeyValueInformation != NULL && Length > 0) {
        memset(KeyValueInformation, 0, Length);
    }
    if (Result != NULL) {
        *Result = Length;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtEnumerateKey(HANDLE KeyHandle, ULONG Index,
                       KEY_INFORMATION_CLASS KeyInformationClass,
                       PVOID KeyInformation, ULONG Length,
                       PULONG Result) {
    (void)KeyHandle;
    (void)Index;
    (void)KeyInformationClass;

    if (Result != NULL) {
        *Result = 0;
    }
    if (KeyInformation == NULL && Length > 0) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (KeyInformation != NULL && Length > 0) {
        memset(KeyInformation, 0, Length);
    }
    if (Result != NULL) {
        *Result = Length;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtEnumerateValueKey(HANDLE KeyHandle, ULONG Index,
                             KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
                             PVOID KeyValueInformation, ULONG Length,
                             PULONG Result) {
    (void)KeyHandle;
    (void)Index;
    (void)KeyValueInformationClass;

    if (Result != NULL) {
        *Result = 0;
    }
    if (KeyValueInformation == NULL && Length > 0) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (KeyValueInformation != NULL && Length > 0) {
        memset(KeyValueInformation, 0, Length);
    }
    if (Result != NULL) {
        *Result = Length;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtFlushKey(HANDLE KeyHandle) {
    (void)KeyHandle;
    return STATUS_SUCCESS;
}

NTSTATUS NtQueryObject(HANDLE Handle, OBJECT_INFORMATION_CLASS ObjectInformationClass,
                      PVOID ObjectInformation, ULONG ObjectInformationLength,
                      PULONG ReturnLength) {
    (void)Handle;
    (void)ObjectInformationClass;

    if (ReturnLength != NULL) {
        *ReturnLength = 0;
    }
    if (ObjectInformation == NULL && ObjectInformationLength > 0) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    if (ObjectInformation != NULL && ObjectInformationLength > 0) {
        memset(ObjectInformation, 0, ObjectInformationLength);
    }
    if (ReturnLength != NULL) {
        *ReturnLength = ObjectInformationLength;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtQuerySymbolicLinkObject(HANDLE LinkHandle, PUNICODE_STRING LinkTarget,
                                  PULONG Result) {
    (void)LinkHandle;
    (void)LinkTarget;
    if (Result != NULL) {
        *Result = 0;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtCreateSymbolicLinkObject(PHANDLE LinkHandle, ACCESS_MASK DesiredAccess,
                                    POBJECT_ATTRIBUTES ObjectAttributes,
                                    PUNICODE_STRING LinkTarget) {
    (void)DesiredAccess;
    (void)ObjectAttributes;
    (void)LinkTarget;

    if (LinkHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *LinkHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtCreateDirectoryObject(PHANDLE DirectoryHandle, ACCESS_MASK DesiredAccess,
                                 POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (DirectoryHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *DirectoryHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtOpenDirectoryObject(PHANDLE DirectoryHandle, ACCESS_MASK DesiredAccess,
                               POBJECT_ATTRIBUTES ObjectAttributes) {
    (void)DesiredAccess;
    (void)ObjectAttributes;

    if (DirectoryHandle == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    *DirectoryHandle = NULL;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NtQueryDirectoryObject(HANDLE DirectoryHandle,
                                 PVOID Buffer, ULONG BufferLength,
                                 BOOLEAN ReturnSingleEntry,
                                 BOOLEAN RestartScan,
                                 ULONG* Context,
                                 ULONG* Result) {
    (void)DirectoryHandle;
    (void)Buffer;
    (void)ReturnSingleEntry;
    (void)RestartScan;
    (void)Context;
    if (Result != NULL) {
        *Result = 0;
    }
    return STATUS_SUCCESS;
}

/* ================================================================
 * NTDLL init
 * ================================================================ */

int ntdll_init(void) {
    return 0;
}