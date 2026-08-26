#include <arch/win32.h>
#include <arch/slab.h>
#include <arch/registry.h>
#include <string.h>
extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);

#define WBEM_NO_ERROR            0x00000000
#define WBEM_S_FALSE             0x00000001
#define WBEM_S_NO_MORE_DATA      0x40001
#define WBEM_E_FAILED            0x80041001u
#define WBEM_E_NOT_FOUND         0x80041002u
#define WBEM_E_ACCESS_DENIED     0x80041003u
#define WBEM_E_PROVIDER_FAILURE  0x80041004u
#define WBEM_E_TYPE_MISMATCH     0x80041005u
#define WBEM_E_OUT_OF_MEMORY     0x80041006u
#define WBEM_E_INVALID_CLASS     0x80041010u
#define WBEM_E_INVALID_NAMESPACE 0x8004100Eu
#define WBEM_E_INVALID_PARAMETER 0x80041008u
#define WBEM_E_INVALID_QUERY     0x80041017u
#define WBEM_E_NOT_SUPPORTED     0x8004100Cu

typedef enum _CIM_TYPES {
    CIM_EMPTY=0,
    CIM_SINT8=16, CIM_UINT8=17,
    CIM_SINT16=2, CIM_UINT16=18,
    CIM_SINT32=3, CIM_UINT32=19,
    CIM_SINT64=20, CIM_UINT64=21,
    CIM_REAL32=4, CIM_REAL64=5,
    CIM_BOOLEAN=11,
    CIM_STRING=8,
    CIM_DATETIME=101,
    CIM_REFERENCE=102,
    CIM_CHAR16=103,
    CIM_OBJECT=13,
    CIM_FLAG_ARRAY=0x2000
} CIMTYPE_ENUM;

typedef struct _wmi_property_t {
    char name[128];
    CIMTYPE_ENUM type;
    union {
        uint8_t u8; int8_t i8;
        uint16_t u16; int16_t i16;
        uint32_t u32; int32_t i32;
        uint64_t u64; int64_t i64;
        float f32; double f64;
        BOOL boolean;
        char string_value[1024];
    } v;
    uint32_t array_size;
    void* array_data;
    struct _wmi_property_t* next;
} wmi_property_t;

typedef struct _wmi_class_instance_t {
    char class_name[128];
    char namespace_[128];
    wmi_property_t* first_property;
    uint32_t prop_count;
    struct _wmi_class_instance_t* next;
    struct _wmi_class_instance_t* next_instance;
} wmi_instance_t;

typedef struct _wmi_enumerator_t {
    uint32_t signature;
    char namespace_[128];
    char class_filter[128];
    wmi_instance_t* current;
} wmi_enum_t;

#define WMI_MAX_INSTANCES 512
static wmi_instance_t g_wmi_instances[WMI_MAX_INSTANCES];
static uint32_t g_wmi_inuse = 0;
static BOOL g_wmi_init = FALSE;

static wmi_instance_t* wmi_create_instance(const char* ns, const char* classname) {
    wmi_instance_t* inst;
    size_t nlen;
    if (g_wmi_inuse >= WMI_MAX_INSTANCES) return NULL;
    inst = &g_wmi_instances[g_wmi_inuse];
    memset(inst, 0, sizeof(wmi_instance_t));
    nlen = strlen(ns);
    if (nlen >= sizeof(inst->namespace_)) nlen = sizeof(inst->namespace_) - 1u;
    memcpy(inst->namespace_, ns, nlen);
    inst->namespace_[nlen] = 0;
    nlen = strlen(classname);
    if (nlen >= sizeof(inst->class_name)) nlen = sizeof(inst->class_name) - 1u;
    memcpy(inst->class_name, classname, nlen);
    inst->class_name[nlen] = 0;
    g_wmi_inuse++;
    return inst;
}

