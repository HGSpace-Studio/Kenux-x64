/*
 * COM / OLE Automation subsystem - freestanding kernel implementation.
 *
 * Provides a minimal COM runtime: GUID comparison, BSTR and SAFEARRAY
 * helpers, class-object registration, a type-library registry and CLSID/
 * ProgID string conversion.  Built-in class factories for the common
 * scripting objects (FileSystemObject, WScript.Shell, ADODB.Connection)
 * are registered by com_init().
 */

/* win32.h references a few WDM pointer typedefs (PDEVICE_OBJECT, PIRP,
 * PDRIVER_OBJECT) and CCHAR before they are defined.  Pre-declare them
 * the same way wdm.c does so the header parses cleanly. */
#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT* PDEVICE_OBJECT;
typedef struct _DRIVER_OBJECT* PDRIVER_OBJECT;
typedef struct _IRP* PIRP;
#endif

#include <arch/win32.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

/* <memory.h> does not declare the kernel allocator; bring it in the same
 * way the other win32 subsystem files do. */
extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

/* ------------------------------------------------------------------ *
 * Internal well-known IIDs
 * ------------------------------------------------------------------ */
static const IID g_IID_IUnknown = {
    0x00000000, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
static const IID g_IID_IClassFactory = {
    0x00000001, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

/* ------------------------------------------------------------------ *
 * GUID comparison
 * ------------------------------------------------------------------ */
BOOL IsEqualGUID(REFGUID a, REFGUID b)
{
    int i;
    if (!a || !b)
        return FALSE;
    if (a->Data1 != b->Data1 || a->Data2 != b->Data2 || a->Data3 != b->Data3)
        return FALSE;
    for (i = 0; i < 8; i++)
        if (a->Data4[i] != b->Data4[i])
            return FALSE;
    return TRUE;
}

BOOL IsEqualIID(REFIID a, REFIID b)
{
    return IsEqualGUID(a, b);
}

BOOL IsEqualCLSID(REFCLSID a, REFCLSID b)
{
    return IsEqualGUID(a, b);
}

/* ------------------------------------------------------------------ *
 * BSTR helpers
 *
 * A BSTR is a length-prefixed wide string: a 4-byte character count is
 * stored immediately before the returned pointer, followed by the WCHAR
 * data and a terminating NUL.  SysFreeString steps back 4 bytes to free
 * the original allocation.
 * ------------------------------------------------------------------ */
BSTR SysAllocString(const OLECHAR* sz)
{
    UINT len;
    UINT bytes;
    unsigned char* buf;
    OLECHAR* str;
    uint32_t prefix;

    if (!sz)
        return NULL;
    len = 0;
    while (sz[len])
        len++;
    bytes = (len + 1) * (UINT)sizeof(OLECHAR);
    buf = (unsigned char*)memory_alloc((uint64_t)(4u + bytes));
    if (!buf)
        return NULL;
    prefix = len;
    memcpy(buf, &prefix, sizeof(prefix));
    str = (OLECHAR*)(buf + 4);
    memcpy(str, sz, (size_t)(len * (UINT)sizeof(OLECHAR)));
    str[len] = 0;
    return str;
}

BSTR SysAllocStringLen(const OLECHAR* sz, UINT len)
{
    UINT bytes;
    unsigned char* buf;
    OLECHAR* str;
    uint32_t prefix;

    bytes = (len + 1) * (UINT)sizeof(OLECHAR);
    buf = (unsigned char*)memory_alloc((uint64_t)(4u + bytes));
    if (!buf)
        return NULL;
    prefix = len;
    memcpy(buf, &prefix, sizeof(prefix));
    str = (OLECHAR*)(buf + 4);
    if (sz)
        memcpy(str, sz, (size_t)(len * (UINT)sizeof(OLECHAR)));
    else
        memset(str, 0, (size_t)bytes);
    str[len] = 0;
    return str;
}

VOID SysFreeString(BSTR bstr)
{
    unsigned char* buf;
    if (!bstr)
        return;
    buf = (unsigned char*)bstr - 4;
    memory_free(buf);
}

UINT SysStringLen(BSTR bstr)
{
    uint32_t prefix;
    if (!bstr)
        return 0;
    memcpy(&prefix, (unsigned char*)bstr - 4, sizeof(prefix));
    return (UINT)prefix;
}

UINT SysStringByteLen(BSTR bstr)
{
    if (!bstr)
        return 0;
    return SysStringLen(bstr) * (UINT)sizeof(OLECHAR);
}

/* ------------------------------------------------------------------ *
 * SAFEARRAY helpers
 * ------------------------------------------------------------------ */
static ULONG safearray_elem_size(VARTYPE vt)
{
    switch (vt) {
    case VT_I1:
    case VT_UI1:
        return 1;
    case VT_I2:
    case VT_UI2:
    case VT_BOOL:
        return 2;
    case VT_I4:
    case VT_UI4:
    case VT_R4:
    case VT_ERROR:
    case VT_INT:
    case VT_UINT:
        return 4;
    case VT_I8:
    case VT_UI8:
    case VT_R8:
    case VT_DATE:
    case VT_CY:
        return 8;
    case VT_VARIANT:
        return (ULONG)sizeof(VARIANT);
    case VT_BSTR:
    case VT_DISPATCH:
    case VT_UNKNOWN:
    case VT_LPSTR:
    case VT_LPWSTR:
    default:
        return (ULONG)sizeof(void*);
    }
}

SAFEARRAY* SafeArrayCreate(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* rgsabound)
{
    ULONG cbElem;
    ULONG total;
    UINT i;
    size_t saSize;
    SAFEARRAY* psa;

    if (cDims == 0 || !rgsabound)
        return NULL;

    cbElem = safearray_elem_size(vt);
    total = 1;
    for (i = 0; i < cDims; i++)
        total *= rgsabound[i].cElements;

    saSize = sizeof(SAFEARRAY) + (size_t)(cDims - 1) * sizeof(SAFEARRAYBOUND);
    psa = (SAFEARRAY*)memory_alloc((uint64_t)saSize);
    if (!psa)
        return NULL;
    memset(psa, 0, saSize);
    psa->cDims = (USHORT)cDims;
    psa->cbElements = cbElem;
    for (i = 0; i < cDims; i++)
        psa->rgsabound[i] = rgsabound[i];

    if (total > 0) {
        psa->pvData = memory_alloc((uint64_t)total * (uint64_t)cbElem);
        if (!psa->pvData) {
            memory_free(psa);
            return NULL;
        }
        memset(psa->pvData, 0, (size_t)((uint64_t)total * (uint64_t)cbElem));
    }
    return psa;
}

HRESULT SafeArrayDestroy(SAFEARRAY* psa)
{
    if (!psa)
        return E_INVALIDARG;
    if (psa->pvData)
        memory_free(psa->pvData);
    memory_free(psa);
    return S_OK;
}

static HRESULT safearray_compute_offset(SAFEARRAY* psa, LONG* rgIndices,
                                        ULONG* outOffset)
{
    ULONG offset = 0;
    ULONG stride = 1;
    UINT i;
    for (i = 0; i < (UINT)psa->cDims; i++) {
        LONG idx = rgIndices[i] - (LONG)psa->rgsabound[i].lLbound;
        if (idx < 0 || (ULONG)idx >= psa->rgsabound[i].cElements)
            return E_INVALIDARG;
        offset += (ULONG)idx * stride;
        stride *= psa->rgsabound[i].cElements;
    }
    *outOffset = offset;
    return S_OK;
}

HRESULT SafeArrayGetElement(SAFEARRAY* psa, LONG* rgIndices, void* pv)
{
    ULONG offset;
    HRESULT hr;
    if (!psa || !rgIndices || !pv)
        return E_INVALIDARG;
    hr = safearray_compute_offset(psa, rgIndices, &offset);
    if (hr < 0)
        return hr;
    memcpy(pv,
           (unsigned char*)psa->pvData + offset * psa->cbElements,
           (size_t)psa->cbElements);
    return S_OK;
}

HRESULT SafeArrayPutElement(SAFEARRAY* psa, LONG* rgIndices, void* pv)
{
    ULONG offset;
    HRESULT hr;
    if (!psa || !rgIndices || !pv)
        return E_INVALIDARG;
    hr = safearray_compute_offset(psa, rgIndices, &offset);
    if (hr < 0)
        return hr;
    memcpy((unsigned char*)psa->pvData + offset * psa->cbElements,
           pv, (size_t)psa->cbElements);
    return S_OK;
}

UINT SafeArrayGetDim(SAFEARRAY* psa)
{
    if (!psa)
        return 0;
    return (UINT)psa->cDims;
}

/* ------------------------------------------------------------------ *
 * COM initialization
 * ------------------------------------------------------------------ */
static BOOL  g_com_initialized;
static DWORD g_com_refcount;

HRESULT CoInitializeEx(void* reserved, DWORD dwCoInit)
{
    (void)reserved;
    (void)dwCoInit;
    if (!g_com_initialized) {
        g_com_initialized = TRUE;
        g_com_refcount = 1;
    } else {
        g_com_refcount++;
    }
    return S_OK;
}

VOID CoUninitialize(void)
{
    if (g_com_refcount > 0)
        g_com_refcount--;
    if (g_com_refcount == 0)
        g_com_initialized = FALSE;
}

/* ------------------------------------------------------------------ *
 * Class object registry
 * ------------------------------------------------------------------ */
#define COM_MAX_CLASSES 32

typedef struct {
    CLSID     clsid;
    IUnknown* punk;
    DWORD     dwClsContext;
    DWORD     flags;
    DWORD     dwRegister;
    int       in_use;
} com_class_entry_t;

static com_class_entry_t g_class_registry[COM_MAX_CLASSES];
static DWORD g_next_register = 1;

HRESULT CoRegisterClassObject(REFCLSID rclsid, IUnknown* pUnk,
                              DWORD dwClsContext, DWORD flags,
                              DWORD* pdwRegister)
{
    int i;
    if (!rclsid || !pUnk || !pdwRegister)
        return E_INVALIDARG;
    *pdwRegister = 0;
    for (i = 0; i < COM_MAX_CLASSES; i++) {
        if (g_class_registry[i].in_use &&
            IsEqualCLSID(rclsid, &g_class_registry[i].clsid))
            return E_FAIL;  /* already registered */
    }
    for (i = 0; i < COM_MAX_CLASSES; i++) {
        if (!g_class_registry[i].in_use) {
            g_class_registry[i].clsid = *rclsid;
            g_class_registry[i].punk = pUnk;
            g_class_registry[i].dwClsContext = dwClsContext;
            g_class_registry[i].flags = flags;
            g_class_registry[i].dwRegister = g_next_register++;
            g_class_registry[i].in_use = 1;
            *pdwRegister = g_class_registry[i].dwRegister;
            return S_OK;
        }
    }
    return E_OUTOFMEMORY;
}

HRESULT CoRevokeClassObject(DWORD dwRegister)
{
    int i;
    for (i = 0; i < COM_MAX_CLASSES; i++) {
        if (g_class_registry[i].in_use &&
            g_class_registry[i].dwRegister == dwRegister) {
            g_class_registry[i].in_use = 0;
            g_class_registry[i].punk = NULL;
            g_class_registry[i].dwRegister = 0;
            return S_OK;
        }
    }
    return E_INVALIDARG;
}

HRESULT CoGetClassObject(REFCLSID rclsid, DWORD dwClsContext,
                         void* reserved, REFIID riid, void** ppv)
{
    int i;
    (void)dwClsContext;  /* simplified: context is not enforced */
    (void)reserved;
    if (!rclsid || !riid || !ppv)
        return E_INVALIDARG;
    *ppv = NULL;
    for (i = 0; i < COM_MAX_CLASSES; i++) {
        if (g_class_registry[i].in_use &&
            IsEqualCLSID(rclsid, &g_class_registry[i].clsid)) {
            IUnknown* punk = g_class_registry[i].punk;
            if (!punk || !punk->lpVtbl || !punk->lpVtbl->QueryInterface)
                return E_FAIL;
            return punk->lpVtbl->QueryInterface(punk, riid, ppv);
        }
    }
    return REGDB_E_CLASSNOTREG;
}

HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown* pUnkOuter,
                         DWORD dwClsContext, REFIID riid, void** ppv)
{
    IClassFactory* pFactory = NULL;
    HRESULT hr;
    if (!rclsid || !riid || !ppv)
        return E_INVALIDARG;
    *ppv = NULL;
    hr = CoGetClassObject(rclsid, dwClsContext, NULL,
                          &g_IID_IClassFactory, (void**)&pFactory);
    if (hr < 0)
        return hr;
    if (!pFactory || !pFactory->lpVtbl || !pFactory->lpVtbl->CreateInstance) {
        if (pFactory && pFactory->lpVtbl && pFactory->lpVtbl->Release)
            pFactory->lpVtbl->Release((IUnknown*)pFactory);
        return E_FAIL;
    }
    hr = pFactory->lpVtbl->CreateInstance(pFactory, pUnkOuter, riid, ppv);
    pFactory->lpVtbl->Release((IUnknown*)pFactory);
    return hr;
}

/* ------------------------------------------------------------------ *
 * Type library registry
 * ------------------------------------------------------------------ */
#define COM_MAX_TYPELIBS 16

typedef struct {
    TYPELIB tlib;
    char    path[MAX_PATH];
    int     in_use;
} com_typelib_entry_t;

static com_typelib_entry_t g_typelib_registry[COM_MAX_TYPELIBS];

HRESULT RegisterTypeLib(TYPELIB* tlib, const char* path)
{
    int i;
    if (!tlib)
        return E_INVALIDARG;
    for (i = 0; i < COM_MAX_TYPELIBS; i++) {
        if (!g_typelib_registry[i].in_use) {
            g_typelib_registry[i].tlib = *tlib;
            if (path) {
                strncpy(g_typelib_registry[i].path, path, MAX_PATH - 1);
                g_typelib_registry[i].path[MAX_PATH - 1] = 0;
            } else {
                g_typelib_registry[i].path[0] = 0;
            }
            g_typelib_registry[i].in_use = 1;
            return S_OK;
        }
    }
    return E_OUTOFMEMORY;
}

HRESULT LoadTypeLib(const char* path, TYPELIB** pptlib)
{
    int i;
    if (!path || !pptlib)
        return E_INVALIDARG;
    *pptlib = NULL;
    for (i = 0; i < COM_MAX_TYPELIBS; i++) {
        if (g_typelib_registry[i].in_use &&
            strcmp(g_typelib_registry[i].path, path) == 0) {
            *pptlib = &g_typelib_registry[i].tlib;
            return S_OK;
        }
    }
    return E_FAIL;
}

UINT GetTypeLibCount(void)
{
    UINT count = 0;
    int i;
    for (i = 0; i < COM_MAX_TYPELIBS; i++)
        if (g_typelib_registry[i].in_use)
            count++;
    return count;
}

TYPELIB* GetTypeLibByIndex(UINT index)
{
    UINT n = 0;
    int i;
    for (i = 0; i < COM_MAX_TYPELIBS; i++) {
        if (g_typelib_registry[i].in_use) {
            if (n == index)
                return &g_typelib_registry[i].tlib;
            n++;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * CLSID / ProgID string helpers
 * ------------------------------------------------------------------ */
static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

HRESULT CLSIDFromString(const char* lpsz, LPCLSID pclsid)
{
    const char* s;
    uint32_t d1 = 0;
    uint16_t d2 = 0;
    uint16_t d3 = 0;
    int i;

    if (!lpsz || !pclsid)
        return E_INVALIDARG;
    memset(pclsid, 0, sizeof(CLSID));
    s = lpsz;
    if (*s == '{')
        s++;

    /* Data1: 8 hex digits */
    for (i = 0; i < 8; i++) {
        int v = hex_value(*s++);
        if (v < 0)
            return E_FAIL;
        d1 = (d1 << 4) | (uint32_t)v;
    }
    if (*s++ != '-')
        return E_FAIL;
    /* Data2: 4 hex digits */
    for (i = 0; i < 4; i++) {
        int v = hex_value(*s++);
        if (v < 0)
            return E_FAIL;
        d2 = (uint16_t)((d2 << 4) | (uint16_t)v);
    }
    if (*s++ != '-')
        return E_FAIL;
    /* Data3: 4 hex digits */
    for (i = 0; i < 4; i++) {
        int v = hex_value(*s++);
        if (v < 0)
            return E_FAIL;
        d3 = (uint16_t)((d3 << 4) | (uint16_t)v);
    }
    if (*s++ != '-')
        return E_FAIL;
    /* Data4[0..1]: 4 hex digits */
    for (i = 0; i < 2; i++) {
        int hi = hex_value(*s++);
        int lo = hex_value(*s++);
        if (hi < 0 || lo < 0)
            return E_FAIL;
        pclsid->Data4[i] = (uint8_t)((hi << 4) | lo);
    }
    if (*s++ != '-')
        return E_FAIL;
    /* Data4[2..7]: 12 hex digits */
    for (i = 2; i < 8; i++) {
        int hi = hex_value(*s++);
        int lo = hex_value(*s++);
        if (hi < 0 || lo < 0)
            return E_FAIL;
        pclsid->Data4[i] = (uint8_t)((hi << 4) | lo);
    }
    pclsid->Data1 = d1;
    pclsid->Data2 = d2;
    pclsid->Data3 = d3;
    return S_OK;
}

HRESULT StringFromCLSID(REFCLSID rclsid, char** lplpsz)
{
    char* buf;
    if (!rclsid || !lplpsz)
        return E_INVALIDARG;
    *lplpsz = NULL;
    /* "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}" + NUL = 39 */
    buf = (char*)memory_alloc(40);
    if (!buf)
        return E_OUTOFMEMORY;
    sprintf(buf,
            "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            (unsigned int)rclsid->Data1,
            (unsigned int)rclsid->Data2,
            (unsigned int)rclsid->Data3,
            (unsigned int)rclsid->Data4[0],
            (unsigned int)rclsid->Data4[1],
            (unsigned int)rclsid->Data4[2],
            (unsigned int)rclsid->Data4[3],
            (unsigned int)rclsid->Data4[4],
            (unsigned int)rclsid->Data4[5],
            (unsigned int)rclsid->Data4[6],
            (unsigned int)rclsid->Data4[7]);
    *lplpsz = buf;
    return S_OK;
}

/* ------------------------------------------------------------------ *
 * ProgID registry
 * ------------------------------------------------------------------ */
#define COM_MAX_PROGIDS 32

typedef struct {
    CLSID clsid;
    char  progid[128];
    int   in_use;
} com_progid_entry_t;

static com_progid_entry_t g_progid_registry[COM_MAX_PROGIDS];

static void com_register_progid(REFCLSID rclsid, const char* progid)
{
    int i;
    for (i = 0; i < COM_MAX_PROGIDS; i++) {
        if (!g_progid_registry[i].in_use) {
            g_progid_registry[i].clsid = *rclsid;
            strncpy(g_progid_registry[i].progid, progid, 127);
            g_progid_registry[i].progid[127] = 0;
            g_progid_registry[i].in_use = 1;
            return;
        }
    }
}

HRESULT CLSIDFromProgID(const char* progid, LPCLSID pclsid)
{
    int i;
    if (!progid || !pclsid)
        return E_INVALIDARG;
    memset(pclsid, 0, sizeof(CLSID));
    for (i = 0; i < COM_MAX_PROGIDS; i++) {
        if (g_progid_registry[i].in_use &&
            strcmp(g_progid_registry[i].progid, progid) == 0) {
            *pclsid = g_progid_registry[i].clsid;
            return S_OK;
        }
    }
    return E_FAIL;
}

HRESULT ProgIDFromCLSID(REFCLSID rclsid, char** lplpsz)
{
    int i;
    if (!rclsid || !lplpsz)
        return E_INVALIDARG;
    *lplpsz = NULL;
    for (i = 0; i < COM_MAX_PROGIDS; i++) {
        if (g_progid_registry[i].in_use &&
            IsEqualCLSID(rclsid, &g_progid_registry[i].clsid)) {
            char* buf = (char*)memory_alloc(128);
            if (!buf)
                return E_OUTOFMEMORY;
            strcpy(buf, g_progid_registry[i].progid);
            *lplpsz = buf;
            return S_OK;
        }
    }
    return E_FAIL;
}

/* ------------------------------------------------------------------ *
 * Dummy IClassFactory for the built-in scripting objects.
 *
 * QueryInterface/AddRef/Release behave enough like a real factory for
 * CoGetClassObject() to succeed; CreateInstance is a stub that returns
 * E_NOTIMPL because no backing object is implemented in this kernel.
 * ------------------------------------------------------------------ */
static HRESULT dummy_QueryInterface(IUnknown* This, REFIID riid, void** ppv)
{
    if (!ppv)
        return E_POINTER;
    *ppv = NULL;
    if (!This)
        return E_POINTER;
    if (IsEqualIID(riid, &g_IID_IUnknown) ||
        IsEqualIID(riid, &g_IID_IClassFactory)) {
        *ppv = This;
        This->lpVtbl->AddRef(This);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG dummy_AddRef(IUnknown* This)
{
    (void)This;
    return 1;
}

static ULONG dummy_Release(IUnknown* This)
{
    (void)This;
    return 1;
}

static HRESULT dummy_CreateInstance(IClassFactory* This, IUnknown* pOuter,
                                    REFIID riid, void** ppv)
{
    (void)This;
    (void)pOuter;
    (void)riid;
    if (ppv)
        *ppv = NULL;
    return E_NOTIMPL;
}

static HRESULT dummy_LockServer(IClassFactory* This, BOOL fLock)
{
    (void)This;
    (void)fLock;
    return S_OK;
}

static const IClassFactoryVtbl dummy_factory_vtbl = {
    dummy_QueryInterface,
    dummy_AddRef,
    dummy_Release,
    dummy_CreateInstance,
    dummy_LockServer
};

static IClassFactory g_dummy_factories[3];

/* ------------------------------------------------------------------ *
 * COM subsystem initialization
 * ------------------------------------------------------------------ */
int com_init(void)
{
    static const CLSID clsid_fso = {
        0x00000000, 0x0000, 0x0000,
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }
    };
    static const CLSID clsid_shell = {
        0x00000000, 0x0000, 0x0000,
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }
    };
    static const CLSID clsid_adodb = {
        0x00000000, 0x0000, 0x0000,
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 }
    };
    DWORD reg = 0;
    int i;

    memset(g_class_registry, 0, sizeof(g_class_registry));
    memset(g_typelib_registry, 0, sizeof(g_typelib_registry));
    memset(g_progid_registry, 0, sizeof(g_progid_registry));
    for (i = 0; i < 3; i++)
        g_dummy_factories[i].lpVtbl = &dummy_factory_vtbl;
    g_com_initialized = FALSE;
    g_com_refcount = 0;
    g_next_register = 1;

    CoRegisterClassObject(&clsid_fso, (IUnknown*)&g_dummy_factories[0],
                          CLSCTX_INPROC_SERVER, 0, &reg);
    CoRegisterClassObject(&clsid_shell, (IUnknown*)&g_dummy_factories[1],
                          CLSCTX_INPROC_SERVER, 0, &reg);
    CoRegisterClassObject(&clsid_adodb, (IUnknown*)&g_dummy_factories[2],
                          CLSCTX_INPROC_SERVER, 0, &reg);

    com_register_progid(&clsid_fso, "Scripting.FileSystemObject");
    com_register_progid(&clsid_shell, "WScript.Shell");
    com_register_progid(&clsid_adodb, "ADODB.Connection");

    return 0;
}
