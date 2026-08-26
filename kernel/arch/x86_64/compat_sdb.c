/*
 * Application Compatibility (AppHelp) subsystem - freestanding implementation.
 *
 * Provides a simplified SDB (Shim Database) engine, an API hooking shim
 * engine, and the version-lie compatibility layer.  All state is kept in
 * fixed-size static pools so no dynamic allocator is required.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>
#include <stdio.h>

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

/* ------------------------------------------------------------------ *
 * Wide-string helpers (libc has no wchar_t support)
 * ------------------------------------------------------------------ */

static size_t sdb_wcslen(const wchar_t* s)
{
    size_t n = 0;
    if (s == NULL) return 0;
    while (s[n]) n++;
    return n;
}

static wchar_t* sdb_wcscpy(wchar_t* d, const wchar_t* s)
{
    wchar_t* r = d;
    if (d == NULL || s == NULL) return d;
    while ((*d++ = *s++) != 0) {}
    return r;
}

static int sdb_wcscmp(const wchar_t* a, const wchar_t* b)
{
    if (a == NULL || b == NULL) {
        if (a == b) return 0;
        return a ? 1 : -1;
    }
    while (*a && *a == *b) { a++; b++; }
    return (int)*a - (int)*b;
}

/* Case-insensitive substring search: returns pointer to first occurrence of
 * needle within haystack, or NULL if not found.  ASCII-only folding. */
static const wchar_t* sdb_wcsistr(const wchar_t* haystack, const wchar_t* needle)
{
    size_t hlen, nlen, i;
    if (haystack == NULL || needle == NULL) return NULL;
    hlen = sdb_wcslen(haystack);
    nlen = sdb_wcslen(needle);
    if (nlen == 0) return haystack;
    if (nlen > hlen) return NULL;
    for (i = 0; i + nlen <= hlen; i++) {
        size_t j;
        int match = 1;
        for (j = 0; j < nlen; j++) {
            wchar_t hc = haystack[i + j];
            wchar_t nc = needle[j];
            if (hc >= L'A' && hc <= L'Z') hc = (wchar_t)(hc + (L'a' - L'A'));
            if (nc >= L'A' && nc <= L'Z') nc = (wchar_t)(nc + (L'a' - L'A'));
            if (hc != nc) { match = 0; break; }
        }
        if (match) return &haystack[i];
    }
    return NULL;
}

static int sdb_guid_equal(const GUID* a, const GUID* b)
{
    if (a == NULL || b == NULL) return 0;
    if (a->Data1 != b->Data1) return 0;
    if (a->Data2 != b->Data2) return 0;
    if (a->Data3 != b->Data3) return 0;
    return memcmp(a->Data4, b->Data4, sizeof(a->Data4)) == 0;
}

/* ================================================================== *
 * 1. SDB Database Engine
 * ================================================================== */

#define SDB_MAX_POOL    32

typedef struct _SDB_ENTRY {
    int             in_use;
    int             auto_loaded;
    wchar_t         path[260];
    GUID            guid;
    DB_INFORMATION  info;
    /* In-memory tag tree (simplified) */
    uint32_t        tag_count;
    TAG             tags[256];
    TAGID           tag_ids[256];
    char            tag_data[256][64];
} SDB_ENTRY;

static SDB_ENTRY g_sdb_pool[SDB_MAX_POOL];
static int       g_sdb_count;

/* Permanent layer registry (per-exe persistent compatibility layers). */
typedef struct _PERM_LAYER_ENTRY {
    int     in_use;
    int     enabled;
    wchar_t exe_path[260];
    wchar_t layer[64];
} PERM_LAYER_ENTRY;

static PERM_LAYER_ENTRY g_perm_layers[32];

/* Tag IDs assigned to the dummy layers stored in each SDB entry.  The
 * values are arbitrary but stable so tagref lookups round-trip cleanly. */
#define SDB_TAGID_WIN95     1
#define SDB_TAGID_WINXP     2
#define SDB_TAGID_WIN7      3

/* Populate an SDB entry with three dummy string tags representing the
 * Win95 / WinXP / Win7 compatibility layers. */