static wmi_property_t* wmi_add_property_base(wmi_instance_t* i, const char* name, CIMTYPE_ENUM type) {
    wmi_property_t* prop;
    wmi_property_t* last;
    size_t nlen;
    if (i == NULL || name == NULL) return NULL;
    prop = (wmi_property_t*)memory_alloc(sizeof(wmi_property_t));
    if (prop == NULL) return NULL;
    memset(prop, 0, sizeof(wmi_property_t));
    nlen = strlen(name);
    if (nlen >= sizeof(prop->name)) nlen = sizeof(prop->name) - 1u;
    memcpy(prop->name, name, nlen);
    prop->name[nlen] = 0;
    prop->type = type;
    prop->next = NULL;
    if (i->first_property == NULL) {
        i->first_property = prop;
    } else {
        last = i->first_property;
        while (last->next != NULL) last = last->next;
        last->next = prop;
    }
    i->prop_count++;
    return prop;
}

static void wmi_add_property_str(wmi_instance_t* i, const char* name, const char* val) {
    wmi_property_t* p = wmi_add_property_base(i, name, CIM_STRING);
    size_t vlen;
    if (p == NULL || val == NULL) return;
    vlen = strlen(val);
    if (vlen >= sizeof(p->v.string_value)) vlen = sizeof(p->v.string_value) - 1u;
    memcpy(p->v.string_value, val, vlen);
    p->v.string_value[vlen] = 0;
}

static void wmi_add_property_u32(wmi_instance_t* i, const char* name, uint32_t val) {
    wmi_property_t* p = wmi_add_property_base(i, name, CIM_UINT32);
    if (p == NULL) return;
    p->v.u32 = val;
}

static void wmi_add_property_u64(wmi_instance_t* i, const char* name, uint64_t val) {
    wmi_property_t* p = wmi_add_property_base(i, name, CIM_UINT64);
    if (p == NULL) return;
    p->v.u64 = val;
}

static void wmi_add_property_bool(wmi_instance_t* i, const char* name, BOOL val) {
    wmi_property_t* p = wmi_add_property_base(i, name, CIM_BOOLEAN);
    if (p == NULL) return;
    p->v.boolean = val;
}

static void wmi_add_property_u16(wmi_instance_t* i, const char* name, uint16_t val) {
    wmi_property_t* p = wmi_add_property_base(i, name, CIM_UINT16);
    if (p == NULL) return;
    p->v.u16 = val;
}

extern uint64_t kapi_get_total_memory(void);
extern uint64_t kapi_get_free_memory(void);

static uint64_t safe_total_mem(void) {
    uint64_t t = 0;
    uint64_t (*fn)(void) = kapi_get_total_memory;
    if (fn != NULL) {
        t = fn();
    }
    if (t == 0) t = 0x40000000ull;
    return t;
}

static uint64_t safe_free_mem(void) {
    uint64_t t = 0;
    uint64_t (*fn)(void) = kapi_get_free_memory;
    if (fn != NULL) {
        t = fn();
    }
    if (t == 0) t = 0x30000000ull;
    return t;
}

