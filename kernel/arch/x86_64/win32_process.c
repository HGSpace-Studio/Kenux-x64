#include <arch/win32.h>
#include <arch/win32_process.h>
#include <arch/pe.h>
#include <arch/slab.h>
#include <string.h>
#include <arch/process.h>

extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS              0x00000000U
#endif
#ifndef STATUS_INVALID_HANDLE
#define STATUS_INVALID_HANDLE       0xC0000008U
#endif
#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED        0xC0000022U
#endif
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004U
#endif

#define TLS_OUT_OF_INDEXES          0xFFFFFFFFU

#define HANDLE_TABLE_SIZE           16384
#define WIN32_MAX_PROCESSES         128
#define HANDLE_FLAG_INHERIT             0x00000001U
#define HANDLE_FLAG_PROTECT_FROM_CLOSE  0x00000002U
#define HANDLE_FLAG_AUDIT_OBJECT_CLOSE  0x00000004U

#define HANDLE_OBJECT_TYPE_FREE         0U
#define HANDLE_OBJECT_TYPE_FILE         1U
#define HANDLE_OBJECT_TYPE_EVENT        2U
#define HANDLE_OBJECT_TYPE_MUTEX        3U
#define HANDLE_OBJECT_TYPE_SEMAPHORE    4U
#define HANDLE_OBJECT_TYPE_THREAD       5U
#define HANDLE_OBJECT_TYPE_PROCESS      6U
#define HANDLE_OBJECT_TYPE_SECTION      7U
#define HANDLE_OBJECT_TYPE_KEY          8U
#define HANDLE_OBJECT_TYPE_TOKEN        9U
#define HANDLE_OBJECT_TYPE_MAILSLOT     10U
#define HANDLE_OBJECT_TYPE_NAMEDPIPE    11U
#define HANDLE_OBJECT_TYPE_SOCKET       12U

static win32_process_t g_processes[WIN32_MAX_PROCESSES];
static win32_process_t* g_current_process = NULL;
static TEB* g_current_thread = NULL;

static uint64_t g_tls_bitmap = 0ULL;
static uint32_t g_next_pid = 100U;

static void list_init_head(PLIST_ENTRY head) {
    head->Flink = head;
    head->Blink = head;
}

static void unicode_string_init(PUNICODE_STRING us, const char* ansi_str) {
    size_t len;
    size_t i;
    uint16_t* buf;

    if (ansi_str == NULL) {
        us->Length = 0;
        us->MaximumLength = 0;
        us->Buffer = NULL;
        return;
    }

    len = strlen(ansi_str);
    if (len > 255) {
        len = 255;
    }

    buf = (uint16_t*)memory_alloc((len + 1) * sizeof(uint16_t));
    if (buf == NULL) {
        us->Length = 0;
        us->MaximumLength = 0;
        us->Buffer = NULL;
        return;
    }

    for (i = 0; i < len; i++) {
        buf[i] = (uint16_t)((unsigned char)ansi_str[i]);
    }
    buf[len] = 0;

    us->Length = (uint16_t)(len * sizeof(uint16_t));
    us->MaximumLength = (uint16_t)((len + 1) * sizeof(uint16_t));
    us->Buffer = buf;
}

static PRTL_USER_PROCESS_PARAMETERS create_process_parameters(
    const char* image_path,
    const char* command_line,
    const char* current_dir
) {
    PRTL_USER_PROCESS_PARAMETERS params;

    params = (PRTL_USER_PROCESS_PARAMETERS)memory_alloc(sizeof(RTL_USER_PROCESS_PARAMETERS));
    if (params == NULL) {
        return NULL;
    }
    memset(params, 0, sizeof(RTL_USER_PROCESS_PARAMETERS));

    params->MaximumLength = sizeof(RTL_USER_PROCESS_PARAMETERS);
    params->Length = sizeof(RTL_USER_PROCESS_PARAMETERS);
    params->Flags = 0U;
    params->DebugFlags = 0U;
    params->ConsoleHandle = NULL;
    params->ConsoleFlags = 0U;
    params->StandardInput = NULL;
    params->StandardOutput = NULL;
    params->StandardError = NULL;

    unicode_string_init(&params->CurrentDirectory, current_dir);
    unicode_string_init(&params->DllPath, "C:\\Windows\\System32");
    unicode_string_init(&params->ImagePathName, image_path);
    unicode_string_init(&params->CommandLine, command_line);

    params->EnvironmentSize = 0U;
    params->Environment = NULL;
    params->StartingX = 0U;
    params->StartingY = 0U;
    params->CountX = 80U;
    params->CountY = 25U;
    params->CountCharsX = 80U;
    params->CountCharsY = 25U;
    params->FillAttribute = 0x0007U;
    params->WindowFlags = 0U;
    params->ShowWindowFlags = 0U;

    return params;
}