static void sdb_populate_dummy_tags(SDB_ENTRY* e)
{
    e->tag_count = 3;

    e->tag_ids[0] = SDB_TAGID_WIN95;
    e->tags[0]    = TAG_TYPE_STRING;
    strncpy(e->tag_data[0], "Win95", sizeof(e->tag_data[0]) - 1);
    e->tag_data[0][sizeof(e->tag_data[0]) - 1] = '\0';

    e->tag_ids[1] = SDB_TAGID_WINXP;
    e->tags[1]    = TAG_TYPE_STRING;
    strncpy(e->tag_data[1], "WinXP", sizeof(e->tag_data[1]) - 1);
    e->tag_data[1][sizeof(e->tag_data[1]) - 1] = '\0';

    e->tag_ids[2] = SDB_TAGID_WIN7;
    e->tags[2]    = TAG_TYPE_STRING;
    strncpy(e->tag_data[2], "Win7", sizeof(e->tag_data[2]) - 1);
    e->tag_data[2][sizeof(e->tag_data[2]) - 1] = '\0';
}

HSDB sdb_init_database(uint32_t flags)
{
    int i;
    for (i = 0; i < SDB_MAX_POOL; i++) {
        SDB_ENTRY* e = &g_sdb_pool[i];
        if (!e->in_use) {
            memset(e, 0, sizeof(*e));
            e->in_use     = 1;
            e->auto_loaded = (flags & 0x00000001u) ? 1 : 0;
            g_sdb_count++;
            return (HSDB)e;
        }
    }
    return NULL;
}

void sdb_release_database(HSDB hsdb)
{
    SDB_ENTRY* e;
    if (hsdb == NULL) return;
    e = (SDB_ENTRY*)hsdb;
    memset(e, 0, sizeof(*e));
    if (g_sdb_count > 0) g_sdb_count--;
}

int sdb_open_database(HSDB hsdb, const wchar_t* path)
{
    SDB_ENTRY* e;
    if (hsdb == NULL) return -1;
    e = (SDB_ENTRY*)hsdb;
    sdb_wcscpy(e->path, path);
    sdb_populate_dummy_tags(e);
    return 0;
}

int sdb_close_database(PDB pdb)
{
    (void)pdb;
    return 0;
}

int sdb_get_matching_exe(HSDB hsdb, const wchar_t* exe_path,
                         const wchar_t* env, uint32_t flags,
                         SDBQUERYRESULT* result)
{
    SDB_ENTRY* e;
    (void)env;
    (void)flags;
    if (hsdb == NULL || exe_path == NULL || result == NULL) return 0;
    e = (SDB_ENTRY*)hsdb;

    memset(result, 0, sizeof(*result));

    if (sdb_wcsistr(exe_path, L"setup") != NULL ||
        sdb_wcsistr(exe_path, L"install") != NULL) {
        /* Match the Win95 compatibility layer. */
        result->atrExes[0]     = SDB_TAGID_WIN95;
        result->adwExeFlags[0] = 1;
        result->atrLayers[0]   = SDB_TAGID_WIN95;
        result->guidID         = e->guid;
        return 1;
    }

    if (sdb_wcsistr(exe_path, L"game") != NULL) {
        /* Match the WinXP compatibility layer. */
        result->atrExes[0]     = SDB_TAGID_WINXP;
        result->adwExeFlags[0] = 1;
        result->atrLayers[0]   = SDB_TAGID_WINXP;
        result->guidID         = e->guid;
        return 1;
    }

    return 0;
}

TAGREF sdb_tagid_to_tagref(HSDB hsdb, TAGID tagid)
{
    (void)hsdb;
    return (TAGREF)tagid;
}

TAGID sdb_tagref_to_tagid(HSDB hsdb, TAGREF tagref)
{
    (void)hsdb;
    return (TAGID)tagref;
}

int sdb_register_database(const wchar_t* path, const GUID* guid)
{
    int i;
    if (path == NULL || guid == NULL) return -1;
    for (i = 0; i < SDB_MAX_POOL; i++) {
        SDB_ENTRY* e = &g_sdb_pool[i];
        if (!e->in_use) {
            memset(e, 0, sizeof(*e));
            e->in_use = 1;
            sdb_wcscpy(e->path, path);
            memcpy(&e->guid, guid, sizeof(GUID));
            sdb_populate_dummy_tags(e);
            g_sdb_count++;
            return 0;
        }
    }
    return -1;
}