int wmi_init(void) {
    wmi_instance_t* inst;
    uint64_t total_mem;
    uint64_t free_mem;
    uint32_t total_mem_kb;
    uint32_t free_mem_kb;
    if (g_wmi_init) return 0;
    memset(g_wmi_instances, 0, sizeof(g_wmi_instances));
    g_wmi_inuse = 0;
    total_mem = safe_total_mem();
    free_mem = safe_free_mem();
    total_mem_kb = (uint32_t)(total_mem / 1024ull);
    free_mem_kb = (uint32_t)(free_mem / 1024ull);

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_OperatingSystem");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Caption", "KenuxOS Windows Compatible");
        wmi_add_property_str(inst, "Organization", "Kenux");
        wmi_add_property_str(inst, "BuildNumber", "26000");
        wmi_add_property_str(inst, "Version", "10.0.26000");
        wmi_add_property_str(inst, "SerialNumber", "00000-00000-00000-AAAAA");
        wmi_add_property_u64(inst, "InstallDate", 0ull);
        wmi_add_property_u64(inst, "LastBootUpTime", 0ull);
        wmi_add_property_u64(inst, "LocalDateTime", 0ull);
        wmi_add_property_str(inst, "SystemDirectory", "C:\\Windows\\system32");
        wmi_add_property_str(inst, "WindowsDirectory", "C:\\Windows");
        wmi_add_property_str(inst, "SystemDevice", "\\Device\\Harddisk0\\Partition1");
        wmi_add_property_str(inst, "BootDevice", "\\Device\\HarddiskVolume2");
        wmi_add_property_str(inst, "CountryCode", "1");
        wmi_add_property_u32(inst, "CurrentTimeZone", 480u);
        wmi_add_property_str(inst, "CodeSet", "936");
        wmi_add_property_str(inst, "Locale", "0804");
        wmi_add_property_u32(inst, "FreePhysicalMemory", free_mem_kb);
        wmi_add_property_u32(inst, "TotalVisibleMemorySize", total_mem_kb);
        wmi_add_property_u32(inst, "FreeVirtualMemory", free_mem_kb);
        wmi_add_property_u32(inst, "TotalVirtualMemorySize", total_mem_kb);
        wmi_add_property_u32(inst, "NumberOfLicensedUsers", 5u);
        wmi_add_property_u32(inst, "NumberOfProcesses", 128u);
        wmi_add_property_u32(inst, "NumberOfUsers", 1u);
        wmi_add_property_u32(inst, "MaxNumberOfProcesses", 0xFFFFFFFFu);
        wmi_add_property_u32(inst, "OSLanguage", 2052u);
        wmi_add_property_u32(inst, "SuiteMask", 0x0112u);
        wmi_add_property_u32(inst, "ProductType", 1u);
        wmi_add_property_bool(inst, "PAEEnabled", TRUE);
        wmi_add_property_bool(inst, "DataExecutionPrevention_Available", TRUE);
        wmi_add_property_bool(inst, "DataExecutionPrevention_Drivers", TRUE);
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_ComputerSystem");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Name", "KENUX-PC");
        wmi_add_property_str(inst, "Manufacturer", "Kenux");
        wmi_add_property_str(inst, "Model", "Virtual Machine");
        wmi_add_property_u64(inst, "TotalPhysicalMemory", total_mem);
        wmi_add_property_u32(inst, "NumberOfProcessors", 1u);
        wmi_add_property_u32(inst, "NumberOfLogicalProcessors", 1u);
        wmi_add_property_str(inst, "UserName", "KENUX-PC\\User");
        wmi_add_property_str(inst, "Domain", "WORKGROUP");
        wmi_add_property_str(inst, "Workgroup", "WORKGROUP");
        wmi_add_property_str(inst, "PrimaryOwnerName", "Kenux User");
        wmi_add_property_str(inst, "SystemType", "x64-based PC");
        wmi_add_property_str(inst, "BootState", "Normal boot");
        wmi_add_property_str(inst, "Status", "OK");
        wmi_add_property_bool(inst, "DaylightInEffect", FALSE);
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_Processor");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Name", "Virtual Intel64 Family 6 Model 158 Stepping 9");
        wmi_add_property_str(inst, "Description", "Central Processor");
        wmi_add_property_str(inst, "Manufacturer", "GenuineIntel");
        wmi_add_property_u16(inst, "Architecture", 9u);
        wmi_add_property_u16(inst, "Family", 6u);
        wmi_add_property_u16(inst, "Model", 158u);
        wmi_add_property_str(inst, "Stepping", "9");
        wmi_add_property_u32(inst, "MaxClockSpeed", 2400u);
        wmi_add_property_u32(inst, "CurrentClockSpeed", 2400u);
        wmi_add_property_u32(inst, "NumberOfCores", 1u);
        wmi_add_property_u32(inst, "NumberOfLogicalProcessors", 1u);
        wmi_add_property_str(inst, "ProcessorId", "FEFEFEFE00000000");
        wmi_add_property_u32(inst, "L2CacheSize", 256u);
        wmi_add_property_u32(inst, "L3CacheSize", 8192u);
        wmi_add_property_u16(inst, "AddressWidth", 64u);
        wmi_add_property_u16(inst, "DataWidth", 64u);
        wmi_add_property_u16(inst, "LoadPercentage", 5u);
        wmi_add_property_bool(inst, "PowerManagementSupported", TRUE);
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_PhysicalMemory");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Tag", "Physical Memory 0");
        wmi_add_property_str(inst, "BankLabel", "BANK 0");
        wmi_add_property_str(inst, "DeviceLocator", "DIMM0");
        wmi_add_property_u64(inst, "Capacity", total_mem);
        wmi_add_property_u32(inst, "Speed", 2133u);
        wmi_add_property_u16(inst, "MemoryType", 24u);
        wmi_add_property_u32(inst, "ConfiguredClockSpeed", 2133u);
        wmi_add_property_u32(inst, "InterleaveDataDepth", 1u);
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_BIOS");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Name", "OVMF 2024");
        wmi_add_property_str(inst, "Manufacturer", "Kenux");
        wmi_add_property_str(inst, "Version", "OVMF.Kenux.2024");
        wmi_add_property_u64(inst, "ReleaseDate", 0x07E20101000000ULL);
        wmi_add_property_str(inst, "SerialNumber", "KENUXBIOS001");
        wmi_add_property_str(inst, "SMBIOSBIOSVersion", "2.8");
        wmi_add_property_u16(inst, "SMBIOSMajorVersion", 2u);
        wmi_add_property_u16(inst, "SMBIOSMinorVersion", 8u);
        wmi_add_property_str(inst, "BIOSVersion", "Kenux OVMF 2.8");
        wmi_add_property_str(inst, "Status", "OK");
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_DiskDrive");
    if (inst != NULL) {
        wmi_add_property_str(inst, "DeviceID", "\\\\.\\PHYSICALDRIVE0");
        wmi_add_property_str(inst, "Model", "VIRTUAL HD");
        wmi_add_property_str(inst, "InterfaceType", "VIRTIO");
        wmi_add_property_str(inst, "MediaType", "Fixed hard disk media");
        wmi_add_property_u64(inst, "Size", 4294967296ull);
        wmi_add_property_u32(inst, "TotalCylinders", 522u);
        wmi_add_property_u32(inst, "TotalHeads", 255u);
        wmi_add_property_u64(inst, "TotalSectors", 8388608ull);
        wmi_add_property_u32(inst, "TotalTracks", 133110u);
        wmi_add_property_u32(inst, "SectorsPerTrack", 63u);
        wmi_add_property_u32(inst, "BytesPerSector", 512u);
        wmi_add_property_str(inst, "FirmwareRevision", "1.0");
        wmi_add_property_str(inst, "SerialNumber", "KENUXHD001");
        wmi_add_property_str(inst, "Status", "OK");
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_LogicalDisk");
    if (inst != NULL) {
        wmi_add_property_str(inst, "DeviceID", "C:");
        wmi_add_property_str(inst, "VolumeName", "System");
        wmi_add_property_u32(inst, "DriveType", 3u);
        wmi_add_property_str(inst, "FileSystem", "NTFS");
        wmi_add_property_u64(inst, "Size", 4294967296ull);
        wmi_add_property_u64(inst, "FreeSpace", 3221225472ull);
        wmi_add_property_str(inst, "VolumeSerialNumber", "0x12345678");
        wmi_add_property_bool(inst, "SupportsFileBasedCompression", FALSE);
        wmi_add_property_bool(inst, "SupportsDiskQuotas", TRUE);
        wmi_add_property_bool(inst, "QuotasDisabled", TRUE);
        wmi_add_property_bool(inst, "SystemVolume", FALSE);
        wmi_add_property_bool(inst, "BootVolume", TRUE);
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_VideoController");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Name", "Standard VGA Graphics Adapter");
        wmi_add_property_str(inst, "AdapterCompatibility", "Kenux");
        wmi_add_property_str(inst, "VideoProcessor", "GOP Framebuffer");
        wmi_add_property_str(inst, "DriverVersion", "10.0.26000.0");
        wmi_add_property_str(inst, "DriverDate", "20240101000000.000000+000");
        wmi_add_property_u32(inst, "AdapterRAM", 268435456u);
        wmi_add_property_u32(inst, "CurrentHorizontalResolution", 1024u);
        wmi_add_property_u32(inst, "CurrentVerticalResolution", 768u);
        wmi_add_property_u16(inst, "CurrentBitsPerPixel", 32u);
        wmi_add_property_u32(inst, "CurrentNumberOfColors", 16777216u);
        wmi_add_property_u32(inst, "CurrentRefreshRate", 60u);
        wmi_add_property_u32(inst, "MinRefreshRate", 50u);
        wmi_add_property_u32(inst, "MaxRefreshRate", 100u);
        wmi_add_property_str(inst, "VideoModeDescription", "1024 x 768 x 4294967296 colors");
        wmi_add_property_str(inst, "Status", "OK");
    }

    inst = wmi_create_instance("ROOT\\CIMV2", "Win32_NetworkAdapter");
    if (inst != NULL) {
        wmi_add_property_str(inst, "Name", "KenuxK VirtIO Ethernet Adapter");
        wmi_add_property_str(inst, "Description", "VirtIO Ethernet Connection");
        wmi_add_property_str(inst, "Manufacturer", "Red Hat, Inc.");
        wmi_add_property_str(inst, "AdapterType", "Ethernet 802.3");
        wmi_add_property_str(inst, "MACAddress", "52-54-00-12-34-56");
        wmi_add_property_u64(inst, "Speed", 1000000000ull);
        wmi_add_property_u32(inst, "InterfaceIndex", 1u);
        wmi_add_property_str(inst, "GUID", "{A32942B7-920C-486B-B09B-08002B27B33D}");
        wmi_add_property_str(inst, "NetConnectionID", "Ethernet");
        wmi_add_property_u16(inst, "NetConnectionStatus", 2u);
        wmi_add_property_u16(inst, "AdminStatus", 2u);
        wmi_add_property_str(inst, "PNPDeviceID", "PCI\\VEN_1AF4&DEV_1000&SUBSYS_00010000&REV_00");
        wmi_add_property_bool(inst, "Installed", TRUE);
        wmi_add_property_bool(inst, "PhysicalAdapter", TRUE);
        wmi_add_property_bool(inst, "PowerManagementSupported", TRUE);
    }

    {
        typedef struct { const char* nm; const char* dn; uint32_t stm; const char* path; } svc_map_t;
        static const svc_map_t svcs[] = {
            {"EventLog",  "Event Log",                   2u, "C:\\Windows\\system32\\services.exe"},
            {"Dhcp",      "DHCP Client",                 2u, "%SystemRoot%\\system32\\drivers\\dhcp.sys"},
            {"Winmgmt",   "WMI",                         2u, "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
            {"Themes",    "Themes",                      2u, "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
            {"Schedule",  "Task Scheduler",              2u, "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
            {"W32Time",   "Windows Time",                2u, "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
            {"Audiosrv",  "Windows Audio",               2u, "C:\\Windows\\system32\\svchost.exe -k netsvcs"},
            {"PlugPlay",  "Plug and Play",               2u, "%SystemRoot%\\system32\\drivers\\plugplay.sys"},
            {"RpcSs",     "Remote Procedure Call (RPC)", 2u, "C:\\Windows\\system32\\svchost.exe -k rpcss"},
            {"MpsSvc",    "Windows Firewall",            2u, "C:\\Windows\\system32\\svchost.exe -k LocalServiceNoNetwork"},
        };
        uint32_t si;
        for (si = 0; si < sizeof(svcs) / sizeof(svcs[0]); si++) {
            char startmode[16];
            inst = wmi_create_instance("ROOT\\CIMV2", "Win32_Service");
            if (inst == NULL) break;
            wmi_add_property_str(inst, "Name", svcs[si].nm);
            wmi_add_property_str(inst, "DisplayName", svcs[si].dn);
            wmi_add_property_str(inst, "State", "Running");
            if (svcs[si].stm == 2u) strcpy(startmode, "Auto");
            else if (svcs[si].stm == 3u) strcpy(startmode, "Manual");
            else strcpy(startmode, "Disabled");
            wmi_add_property_str(inst, "StartMode", startmode);
            wmi_add_property_str(inst, "PathName", svcs[si].path);
            wmi_add_property_u32(inst, "ServiceType", 0x10u);
            wmi_add_property_bool(inst, "Started", TRUE);
            wmi_add_property_str(inst, "Status", "OK");
            wmi_add_property_u32(inst, "ProcessId", 1000u + si);
            wmi_add_property_u32(inst, "ExitCode", 0u);
            wmi_add_property_bool(inst, "DesktopInteract", FALSE);
            wmi_add_property_str(inst, "ErrorControl", "Normal");
        }
    }

    inst = wmi_create_instance("ROOT\\DEFAULT", "StdRegProv");
    if (inst != NULL) {
        wmi_add_property_str(inst, "METHOD_GetDWORDValue", "OK");
        wmi_add_property_str(inst, "METHOD_GetStringValue", "OK");
        wmi_add_property_str(inst, "METHOD_EnumKey", "OK");
        wmi_add_property_str(inst, "METHOD_EnumValues", "OK");
    }

    g_wmi_init = TRUE;
    return 0;
}

typedef struct {
    uint32_t signature;
    char namespace_[128];
} wmi_services_t;

static wmi_services_t* g_wmi_services_list[64];
static uint32_t g_wmi_services_count = 0;

uint32_t wmi_connect_server(const char* namespace_path, void** out_ptr) {
    wmi_services_t* svc;
    size_t nslen;
    if (!g_wmi_init) wmi_init();
    if (namespace_path == NULL || out_ptr == NULL) return WBEM_E_INVALID_PARAMETER;
    if (strcmp(namespace_path, "ROOT\\CIMV2") != 0 &&
        strcmp(namespace_path, "ROOT\\DEFAULT") != 0 &&
        strcmp(namespace_path, "root\\cimv2") != 0 &&
        strcmp(namespace_path, "root\\default") != 0) {
        return WBEM_E_INVALID_NAMESPACE;
    }
    if (g_wmi_services_count >= 64u) return WBEM_E_OUT_OF_MEMORY;
    svc = (wmi_services_t*)memory_alloc(sizeof(wmi_services_t));
    if (svc == NULL) return WBEM_E_OUT_OF_MEMORY;
    memset(svc, 0, sizeof(wmi_services_t));
    svc->signature = 0x5742454Du;
    nslen = strlen(namespace_path);
    if (nslen >= sizeof(svc->namespace_)) nslen = sizeof(svc->namespace_) - 1u;
    memcpy(svc->namespace_, namespace_path, nslen);
    svc->namespace_[nslen] = 0;
    g_wmi_services_list[g_wmi_services_count++] = svc;
    *out_ptr = svc;
    return WBEM_NO_ERROR;
}

static int strcasecmp_wmi(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a; char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static wmi_instance_t* find_first_instance(const char* ns, const char* classname) {
    uint32_t i;
    for (i = 0; i < g_wmi_inuse; i++) {
        if (strcasecmp_wmi(g_wmi_instances[i].namespace_, ns) == 0 &&
            strcasecmp_wmi(g_wmi_instances[i].class_name, classname) == 0) {
            return &g_wmi_instances[i];
        }
    }
    return NULL;
}

uint32_t wmi_exec_query(void* services, const char* query, const char* query_language, void** out_enum) {
    wmi_services_t* svc = (wmi_services_t*)services;
    wmi_enum_t* en;
    const char* sel;
    const char* from;
    const char* class_start;
    char class_buf[128];
    size_t class_len;
    uint32_t i;
    (void)query_language;
    if (svc == NULL || query == NULL || out_enum == NULL) return WBEM_E_INVALID_PARAMETER;
    if (svc->signature != 0x5742454Du) return WBEM_E_INVALID_PARAMETER;
    sel = strstr(query, "SELECT");
    from = strstr(query, "FROM");
    if (sel == NULL || from == NULL) return WBEM_E_INVALID_QUERY;
    class_start = from + 4;
    while (*class_start == ' ' || *class_start == '\t') class_start++;
    class_len = 0;
    while (class_start[class_len] != 0 && class_start[class_len] != ' ' && class_start[class_len] != '\t' &&
           class_start[class_len] != '\r' && class_start[class_len] != '\n' && class_start[class_len] != ';') {
        if (class_len + 1u < sizeof(class_buf)) {
            class_buf[class_len] = class_start[class_len];
        }
        class_len++;
    }
    if (class_len >= sizeof(class_buf)) class_len = sizeof(class_buf) - 1u;
    class_buf[class_len] = 0;

    en = (wmi_enum_t*)memory_alloc(sizeof(wmi_enum_t));
    if (en == NULL) return WBEM_E_OUT_OF_MEMORY;
    memset(en, 0, sizeof(wmi_enum_t));
    en->signature = 'ENUM';
    {
        size_t nslen = strlen(svc->namespace_);
        if (nslen >= sizeof(en->namespace_)) nslen = sizeof(en->namespace_) - 1u;
        memcpy(en->namespace_, svc->namespace_, nslen);
        en->namespace_[nslen] = 0;
    }
    {
        size_t clen = strlen(class_buf);
        if (clen >= sizeof(en->class_filter)) clen = sizeof(en->class_filter) - 1u;
        memcpy(en->class_filter, class_buf, clen);
        en->class_filter[clen] = 0;
    }

    for (i = 0; i < g_wmi_inuse; i++) {
        if (strcasecmp_wmi(g_wmi_instances[i].namespace_, en->namespace_) == 0 &&
            strcasecmp_wmi(g_wmi_instances[i].class_name, class_buf) == 0) {
            en->current = &g_wmi_instances[i];
            break;
        }
    }
    if (i == g_wmi_inuse) {
        en->current = NULL;
    }
    *out_enum = en;
    return WBEM_NO_ERROR;
}

uint32_t wmi_enum_get(void* enumerator, uint32_t timeout, uint32_t count, wmi_instance_t** arr, uint32_t* returned) {
    wmi_enum_t* en = (wmi_enum_t*)enumerator;
    uint32_t got = 0;
    (void)timeout;
    if (en == NULL || arr == NULL || returned == NULL) return WBEM_E_INVALID_PARAMETER;
    if (en->signature != (uint32_t)'ENUM') return WBEM_E_INVALID_PARAMETER;
    while (got < count && en->current != NULL) {
        wmi_instance_t* cur = en->current;
        arr[got++] = cur;
        {
            uint32_t i;
            uint32_t idx = (uint32_t)(cur - g_wmi_instances);
            wmi_instance_t* next = NULL;
            for (i = idx + 1u; i < g_wmi_inuse; i++) {
                if (strcasecmp_wmi(g_wmi_instances[i].namespace_, en->namespace_) == 0 &&
                    strcasecmp_wmi(g_wmi_instances[i].class_name, en->class_filter) == 0) {
                    next = &g_wmi_instances[i];
                    break;
                }
            }
            en->current = next;
        }
    }
    *returned = got;
    if (got == 0) return WBEM_S_FALSE;
    return WBEM_NO_ERROR;
}

wmi_instance_t* wmi_get_class_instance(const char* ns, const char* class_name, const char* key_prop, const char* key_val) {
    wmi_instance_t* first;
    wmi_property_t* prop;
    if (!g_wmi_init) wmi_init();
    first = find_first_instance(ns, class_name);
    if (first == NULL) return NULL;
    if (key_prop == NULL || key_val == NULL) return first;
    {
        wmi_instance_t* scan = first;
        while (scan != NULL) {
            if (strcasecmp_wmi(scan->namespace_, ns) == 0 &&
                strcasecmp_wmi(scan->class_name, class_name) == 0) {
                prop = scan->first_property;
                while (prop != NULL) {
                    if (strcasecmp_wmi(prop->name, key_prop) == 0 && prop->type == CIM_STRING) {
                        if (strcmp(prop->v.string_value, key_val) == 0) return scan;
                    }
                    prop = prop->next;
                }
            }
            if (scan->next_instance != NULL) {
                scan = scan->next_instance;
            } else {
                uint32_t i;
                uint32_t idx = (uint32_t)(scan - g_wmi_instances);
                wmi_instance_t* next_scan = NULL;
                for (i = idx + 1u; i < g_wmi_inuse; i++) {
                    if (strcasecmp_wmi(g_wmi_instances[i].namespace_, ns) == 0 &&
                        strcasecmp_wmi(g_wmi_instances[i].class_name, class_name) == 0) {
                        next_scan = &g_wmi_instances[i];
                        break;
                    }
                }
                scan = next_scan;
            }
        }
    }
    return first;
}

uint32_t wmi_get_property(wmi_instance_t* inst, const char* property_name, CIMTYPE_ENUM* out_type,
                          void* out_buffer, uint32_t buffer_size) {
    wmi_property_t* p;
    if (inst == NULL || property_name == NULL) return WBEM_E_INVALID_PARAMETER;
    p = inst->first_property;
    while (p != NULL) {
        if (strcasecmp_wmi(p->name, property_name) == 0) {
            if (out_type != NULL) *out_type = p->type;
            if (out_buffer != NULL && buffer_size > 0) {
                uint32_t copy_size = 0;
                switch (p->type) {
                    case CIM_STRING:
                        copy_size = (uint32_t)strlen(p->v.string_value) + 1u;
                        if (copy_size > buffer_size) copy_size = buffer_size;
                        memcpy(out_buffer, p->v.string_value, copy_size);
                        break;
                    case CIM_UINT8:
                    case CIM_SINT8:
                        copy_size = 1u;
                        if (buffer_size >= copy_size) *(uint8_t*)out_buffer = p->v.u8;
                        break;
                    case CIM_UINT16:
                    case CIM_SINT16:
                        copy_size = 2u;
                        if (buffer_size >= copy_size) *(uint16_t*)out_buffer = p->v.u16;
                        break;
                    case CIM_UINT32:
                    case CIM_SINT32:
                        copy_size = 4u;
                        if (buffer_size >= copy_size) *(uint32_t*)out_buffer = p->v.u32;
                        break;
                    case CIM_UINT64:
                    case CIM_SINT64:
                    case CIM_DATETIME:
                        copy_size = 8u;
                        if (buffer_size >= copy_size) *(uint64_t*)out_buffer = p->v.u64;
                        break;
                    case CIM_BOOLEAN:
                        copy_size = sizeof(BOOL);
                        if (buffer_size >= copy_size) *(BOOL*)out_buffer = p->v.boolean;
                        break;
                    default:
                        return WBEM_E_TYPE_MISMATCH;
                }
            }
            return WBEM_NO_ERROR;
        }
        p = p->next;
    }
    return WBEM_E_NOT_FOUND;
}

BOOL wmi_put_property(wmi_instance_t* inst, const char* property_name, CIMTYPE_ENUM type,
                       const void* data, uint32_t array_len) {
    wmi_property_t* p;
    (void)array_len;
    if (inst == NULL || property_name == NULL) return FALSE;
    p = inst->first_property;
    while (p != NULL) {
        if (strcasecmp_wmi(p->name, property_name) == 0) {
            p->type = type;
            if (data != NULL) {
                switch (type) {
                    case CIM_STRING:
                        {
                            size_t n = strlen((const char*)data);
                            if (n >= sizeof(p->v.string_value)) n = sizeof(p->v.string_value) - 1u;
                            memcpy(p->v.string_value, data, n);
                            p->v.string_value[n] = 0;
                        }
                        break;
                    case CIM_UINT8:
                    case CIM_SINT8:
                        p->v.u8 = *(const uint8_t*)data;
                        break;
                    case CIM_UINT16:
                    case CIM_SINT16:
                        p->v.u16 = *(const uint16_t*)data;
                        break;
                    case CIM_UINT32:
                    case CIM_SINT32:
                        p->v.u32 = *(const uint32_t*)data;
                        break;
                    case CIM_UINT64:
                    case CIM_SINT64:
                    case CIM_DATETIME:
                        p->v.u64 = *(const uint64_t*)data;
                        break;
                    case CIM_BOOLEAN:
                        p->v.boolean = *(const BOOL*)data;
                        break;
                    default:
                        return FALSE;
                }
            }
            return TRUE;
        }
        p = p->next;
    }
    {
        wmi_property_t* np = wmi_add_property_base(inst, property_name, type);
        if (np == NULL) return FALSE;
        if (data != NULL) {
            switch (type) {
                case CIM_STRING:
                    {
                        size_t n = strlen((const char*)data);
                        if (n >= sizeof(np->v.string_value)) n = sizeof(np->v.string_value) - 1u;
                        memcpy(np->v.string_value, data, n);
                        np->v.string_value[n] = 0;
                    }
                    break;
                case CIM_UINT8:
                case CIM_SINT8:
                    np->v.u8 = *(const uint8_t*)data;
                    break;
                case CIM_UINT16:
                case CIM_SINT16:
                    np->v.u16 = *(const uint16_t*)data;
                    break;
                case CIM_UINT32:
                case CIM_SINT32:
                    np->v.u32 = *(const uint32_t*)data;
                    break;
                case CIM_UINT64:
                case CIM_SINT64:
                case CIM_DATETIME:
                    np->v.u64 = *(const uint64_t*)data;
                    break;
                case CIM_BOOLEAN:
                    np->v.boolean = *(const BOOL*)data;
                    break;
                default:
                    break;
            }
        }
        return TRUE;
    }
}