static PPEB_LDR_DATA create_peb_ldr_data(void) {
    PPEB_LDR_DATA ldr;

    ldr = (PPEB_LDR_DATA)memory_alloc(sizeof(PEB_LDR_DATA));
    if (ldr == NULL) {
        return NULL;
    }
    memset(ldr, 0, sizeof(PEB_LDR_DATA));

    ldr->Length = sizeof(PEB_LDR_DATA);
    ldr->Initialized = TRUE;
    ldr->SsHandle = NULL;

    list_init_head(&ldr->InLoadOrderModuleList);
    list_init_head(&ldr->InMemoryOrderModuleList);
    list_init_head(&ldr->InInitializationOrderModuleList);

    return ldr;
}

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* p);

static void handle_table_init(handle_table_t* ht) {
    uint32_t i;

    memset(ht, 0, sizeof(handle_table_t));
    ht->capacity = HANDLE_TABLE_SIZE;
    ht->next_handle_value = 4U;
    ht->handle_count = 0U;
    ht->entries = (handle_entry_t*)memory_alloc((uint64_t)sizeof(handle_entry_t) * (uint64_t)HANDLE_TABLE_SIZE);
    if (ht->entries == NULL) {
        ht->capacity = 0U;
        return;
    }

    for (i = 0U; i < HANDLE_TABLE_SIZE; i++) {
        ht->entries[i].Handle = NULL;
        ht->entries[i].Object = NULL;
        ht->entries[i].ObjectType = HANDLE_OBJECT_TYPE_FREE;
        ht->entries[i].AccessRights = 0U;
        ht->entries[i].Flags = 0U;
        ht->entries[i].RefCount = 0U;
    }
}

static void peb_init(PPEB peb) {
    memset(peb, 0, sizeof(PEB));

    peb->InheritedAddressSpace = FALSE;
    peb->ReadImageFileExecOptions = FALSE;
    peb->BeingDebugged = FALSE;
    peb->BitField = 0U;
    peb->SpareBool[0] = 0U;
    peb->Mutant = NULL;
    peb->ImageBaseAddress = 0ULL;
    peb->LoaderData = create_peb_ldr_data();
    peb->ProcessParameters = NULL;
    peb->SubSystemData = 0U;
    peb->ProcessHeap = 0x00100000U;
    peb->FastPebLock = 0ULL;
    peb->AtlThunkSListPtr = 0U;
    peb->IFEOKey = 0U;
    peb->CrossProcessFlags = 0U;
    peb->KernelCallbackTable = 0U;
    peb->SystemReserved[0] = 0U;
    peb->AtlThunkSListPtr32 = 0U;
    peb->ApiSetMap = 0U;
    peb->TlsExpansionCounter = 0U;
    peb->TlsBitmap = 0ULL;
    peb->TlsBitmapBits[0] = 0U;
    peb->TlsBitmapBits[1] = 0U;
    peb->ReadOnlySharedMemoryBase = 0U;
    peb->HotpatchInformation = 0U;
    peb->ReadOnlyStaticServerData = 0U;
    peb->AnsiCodePageData = 0U;
    peb->OemCodePageData = 0U;
    peb->UnicodeCaseTableData = 0U;
    peb->NumberOfProcessors = 1U;
    peb->NtGlobalFlag = 0U;
    peb->CriticalSectionTimeout = 0x00278D00ULL;
    peb->HeapSegmentReserve = 0x00100000ULL;
    peb->HeapSegmentCommit = 0x00002000ULL;
    peb->HeapDeCommitTotalFreeThreshold = 0x00010000ULL;
    peb->HeapDeCommitFreeBlockThreshold = 0x00001000ULL;
    peb->NumberOfHeaps = 1U;
    peb->MaximumNumberOfHeaps = 16U;
    peb->ProcessHeaps = 0U;
    peb->GdiSharedHandleTable = 0U;
    peb->ProcessStarterHelper = 0U;
    peb->GdiDCAttributeList = 0U;
    peb->LoaderLock = 0U;
    peb->OSMajorVersion = 10U;
    peb->OSMinorVersion = 0U;
    peb->OSBuildNumber = 19045U;
    peb->OSCSDVersion = 0U;
    peb->OSPlatformId = 2U;
    peb->ImageSubsystem = 2U;
    peb->ImageSubsystemMajorVersion = 10U;
    peb->ImageSubsystemMinorVersion = 0U;
    peb->ActiveProcessAffinityMask = 0x00000001ULL;
    peb->PostProcessInitRoutine = 0U;
    peb->TlsExpansionBitmap = 0U;
    peb->SessionId = 1U;
    peb->AppCompatFlags = 0ULL;
    peb->AppCompatFlagsUser = 0ULL;
    peb->pShimData = 0ULL;
    peb->AppCompatInfo = 0ULL;
    peb->CSDVersion.Length = 0;
    peb->CSDVersion.MaximumLength = 0;
    peb->CSDVersion.Buffer = NULL;
    peb->ActivationContextData = 0ULL;
    peb->ProcessAssemblyStorageMap = 0ULL;
    peb->SystemDefaultActivationContextData = 0ULL;
    peb->SystemAssemblyStorageMap = 0ULL;
    peb->MinimumStackCommit = 0x00001000ULL;
    peb->FlsCallback = 0ULL;
    list_init_head(&peb->FlsListHead);
    peb->FlsBitmap = 0ULL;
    peb->FlsBitmapBits[0] = 0U;
    peb->FlsBitmapBits[1] = 0U;
    peb->FlsBitmapBits[2] = 0U;
    peb->FlsBitmapBits[3] = 0U;
    peb->FlsHighIndex = 0U;
    peb->WerRegistrationData = 0ULL;
    peb->WerShipAssertPtr = 0ULL;
    peb->pUnused = 0ULL;
    peb->pImageHeaderHash = 0ULL;
    peb->TracingFlags = 0U;
}