int sdb_unregister_database(const GUID* guid)
{
    int i;
    if (guid == NULL) return -1;
    for (i = 0; i < SDB_MAX_POOL; i++) {
        SDB_ENTRY* e = &g_sdb_pool[i];
        if (e->in_use && sdb_guid_equal(&e->guid, guid)) {
            memset(e, 0, sizeof(*e));
            if (g_sdb_count > 0) g_sdb_count--;
            return 0;
        }
    }
    return 0;
}

int sdb_get_database_information(PDB pdb, DB_INFORMATION* info)
{
    (void)pdb;
    if (info == NULL) return -1;
    info->dwMajor      = 2;
    info->dwMinor      = 1;
    info->szDescription = L"Kenux SDB";
    memset(&info->guidID, 0, sizeof(GUID));
    return 0;
}

int sdb_get_file_attributes(const wchar_t* path, ATTRINFO* attrs, uint32_t* count)
{
    (void)path;
    if (attrs == NULL || count == NULL) return -1;
    if (*count < 2) return -1;

    memset(attrs, 0, sizeof(ATTRINFO) * (*count));

    /* Entry 0: file size (qword). */
    attrs[0].type     = TAG_TYPE_QWORD;
    attrs[0].flags    = ATTRIBUTE_AVAILABLE;
    attrs[0].qwattr   = 4096;

    /* Entry 1: modification time (qword). */
    attrs[1].type     = TAG_TYPE_QWORD;
    attrs[1].flags    = ATTRIBUTE_AVAILABLE;
    attrs[1].qwattr   = 0;

    *count = 2;
    return 0;
}

void sdb_free_file_attributes(ATTRINFO* attrs, uint32_t count)
{
    (void)attrs;
    (void)count;
    /* No-op: storage is caller-provided. */
}

int sdb_get_perm_layer_keys(const wchar_t* exe_path, wchar_t* layers,
                            uint32_t* length, uint32_t flags)
{
    int i;
    uint32_t needed = 0;
    (void)flags;
    if (exe_path == NULL || layers == NULL || length == NULL) return -1;

    layers[0] = 0;
    for (i = 0; i < 32; i++) {
        PERM_LAYER_ENTRY* p = &g_perm_layers[i];
        if (p->in_use && p->enabled && sdb_wcscmp(p->exe_path, exe_path) == 0) {
            size_t l = sdb_wcslen(p->layer);
            if (needed > 0 && needed + 1 + l + 1 > *length) break;
            if (needed > 0) {
                layers[needed++] = L' ';
            }
            sdb_wcscpy(&layers[needed], p->layer);
            needed += (uint32_t)l;
        }
    }
    *length = needed;
    return 0;
}

int sdb_set_perm_layer_state(const wchar_t* exe_path, const wchar_t* layer,
                             int enable, uint32_t flags)
{
    int i;
    int free_slot = -1;
    (void)flags;
    if (exe_path == NULL || layer == NULL) return -1;

    for (i = 0; i < 32; i++) {
        PERM_LAYER_ENTRY* p = &g_perm_layers[i];
        if (p->in_use &&
            sdb_wcscmp(p->exe_path, exe_path) == 0 &&
            sdb_wcscmp(p->layer, layer) == 0) {
            if (enable) {
                p->enabled = 1;
            } else {
                /* Disable by clearing the slot. */
                memset(p, 0, sizeof(*p));
            }
            return 0;
        }
        if (!p->in_use && free_slot < 0) free_slot = i;
    }

    if (enable && free_slot >= 0) {
        PERM_LAYER_ENTRY* p = &g_perm_layers[free_slot];
        memset(p, 0, sizeof(*p));
        p->in_use  = 1;
        p->enabled = 1;
        sdb_wcscpy(p->exe_path, exe_path);
        sdb_wcscpy(p->layer, layer);
    }
    return 0;
}

/* ================================================================== *
 * 2. Shim Engine (API Hooking)
 * ================================================================== */

#define MAX_HOOKS 128

typedef struct _HOOK_ENTRY {
    int         in_use;
    char        library[64];
    char        function[64];
    void*       replacement;
    void*       original;
    SHIM_REG*   shim_reg;
} HOOK_ENTRY;

static HOOK_ENTRY g_hooks[MAX_HOOKS];
static int        g_shim_initialized;
static SHIM_REG*  g_loaded_shims[32];
static int        g_shim_count;

/* ---- Dummy replacement functions ----
 * Each shim exposes a small set of replacement hooks.  Because this is a
 * simulation with no real target binary, the replacement / original
 * pointers are sentinels derived from a static byte array so they are
 * non-NULL, stable and distinct. */
static unsigned char g_hook_sentinel[64];

static void* sdb_hook_replacement(int idx)
{
    return (void*)&g_hook_sentinel[(idx * 4) % (int)sizeof(g_hook_sentinel)];
}

static void* sdb_hook_original(int idx)
{
    return (void*)&g_hook_sentinel[((idx * 4) + 2) % (int)sizeof(g_hook_sentinel)];
}

/* ---- Built-in shim: Version Lie ---- */
static HOOKAPI g_vlie_hooks[2];

static PHOOKAPI vlie_get_hooks(const char* cmdline, const wchar_t* shim_name,
                               uint32_t* hook_count)
{
    (void)cmdline;
    (void)shim_name;
    g_vlie_hooks[0].LibraryName        = "kernel32.dll";
    g_vlie_hooks[0].FunctionName       = "GetVersionExA";
    g_vlie_hooks[0].ReplacementFunction = sdb_hook_replacement(0);
    g_vlie_hooks[0].OriginalFunction   = sdb_hook_original(0);
    g_vlie_hooks[0].Reserved[0]        = NULL;
    g_vlie_hooks[0].Reserved[1]        = NULL;

    g_vlie_hooks[1].LibraryName        = "kernel32.dll";
    g_vlie_hooks[1].FunctionName       = "GetVersionExW";
    g_vlie_hooks[1].ReplacementFunction = sdb_hook_replacement(1);
    g_vlie_hooks[1].OriginalFunction   = sdb_hook_original(1);
    g_vlie_hooks[1].Reserved[0]        = NULL;
    g_vlie_hooks[1].Reserved[1]        = NULL;

    if (hook_count) *hook_count = 2;
    return g_vlie_hooks;
}

static int vlie_notify(uint32_t reason, void* ptr)
{
    (void)reason;
    (void)ptr;
    return 0;
}

static SHIM_REG g_shim_versionlie = {
    vlie_get_hooks,
    vlie_notify,
    "versionlie"
};

/* ---- Built-in shim: MSYS2 ---- */
static HOOKAPI g_msys2_hooks[2];

static PHOOKAPI msys2_get_hooks(const char* cmdline, const wchar_t* shim_name,
                                uint32_t* hook_count)
{
    (void)cmdline;
    (void)shim_name;
    g_msys2_hooks[0].LibraryName        = "kernel32.dll";
    g_msys2_hooks[0].FunctionName       = "CreateFileA";
    g_msys2_hooks[0].ReplacementFunction = sdb_hook_replacement(2);
    g_msys2_hooks[0].OriginalFunction   = sdb_hook_original(2);
    g_msys2_hooks[0].Reserved[0]        = NULL;
    g_msys2_hooks[0].Reserved[1]        = NULL;

    g_msys2_hooks[1].LibraryName        = "kernel32.dll";
    g_msys2_hooks[1].FunctionName       = "CreateFileW";
    g_msys2_hooks[1].ReplacementFunction = sdb_hook_replacement(3);
    g_msys2_hooks[1].OriginalFunction   = sdb_hook_original(3);
    g_msys2_hooks[1].Reserved[0]        = NULL;
    g_msys2_hooks[1].Reserved[1]        = NULL;

    if (hook_count) *hook_count = 2;
    return g_msys2_hooks;
}

static int msys2_notify(uint32_t reason, void* ptr)
{
    (void)reason;
    (void)ptr;
    return 0;
}

static SHIM_REG g_shim_msys2 = {
    msys2_get_hooks,
    msys2_notify,
    "msys2"
};