static win32_process_t* find_free_process_slot(void) {
    uint32_t i;

    for (i = 0U; i < WIN32_MAX_PROCESSES; i++) {
        if (g_processes[i].pid == 0U && !g_processes[i].is_running) {
            return &g_processes[i];
        }
    }
    return NULL;
}

static win32_process_t* allocate_pid(uint32_t pid) {
    win32_process_t* proc;

    proc = find_free_process_slot();
    if (proc == NULL) {
        return NULL;
    }

    memset(proc, 0, sizeof(win32_process_t));
    proc->pid = pid;
    return proc;
}

static void process_slot_cleanup(win32_process_t* proc) {
    uint32_t i;

    if (proc == NULL) {
        return;
    }

    for (i = 0U; i < proc->thread_count; i++) {
        if (proc->threads[i] != NULL) {
            memory_free(proc->threads[i]);
            proc->threads[i] = NULL;
        }
    }
    proc->thread_count = 0U;

    if (proc->peb.LoaderData != NULL) {
        memory_free(proc->peb.LoaderData);
        proc->peb.LoaderData = NULL;
    }

    if (proc->peb.ProcessParameters != NULL) {
        if (proc->peb.ProcessParameters->CurrentDirectory.Buffer != NULL) {
            memory_free(proc->peb.ProcessParameters->CurrentDirectory.Buffer);
        }
        if (proc->peb.ProcessParameters->DllPath.Buffer != NULL) {
            memory_free(proc->peb.ProcessParameters->DllPath.Buffer);
        }
        if (proc->peb.ProcessParameters->ImagePathName.Buffer != NULL) {
            memory_free(proc->peb.ProcessParameters->ImagePathName.Buffer);
        }
        if (proc->peb.ProcessParameters->CommandLine.Buffer != NULL) {
            memory_free(proc->peb.ProcessParameters->CommandLine.Buffer);
        }
        memory_free(proc->peb.ProcessParameters);
        proc->peb.ProcessParameters = NULL;
    }
}