/* ---- Built-in shim: Display Mode ---- */
static HOOKAPI g_disp_hooks[1];

static PHOOKAPI dispmode_get_hooks(const char* cmdline, const wchar_t* shim_name,
                                   uint32_t* hook_count)
{
    (void)cmdline;
    (void)shim_name;
    g_disp_hooks[0].LibraryName        = "user32.dll";
    g_disp_hooks[0].FunctionName       = "ChangeDisplaySettingsA";
    g_disp_hooks[0].ReplacementFunction = sdb_hook_replacement(4);
    g_disp_hooks[0].OriginalFunction   = sdb_hook_original(4);
    g_disp_hooks[0].Reserved[0]        = NULL;
    g_disp_hooks[0].Reserved[1]        = NULL;

    if (hook_count) *hook_count = 1;
    return g_disp_hooks;
}

static int dispmode_notify(uint32_t reason, void* ptr)
{
    (void)reason;
    (void)ptr;
    return 0;
}

static SHIM_REG g_shim_dispmode = {
    dispmode_get_hooks,
    dispmode_notify,
    "dispmode"
};

/* ---- Built-in shim: GlobalMemoryStatus ---- */
static HOOKAPI g_gms_hooks[1];

static PHOOKAPI globalmemorystatus_get_hooks(const char* cmdline,
                                             const wchar_t* shim_name,
                                             uint32_t* hook_count)
{
    (void)cmdline;
    (void)shim_name;
    g_gms_hooks[0].LibraryName        = "kernel32.dll";
    g_gms_hooks[0].FunctionName       = "GlobalMemoryStatus";
    g_gms_hooks[0].ReplacementFunction = sdb_hook_replacement(5);
    g_gms_hooks[0].OriginalFunction   = sdb_hook_original(5);
    g_gms_hooks[0].Reserved[0]        = NULL;
    g_gms_hooks[0].Reserved[1]        = NULL;

    if (hook_count) *hook_count = 1;
    return g_gms_hooks;
}

static int globalmemorystatus_notify(uint32_t reason, void* ptr)
{
    (void)reason;
    (void)ptr;
    return 0;
}

static SHIM_REG g_shim_globalmemorystatus = {
    globalmemorystatus_get_hooks,
    globalmemorystatus_notify,
    "globalmemorystatus"
};

/* ---- Engine ---- */

int shim_engine_init(void)
{
    memset(g_hooks, 0, sizeof(g_hooks));
    memset(g_loaded_shims, 0, sizeof(g_loaded_shims));
    g_shim_count = 0;

    /* Register the four built-in shim modules. */
    g_loaded_shims[g_shim_count++] = &g_shim_versionlie;
    g_loaded_shims[g_shim_count++] = &g_shim_msys2;
    g_loaded_shims[g_shim_count++] = &g_shim_dispmode;
    g_loaded_shims[g_shim_count++] = &g_shim_globalmemorystatus;

    g_shim_initialized = 1;
    return 0;
}

void shim_engine_deinit(void)
{
    memset(g_hooks, 0, sizeof(g_hooks));
    memset(g_loaded_shims, 0, sizeof(g_loaded_shims));
    g_shim_count = 0;
    g_shim_initialized = 0;
}

int shim_engine_load_dll(const char* dll_name)
{
    (void)dll_name;
    /* Simulation: no actual DLL loading is performed. */
    return 0;
}

int shim_engine_apply(const wchar_t* exe_path, const SDBQUERYRESULT* result)
{
    int i;
    (void)exe_path;
    if (!g_shim_initialized || result == NULL) return 0;

    /* For each loaded shim whose layer appears in the result, ask the
     * shim for its hook table and register the hooks.  In this simulation
     * every loaded shim is consulted, mirroring the real engine's
     * behaviour of applying all matching layers. */
    for (i = 0; i < g_shim_count; i++) {
        SHIM_REG* shim = g_loaded_shims[i];
        uint32_t count = 0;
        PHOOKAPI hooks;
        if (shim == NULL || shim->GetHookAPIs == NULL) continue;
        hooks = shim->GetHookAPIs("", NULL, &count);
        if (hooks != NULL && count > 0) {
            shim_engine_register_hooks(hooks, count);
        }
    }
    return 0;
}

int shim_engine_register_hooks(const HOOKAPI* hooks, uint32_t count)
{
    uint32_t i;
    if (hooks == NULL) return -1;
    for (i = 0; i < count; i++) {
        int j;
        const HOOKAPI* h = &hooks[i];
        if (h->LibraryName == NULL || h->FunctionName == NULL) continue;
        for (j = 0; j < MAX_HOOKS; j++) {
            HOOK_ENTRY* e = &g_hooks[j];
            if (!e->in_use) {
                memset(e, 0, sizeof(*e));
                e->in_use = 1;
                strncpy(e->library, h->LibraryName, sizeof(e->library) - 1);
                e->library[sizeof(e->library) - 1] = '\0';
                strncpy(e->function, h->FunctionName, sizeof(e->function) - 1);
                e->function[sizeof(e->function) - 1] = '\0';
                e->replacement = h->ReplacementFunction;
                e->original     = h->OriginalFunction;
                e->shim_reg    = NULL;
                break;
            }
        }
    }
    return 0;
}

void* shim_engine_get_original(const char* library, const char* function)
{
    int i;
    if (library == NULL || function == NULL) return NULL;
    for (i = 0; i < MAX_HOOKS; i++) {
        HOOK_ENTRY* e = &g_hooks[i];
        if (e->in_use &&
            strcmp(e->library, library) == 0 &&
            strcmp(e->function, function) == 0) {
            return e->original;
        }
    }
    return NULL;
}

int shim_engine_notify_all(uint32_t reason, void* ptr)
{
    int i;
    for (i = 0; i < g_shim_count; i++) {
        SHIM_REG* shim = g_loaded_shims[i];
        if (shim != NULL && shim->Notify != NULL) {
            shim->Notify(reason, ptr);
        }
    }
    return 0;
}

int shim_engine_is_initialized(void)
{
    return g_shim_initialized;
}

/* ================================================================== *
 * 3. Version Lie Shim
 *
 * All FullVersion / build values are sourced verbatim from the ReactOS
 * compatibility layer:
 *   compat_layer/dll/appcompat/shims/layer/versionlie.c
 * so that GetVersion() / GetVersionEx() lies match real Windows behaviour.
 * ================================================================== */