void win32_process_init(void) {
    win32_process_t* system_proc;
    win32_process_t* explorer_proc;
    uint32_t i;

    for (i = 0U; i < WIN32_MAX_PROCESSES; i++) {
        memset(&g_processes[i], 0, sizeof(win32_process_t));
    }
    g_current_process = NULL;
    g_current_thread = NULL;
    g_tls_bitmap = 0ULL;
    g_next_pid = 100U;

    system_proc = allocate_pid(4U);
    if (system_proc != NULL) {
        system_proc->parent_pid = 0U;
        strncpy(system_proc->image_path, "System", sizeof(system_proc->image_path) - 1);
        strncpy(system_proc->command_line, "System", sizeof(system_proc->command_line) - 1);
        strncpy(system_proc->current_dir, "C:\\", sizeof(system_proc->current_dir) - 1);
        peb_init(&system_proc->peb);
        handle_table_init(&system_proc->handle_table);
        system_proc->thread_count = 0U;
        system_proc->exit_code = 0U;
        system_proc->is_running = TRUE;
        system_proc->creation_time_low = 0U;
        system_proc->creation_time_high = 0U;
        system_proc->user_time = 0ULL;
        system_proc->kernel_time = 0ULL;
        system_proc->image_base = NULL;
        system_proc->image_size = 0U;
        system_proc->priority_class = 0x00000010U;
        system_proc->page_directory = NULL;
        system_proc->is_32bit_process = FALSE;
        system_proc->session_id = 0U;
        system_proc->wow64_process = FALSE;
    }

    explorer_proc = allocate_pid(4096U);
    if (explorer_proc != NULL) {
        explorer_proc->parent_pid = 4U;
        strncpy(explorer_proc->image_path, "C:\\Windows\\explorer.exe", sizeof(explorer_proc->image_path) - 1);
        strncpy(explorer_proc->command_line, "C:\\Windows\\explorer.exe", sizeof(explorer_proc->command_line) - 1);
        strncpy(explorer_proc->current_dir, "C:\\Windows", sizeof(explorer_proc->current_dir) - 1);
        peb_init(&explorer_proc->peb);
        explorer_proc->peb.ProcessParameters = create_process_parameters(
            explorer_proc->image_path,
            explorer_proc->command_line,
            explorer_proc->current_dir
        );
        handle_table_init(&explorer_proc->handle_table);
        explorer_proc->thread_count = 0U;
        explorer_proc->exit_code = 0U;
        explorer_proc->is_running = TRUE;
        explorer_proc->creation_time_low = 0U;
        explorer_proc->creation_time_high = 0U;
        explorer_proc->user_time = 0ULL;
        explorer_proc->kernel_time = 0ULL;
        explorer_proc->image_base = NULL;
        explorer_proc->image_size = 0U;
        explorer_proc->priority_class = 0x00000020U;
        explorer_proc->page_directory = NULL;
        explorer_proc->is_32bit_process = FALSE;
        explorer_proc->session_id = 1U;
        explorer_proc->wow64_process = FALSE;
    }

    if (system_proc != NULL) {
        g_current_process = system_proc;
    }
}

win32_process_t* win32_create_process(
    const char* image_path,
    const char* command_line,
    const char* current_dir
) {
    win32_process_t* proc;
    uint32_t pid;
    const char* effective_cmd;
    const char* effective_dir;

    if (image_path == NULL) {
        return NULL;
    }

    pid = g_next_pid;
    g_next_pid++;
    if (g_next_pid < 100U) {
        g_next_pid = 100U;
    }

    proc = allocate_pid(pid);
    if (proc == NULL) {
        return NULL;
    }

    proc->parent_pid = g_current_process != NULL ? g_current_process->pid : 4U;

    strncpy(proc->image_path, image_path, sizeof(proc->image_path) - 1);
    proc->image_path[sizeof(proc->image_path) - 1] = '\0';

    if (command_line != NULL) {
        effective_cmd = command_line;
    } else {
        effective_cmd = image_path;
    }
    strncpy(proc->command_line, effective_cmd, sizeof(proc->command_line) - 1);
    proc->command_line[sizeof(proc->command_line) - 1] = '\0';

    if (current_dir != NULL) {
        effective_dir = current_dir;
    } else {
        effective_dir = "C:\\";
    }
    strncpy(proc->current_dir, effective_dir, sizeof(proc->current_dir) - 1);
    proc->current_dir[sizeof(proc->current_dir) - 1] = '\0';

    peb_init(&proc->peb);
    proc->peb.ProcessParameters = create_process_parameters(
        proc->image_path,
        proc->command_line,
        proc->current_dir
    );

    handle_table_init(&proc->handle_table);

    proc->thread_count = 0U;
    proc->exit_code = 0U;
    proc->is_running = TRUE;
    proc->creation_time_low = 0U;
    proc->creation_time_high = 0U;
    proc->user_time = 0ULL;
    proc->kernel_time = 0ULL;
    proc->image_base = NULL;
    proc->image_size = 0U;
    proc->priority_class = 0x00000020U;
    proc->page_directory = NULL;
    proc->is_32bit_process = FALSE;
    proc->session_id = 1U;
    proc->wow64_process = FALSE;

    proc->peb.ImageBaseAddress = 0x0000000000400000ULL;

    return proc;
}

BOOL win32_terminate_process(uint32_t pid, uint32_t exit_code) {
    win32_process_t* proc;
    uint32_t i;

    proc = win32_get_process(pid);
    if (proc == NULL) {
        return FALSE;
    }

    proc->exit_code = exit_code;
    proc->is_running = FALSE;

    if (proc->handle_table.entries != NULL) {
        for (i = 0U; i < proc->handle_table.capacity; i++) {
            if (proc->handle_table.entries[i].ObjectType != HANDLE_OBJECT_TYPE_FREE) {
                proc->handle_table.entries[i].ObjectType = HANDLE_OBJECT_TYPE_FREE;
                proc->handle_table.entries[i].Object = NULL;
                proc->handle_table.entries[i].Handle = NULL;
                proc->handle_table.entries[i].RefCount = 0U;
                if (proc->handle_table.handle_count > 0U) {
                    proc->handle_table.handle_count--;
                }
            }
        }
    }

    for (i = 0U; i < proc->thread_count; i++) {
        if (proc->threads[i] != NULL) {
            if (g_current_thread == proc->threads[i]) {
                g_current_thread = NULL;
            }
            memory_free(proc->threads[i]);
            proc->threads[i] = NULL;
        }
    }
    proc->thread_count = 0U;

    if (g_current_process == proc) {
        g_current_process = &g_processes[0];
        g_current_thread = NULL;
    }

    return TRUE;
}

win32_process_t* win32_get_process(uint32_t pid) {
    uint32_t i;

    if (pid == 0U) {
        return NULL;
    }

    for (i = 0U; i < WIN32_MAX_PROCESSES; i++) {
        if (g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }
    return NULL;
}

TEB* win32_create_thread(
    win32_process_t* process,
    uint32_t (*start)(void*),
    void* param
) {
    TEB* teb;

    (void)start;
    (void)param;

    if (process == NULL) {
        return NULL;
    }

    if (process->thread_count >= WIN32_MAX_THREADS_PER_PROCESS) {
        return NULL;
    }

    teb = (TEB*)memory_alloc(sizeof(TEB));
    if (teb == NULL) {
        return NULL;
    }
    memset(teb, 0, sizeof(TEB));

    teb->ExceptionList = 0xFFFFFFFFFFFFFFFFULL;
    teb->StackBase = 0ULL;
    teb->StackLimit = 0ULL;
    teb->SubSystemTib = 0ULL;
    teb->FiberData = 0ULL;
    teb->ArbitraryUserPointer = 0ULL;
    teb->Self = teb;

    teb->EnvironmentPointer = NULL;
    teb->ClientId_UniqueProcess = process->pid;
    teb->ClientId_UniqueThread = process->thread_count + 1U;

    teb->ActiveRpcHandle = 0U;
    teb->ThreadLocalStoragePointer = (uint32_t)((uint64_t)&teb->TlsSlots[0]);

    teb->Peb = &process->peb;

    teb->LastErrorValue = 0U;
    teb->CountOfOwnedCriticalSections = 0U;
    teb->CsrClientThread = 0U;
    teb->Win32ThreadInfo = 0U;

    list_init_head(&teb->TlsLinks);
    list_init_head(&teb->TlsExpansionLinks);

    process->threads[process->thread_count] = teb;
    process->thread_count++;

    return teb;
}

BOOL win32_destroy_thread(TEB* teb) {
    win32_process_t* owner;
    uint32_t i;
    uint32_t j;
    BOOL found;

    if (teb == NULL) {
        return FALSE;
    }

    owner = win32_get_process(teb->ClientId_UniqueProcess);
    if (owner == NULL) {
        memory_free(teb);
        return TRUE;
    }

    found = FALSE;
    for (i = 0U; i < owner->thread_count; i++) {
        if (owner->threads[i] == teb) {
            found = TRUE;
            for (j = i; j < owner->thread_count - 1U; j++) {
                owner->threads[j] = owner->threads[j + 1U];
            }
            owner->threads[owner->thread_count - 1U] = NULL;
            owner->thread_count--;
            break;
        }
    }

    if (g_current_thread == teb) {
        if (owner->thread_count > 0U) {
            g_current_thread = owner->threads[0];
        } else {
            g_current_thread = NULL;
        }
    }

    (void)found;
    memory_free(teb);
    return TRUE;
}

uint32_t TlsAlloc(void) {
    uint32_t i;

    for (i = 0U; i < 64U; i++) {
        if ((g_tls_bitmap & (1ULL << i)) == 0ULL) {
            g_tls_bitmap |= (1ULL << i);
            return i;
        }
    }
    return TLS_OUT_OF_INDEXES;
}

BOOL TlsFree(uint32_t dwTlsIndex) {
    if (dwTlsIndex >= 64U) {
        return FALSE;
    }

    g_tls_bitmap &= ~(1ULL << dwTlsIndex);
    return TRUE;
}

void* TlsGetValue(uint32_t dwTlsIndex) {
    if (g_current_thread == NULL) {
        return NULL;
    }

    if (dwTlsIndex < 64U) {
        return (void*)g_current_thread->TlsSlots[dwTlsIndex];
    }

    if (dwTlsIndex < (64U + 1024U)) {
        return (void*)g_current_thread->TlsSlots_Expansion[dwTlsIndex - 64U];
    }

    return NULL;
}

BOOL TlsSetValue(uint32_t dwTlsIndex, void* lpTlsValue) {
    if (g_current_thread == NULL) {
        return FALSE;
    }

    if (dwTlsIndex < 64U) {
        g_current_thread->TlsSlots[dwTlsIndex] = (uint64_t)lpTlsValue;
        return TRUE;
    }

    if (dwTlsIndex < (64U + 1024U)) {
        g_current_thread->TlsSlots_Expansion[dwTlsIndex - 64U] = (uint64_t)lpTlsValue;
        return TRUE;
    }

    return FALSE;
}

HANDLE win32_handle_alloc(
    void* object,
    uint32_t type,
    uint32_t access
) {
    win32_process_t* proc;
    handle_table_t* ht;
    uint32_t index;
    uint32_t i;

    if (g_current_process == NULL) {
        proc = &g_processes[0];
    } else {
        proc = g_current_process;
    }
    ht = &proc->handle_table;
    if (ht->entries == NULL || ht->capacity == 0U) {
        return NULL;
    }

    index = (uint32_t)((uintptr_t)ht->next_handle_value >> 2);
    for (i = 0U; i < ht->capacity; i++) {
        index = (index + i) % ht->capacity;
        if (ht->entries[index].ObjectType == HANDLE_OBJECT_TYPE_FREE) {
            break;
        }
    }

    if (index >= ht->capacity || ht->entries[index].ObjectType != HANDLE_OBJECT_TYPE_FREE) {
        return NULL;
    }

    ht->entries[index].Handle = (HANDLE)((uint64_t)index << 2);
    ht->entries[index].Object = object;
    ht->entries[index].ObjectType = type;
    ht->entries[index].AccessRights = access;
    ht->entries[index].Flags = 0U;
    ht->entries[index].RefCount = 1U;

    ht->next_handle_value = (uint32_t)(((uint64_t)index + 1U) << 2);
    if (ht->next_handle_value < 4U) {
        ht->next_handle_value = 4U;
    }
    ht->handle_count++;

    return ht->entries[index].Handle;
}

void* win32_handle_to_object(HANDLE h, uint32_t expect_type) {
    win32_process_t* proc;
    handle_table_t* ht;
    uint32_t index;

    if (g_current_process == NULL) {
        proc = &g_processes[0];
    } else {
        proc = g_current_process;
    }
    ht = &proc->handle_table;

    if (h == NULL || ht->entries == NULL || ht->capacity == 0U) {
        return NULL;
    }

    index = (uint32_t)((uint64_t)h >> 2);
    if (index >= ht->capacity) {
        return NULL;
    }

    if (ht->entries[index].ObjectType == HANDLE_OBJECT_TYPE_FREE) {
        return NULL;
    }

    if (ht->entries[index].Handle != h) {
        return NULL;
    }

    if (expect_type != 0U && ht->entries[index].ObjectType != expect_type) {
        return NULL;
    }

    return ht->entries[index].Object;
}

BOOL win32_handle_close(HANDLE h) {
    win32_process_t* proc;
    handle_table_t* ht;
    uint32_t index;

    if (g_current_process == NULL) {
        proc = &g_processes[0];
    } else {
        proc = g_current_process;
    }
    ht = &proc->handle_table;

    if (h == NULL || ht->entries == NULL || ht->capacity == 0U) {
        return FALSE;
    }

    index = (uint32_t)((uint64_t)h >> 2);
    if (index >= ht->capacity) {
        return FALSE;
    }

    if (ht->entries[index].ObjectType == HANDLE_OBJECT_TYPE_FREE) {
        return FALSE;
    }

    if (ht->entries[index].Handle != h) {
        return FALSE;
    }

    if (ht->entries[index].Flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) {
        return FALSE;
    }

    if (ht->entries[index].RefCount > 0U) {
        ht->entries[index].RefCount--;
    }

    if (ht->entries[index].RefCount == 0U) {
        ht->entries[index].Handle = NULL;
        ht->entries[index].Object = NULL;
        ht->entries[index].ObjectType = HANDLE_OBJECT_TYPE_FREE;
        ht->entries[index].AccessRights = 0U;
        ht->entries[index].Flags = 0U;
        if (ht->handle_count > 0U) {
            ht->handle_count--;
        }
    }

    return TRUE;
}

uint32_t win32_handle_duplicate(HANDLE src) {
    win32_process_t* proc;
    handle_table_t* ht;
    uint32_t src_index;
    uint32_t dst_index;
    uint32_t i;

    if (g_current_process == NULL) {
        proc = &g_processes[0];
    } else {
        proc = g_current_process;
    }
    ht = &proc->handle_table;

    if (src == NULL || ht->entries == NULL || ht->capacity == 0U) {
        return 0U;
    }

    src_index = (uint32_t)((uint64_t)src >> 2);
    if (src_index >= ht->capacity) {
        return 0U;
    }

    if (ht->entries[src_index].ObjectType == HANDLE_OBJECT_TYPE_FREE) {
        return 0U;
    }

    if (ht->entries[src_index].Handle != src) {
        return 0U;
    }

    dst_index = (uint32_t)((uintptr_t)ht->next_handle_value >> 2);
    for (i = 0U; i < ht->capacity; i++) {
        dst_index = (dst_index + i) % ht->capacity;
        if (ht->entries[dst_index].ObjectType == HANDLE_OBJECT_TYPE_FREE) {
            break;
        }
    }

    if (dst_index >= ht->capacity || ht->entries[dst_index].ObjectType != HANDLE_OBJECT_TYPE_FREE) {
        return 0U;
    }

    ht->entries[dst_index].Handle = (HANDLE)((uint64_t)dst_index << 2);
    ht->entries[dst_index].Object = ht->entries[src_index].Object;
    ht->entries[dst_index].ObjectType = ht->entries[src_index].ObjectType;
    ht->entries[dst_index].AccessRights = ht->entries[src_index].AccessRights;
    ht->entries[dst_index].Flags = ht->entries[src_index].Flags;
    ht->entries[dst_index].RefCount = 1U;

    ht->next_handle_value = (uint32_t)(((uint64_t)dst_index + 1U) << 2);
    if (ht->next_handle_value < 4U) {
        ht->next_handle_value = 4U;
    }
    ht->handle_count++;

    return (uint32_t)((uintptr_t)ht->entries[dst_index].Handle);
}

NTSTATUS NtClose(HANDLE Handle) {
    if (!win32_handle_close(Handle)) {
        return STATUS_INVALID_HANDLE;
    }
    return STATUS_SUCCESS;
}

NTSTATUS NtAllocateVirtualMemory(
    HANDLE ProcessHandle,
    void** BaseAddress,
    ULONG_PTR ZeroBits,
    PSIZE_T RegionSize,
    ULONG AllocationType,
    ULONG Protect
) {
    (void)ProcessHandle;
    (void)ZeroBits;
    (void)AllocationType;
    (void)Protect;

    if (BaseAddress == NULL || RegionSize == NULL) {
        return STATUS_INVALID_HANDLE;
    }

    if (*BaseAddress == NULL) {
        *BaseAddress = memory_alloc(*RegionSize);
        if (*BaseAddress == NULL) {
            return STATUS_ACCESS_DENIED;
        }
        memset(*BaseAddress, 0, (size_t)*RegionSize);
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtFreeVirtualMemory(
    HANDLE ProcessHandle,
    void** BaseAddress,
    PSIZE_T RegionSize,
    ULONG FreeType
) {
    (void)ProcessHandle;
    (void)FreeType;
    (void)RegionSize;

    if (BaseAddress == NULL) {
        return STATUS_INVALID_HANDLE;
    }

    if (*BaseAddress != NULL) {
        memory_free(*BaseAddress);
        *BaseAddress = NULL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtQueryInformationProcess(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    void* ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
) {
    (void)ProcessHandle;
    (void)ProcessInformationClass;

    if (ProcessInformation == NULL) {
        if (ReturnLength != NULL) {
            *ReturnLength = 0U;
        }
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    if (ProcessInformationLength < 4U) {
        if (ReturnLength != NULL) {
            *ReturnLength = 0U;
        }
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    memset(ProcessInformation, 0, ProcessInformationLength);

    if (ReturnLength != NULL) {
        *ReturnLength = ProcessInformationLength;
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtQueryInformationThread(
    HANDLE ThreadHandle,
    ULONG ThreadInformationClass,
    void* ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
) {
    (void)ThreadHandle;
    (void)ThreadInformationClass;

    if (ThreadInformation == NULL) {
        if (ReturnLength != NULL) {
            *ReturnLength = 0U;
        }
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    if (ThreadInformationLength < 4U) {
        if (ReturnLength != NULL) {
            *ReturnLength = 0U;
        }
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    memset(ThreadInformation, 0, ThreadInformationLength);

    if (ReturnLength != NULL) {
        *ReturnLength = ThreadInformationLength;
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtCreateFile(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength
) {
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
        return STATUS_INVALID_HANDLE;
    }

    *FileHandle = win32_handle_alloc(NULL, HANDLE_OBJECT_TYPE_FILE, DesiredAccess);
    if (*FileHandle == NULL) {
        return STATUS_ACCESS_DENIED;
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtReadFile(
    HANDLE FileHandle,
    HANDLE Event,
    PVOID ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key
) {
    void* obj;

    (void)Event;
    (void)ApcRoutine;
    (void)ApcContext;
    (void)IoStatusBlock;
    (void)ByteOffset;
    (void)Key;

    obj = win32_handle_to_object(FileHandle, HANDLE_OBJECT_TYPE_FILE);
    if (FileHandle == NULL || obj == NULL) {
        if ((uint64_t)FileHandle >> 2 >= HANDLE_TABLE_SIZE || FileHandle == NULL) {
            return STATUS_INVALID_HANDLE;
        }
        if (win32_handle_to_object(FileHandle, 0U) == NULL) {
            return STATUS_INVALID_HANDLE;
        }
    }

    if (Buffer != NULL) {
        memset(Buffer, 0, Length);
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtWriteFile(
    HANDLE FileHandle,
    HANDLE Event,
    PVOID ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    const VOID* Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key
) {
    void* obj;

    (void)Event;
    (void)ApcRoutine;
    (void)ApcContext;
    (void)IoStatusBlock;
    (void)Buffer;
    (void)Length;
    (void)ByteOffset;
    (void)Key;

    obj = win32_handle_to_object(FileHandle, HANDLE_OBJECT_TYPE_FILE);
    if (FileHandle == NULL || obj == NULL) {
        if ((uint64_t)FileHandle >> 2 >= HANDLE_TABLE_SIZE || FileHandle == NULL) {
            return STATUS_INVALID_HANDLE;
        }
        if (win32_handle_to_object(FileHandle, 0U) == NULL) {
            return STATUS_INVALID_HANDLE;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS NtQueryAttributesFile(
    void* ObjectAttributes,
    void* FileInformation
) {
    (void)ObjectAttributes;

    if (FileInformation == NULL) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    memset(FileInformation, 0, 64);

    return STATUS_SUCCESS;
}

#undef ZwClose
#undef ZwAllocateVirtualMemory
#undef ZwFreeVirtualMemory
#undef ZwQueryInformationProcess
#undef ZwQueryInformationThread
#undef ZwCreateFile
#undef ZwReadFile
#undef ZwWriteFile
#undef ZwQueryAttributesFile

uint32_t ZwClose(HANDLE Handle) {
    return NtClose(Handle);
}

uint32_t ZwAllocateVirtualMemory(
    HANDLE ProcessHandle,
    void** BaseAddress,
    uint32_t ZeroBits,
    PSIZE_T RegionSize,
    uint32_t AllocationType,
    uint32_t Protect
) {
    return NtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
}

uint32_t ZwFreeVirtualMemory(
    HANDLE ProcessHandle,
    void** BaseAddress,
    PSIZE_T RegionSize,
    uint32_t FreeType
) {
    return NtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, FreeType);
}

uint32_t ZwQueryInformationProcess(
    HANDLE ProcessHandle,
    uint32_t ProcessInformationClass,
    void* ProcessInformation,
    uint32_t ProcessInformationLength,
    PULONG ReturnLength
) {
    return NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
}

uint32_t ZwQueryInformationThread(
    HANDLE ThreadHandle,
    uint32_t ThreadInformationClass,
    void* ThreadInformation,
    uint32_t ThreadInformationLength,
    PULONG ReturnLength
) {
    return NtQueryInformationThread(ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength);
}

uint32_t ZwCreateFile(
    HANDLE* FileHandle,
    uint32_t DesiredAccess,
    void* ObjectAttributes,
    void* IoStatusBlock,
    int64_t* AllocationSize,
    uint32_t FileAttributes,
    uint32_t ShareAccess,
    uint32_t CreateDisposition,
    uint32_t CreateOptions,
    void* EaBuffer,
    uint32_t EaLength
) {
    return NtCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
}

uint32_t ZwReadFile(
    HANDLE FileHandle,
    HANDLE Event,
    void* ApcRoutine,
    void* ApcContext,
    void* IoStatusBlock,
    void* Buffer,
    uint32_t Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key
) {
    return NtReadFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);
}

uint32_t ZwWriteFile(
    HANDLE FileHandle,
    HANDLE Event,
    void* ApcRoutine,
    void* ApcContext,
    void* IoStatusBlock,
    const void* Buffer,
    uint32_t Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key
) {
    return NtWriteFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);
}

uint32_t ZwQueryAttributesFile(
    void* ObjectAttributes,
    void* FileInformation
) {
    return NtQueryAttributesFile(ObjectAttributes, FileInformation);
}

PPEB NtCurrentPeb(void) {
    if (g_current_thread != NULL) {
        return g_current_thread->Peb;
    }
    return &g_processes[0].peb;
}

PTEB NtCurrentTeb(void) {
    return g_current_thread;
}

void win32_switch_to_process(uint32_t pid) {
    win32_process_t* proc;

    proc = win32_get_process(pid);
    if (proc == NULL) {
        return;
    }

    g_current_process = proc;

    if (proc->thread_count > 0U && proc->threads[0] != NULL) {
        g_current_thread = proc->threads[0];
    } else {
        g_current_thread = NULL;
    }
}