const VERSION_LIE_INFO g_vlie_win95       = {0xC3B60004, 4, 0, 950,         VER_PLATFORM_WIN32_WINDOWS, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win98       = {0xC0000A04, 4, 10, 0x040A08AE, VER_PLATFORM_WIN32_WINDOWS, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_winnt4sp5   = {0x05650004, 4, 0, 1381,        VER_PLATFORM_WIN32_NT, 5, 0, "Service Pack 5", L"Service Pack 5"};
const VERSION_LIE_INFO g_vlie_win2000     = {0x08930005, 5, 0, 2195,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win2000sp1  = {0x08930005, 5, 0, 2195,        VER_PLATFORM_WIN32_NT, 1, 0, "Service Pack 1", L"Service Pack 1"};
const VERSION_LIE_INFO g_vlie_win2000sp2  = {0x08930005, 5, 0, 2195,        VER_PLATFORM_WIN32_NT, 2, 0, "Service Pack 2", L"Service Pack 2"};
const VERSION_LIE_INFO g_vlie_win2000sp3  = {0x08930005, 5, 0, 2195,        VER_PLATFORM_WIN32_NT, 3, 0, "Service Pack 3", L"Service Pack 3"};
const VERSION_LIE_INFO g_vlie_winxp       = {0x0a280105, 5, 1, 2600,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_winxpsp1    = {0x0a280105, 5, 1, 2600,        VER_PLATFORM_WIN32_NT, 1, 0, "Service Pack 1", L"Service Pack 1"};
const VERSION_LIE_INFO g_vlie_winxpsp2    = {0x0a280105, 5, 1, 2600,        VER_PLATFORM_WIN32_NT, 2, 0, "Service Pack 2", L"Service Pack 2"};
const VERSION_LIE_INFO g_vlie_winxpsp3    = {0x0a280105, 5, 1, 2600,        VER_PLATFORM_WIN32_NT, 3, 0, "Service Pack 3", L"Service Pack 3"};
const VERSION_LIE_INFO g_vlie_win2k3rtm   = {0x0ece0205, 5, 2, 3790,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win2k3sp1   = {0x0ece0205, 5, 2, 3790,        VER_PLATFORM_WIN32_NT, 1, 0, "Service Pack 1", L"Service Pack 1"};
const VERSION_LIE_INFO g_vlie_winvistartm = {0x17700006, 6, 0, 6000,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_winvistasp1 = {0x17710006, 6, 0, 6001,        VER_PLATFORM_WIN32_NT, 1, 0, "Service Pack 1", L"Service Pack 1"};
const VERSION_LIE_INFO g_vlie_winvistasp2 = {0x17720006, 6, 0, 6002,        VER_PLATFORM_WIN32_NT, 2, 0, "Service Pack 2", L"Service Pack 2"};
const VERSION_LIE_INFO g_vlie_win7rtm     = {0x1db00106, 6, 1, 7600,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win7sp1     = {0x1db10106, 6, 1, 7601,        VER_PLATFORM_WIN32_NT, 1, 0, "Service Pack 1", L"Service Pack 1"};
const VERSION_LIE_INFO g_vlie_win8rtm     = {0x23f00206, 6, 2, 9200,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win81rtm    = {0x25800306, 6, 3, 9600,        VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win10rtm    = {0x47ba000a, 10, 0, 18362,      VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win2k16rtm  = {0x3fab000a, 10, 0, 16299,      VER_PLATFORM_WIN32_NT, 0, 0, "", L""};
const VERSION_LIE_INFO g_vlie_win2k19rtm  = {0x4563000a, 10, 0, 17763,      VER_PLATFORM_WIN32_NT, 0, 0, "", L""};

void version_lie_get_info(uint32_t version_id, VERSION_LIE_INFO* info)
{
    const VERSION_LIE_INFO* src;
    if (info == NULL) return;
    switch (version_id) {
        case 0:  src = &g_vlie_win95;       break;
        case 1:  src = &g_vlie_win98;       break;
        case 2:  src = &g_vlie_winnt4sp5;   break;
        case 3:  src = &g_vlie_win2000;     break;
        case 4:  src = &g_vlie_win2000sp1;  break;
        case 5:  src = &g_vlie_win2000sp2;  break;
        case 6:  src = &g_vlie_win2000sp3;  break;
        case 7:  src = &g_vlie_winxp;       break;
        case 8:  src = &g_vlie_winxpsp1;    break;
        case 9:  src = &g_vlie_winxpsp2;    break;
        case 10: src = &g_vlie_winxpsp3;    break;
        case 11: src = &g_vlie_win2k3rtm;   break;
        case 12: src = &g_vlie_win2k3sp1;   break;
        case 13: src = &g_vlie_winvistartm; break;
        case 14: src = &g_vlie_winvistasp1; break;
        case 15: src = &g_vlie_winvistasp2; break;
        case 16: src = &g_vlie_win7rtm;     break;
        case 17: src = &g_vlie_win7sp1;     break;
        case 18: src = &g_vlie_win8rtm;     break;
        case 19: src = &g_vlie_win81rtm;    break;
        case 20: src = &g_vlie_win10rtm;    break;
        case 21: src = &g_vlie_win2k16rtm;  break;
        case 22: src = &g_vlie_win2k19rtm;  break;
        default: src = &g_vlie_win10rtm;    break;
    }
    memcpy(info, src, sizeof(VERSION_LIE_INFO));
}

int version_lie_apply(const VERSION_LIE_INFO* info,
                      uint32_t* major, uint32_t* minor,
                      uint32_t* build, uint16_t* platform,
                      char* csd, uint32_t csd_size)
{
    if (info == NULL) return -1;
    if (major)    *major    = info->major;
    if (minor)    *minor    = info->minor;
    if (build)    *build    = info->build;
    if (platform) *platform = info->platform_id;
    if (csd != NULL && csd_size > 0) {
        size_t n = strlen(info->csd_a);
        if (n >= csd_size) n = csd_size - 1;
        memcpy(csd, info->csd_a, n);
        csd[n] = '\0';
    }
    return 0;
}
