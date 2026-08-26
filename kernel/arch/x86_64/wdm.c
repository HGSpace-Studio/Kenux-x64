/*
 * WDM (Windows Driver Model) subsystem - freestanding kernel implementation.
 *
 * Drivers, devices and IRPs are managed through fixed-size static pools so
 * the code needs no dynamic allocator.  All operations are data-structure
 * management only; no real hardware is touched.
 */

/* win32.h references a handful of pointer/char typedefs that are not yet
 * defined in the header itself.  Provide them up front so the header parses
 * cleanly (these are the standard Windows spellings). */
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

/* ------------------------------------------------------------------ *
 * Constants
 * ------------------------------------------------------------------ */
#define WDM_MAX_DRIVERS    32
#define WDM_MAX_DEVICES    64
#define WDM_MAX_IRPS       128
#define WDM_MAX_SYMLINKS   32
#define WDM_IRP_MAX_STACK  8
#define WDM_DEV_EXT_SIZE   256

/* NT object type tags (mirror of IO_TYPE_* constants) */
#define WDM_IO_TYPE_IRP       0x0006
#define WDM_IO_TYPE_DEVICE    0x0003
#define WDM_IO_TYPE_DRIVER    0x0004

/* Internal IRP state bits kept in Irp->Flags */
#define WDM_IRP_PENDING       0x00010000u
#define WDM_IRP_COMPLETED     0x00020000u

/* ------------------------------------------------------------------ *
 * Pools
 * ------------------------------------------------------------------ */

/* Driver registry (1:1 with the driver-object pool below). */
static wdm_driver_entry_t g_drivers[WDM_MAX_DRIVERS];
static DRIVER_OBJECT      g_driver_objects[WDM_MAX_DRIVERS];
static int                g_driver_count;
static BOOL               g_initialized;

/* Device object pool.  Each slot carries an optional device extension. */
typedef struct {
    DEVICE_OBJECT obj;
    int           in_use;
    UCHAR         extension[WDM_DEV_EXT_SIZE];
} wdm_device_slot_t;
static wdm_device_slot_t g_device_pool[WDM_MAX_DEVICES];

/* IRP pool.  Each slot carries a small fixed stack-location array. */
typedef struct {
    IRP               irp;
    int               in_use;
    IO_STACK_LOCATION stack[WDM_IRP_MAX_STACK];
} wdm_irp_slot_t;
static wdm_irp_slot_t g_irp_pool[WDM_MAX_IRPS];

/* Symbolic link table: maps symbolic name -> device name (ASCII). */
typedef struct {
    int  in_use;
    char sym[256];
    char dev[256];
} wdm_symlink_t;
static wdm_symlink_t g_symlinks[WDM_MAX_SYMLINKS];

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

static void wdm_unicode_to_ascii(PUNICODE_STRING u, char* dst, size_t dstsz) {
    size_t i;
    size_t n;
    if (dst == NULL || dstsz == 0) return;
    dst[0] = 0;
    if (u == NULL || u->Buffer == NULL) return;
    n = (size_t)u->Length / sizeof(WCHAR);
    if (n >= dstsz) n = dstsz - 1u;
    for (i = 0; i < n; i++) {
        WCHAR wc = u->Buffer[i];
        dst[i] = (wc < 128) ? (char)wc : '?';
    }
    dst[n] = 0;
}

static int wdm_find_free_driver_slot(void) {
    int i;
    for (i = 0; i < WDM_MAX_DRIVERS; i++) {
        if (!g_drivers[i].loaded) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ *
 * Device object management
 * ------------------------------------------------------------------ */

NTSTATUS IoCreateDevice(PDRIVER_OBJECT DriverObject, ULONG DeviceExtensionSize,
                        PUNICODE_STRING DeviceName, ULONG DeviceType,
                        ULONG DeviceCharacteristics, BOOL Exclusive,
                        PDEVICE_OBJECT* DeviceObject) {
    int i;
    (void)DeviceName;
    (void)Exclusive;
    if (DeviceObject == NULL) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < WDM_MAX_DEVICES; i++) {
        wdm_device_slot_t* slot;
        DEVICE_OBJECT* dev;
        if (g_device_pool[i].in_use) continue;
        slot = &g_device_pool[i];
        dev = &slot->obj;
        memset(dev, 0, sizeof(DEVICE_OBJECT));
        dev->Type            = WDM_IO_TYPE_DEVICE;
        dev->Size            = (USHORT)sizeof(DEVICE_OBJECT);
        dev->DriverObject    = DriverObject;
        dev->DeviceType      = (UCHAR)DeviceType;
        dev->StackSize       = 1;
        dev->Flags           = DO_BUFFERED_IO;
        dev->Characteristics = DeviceCharacteristics;
        if (DeviceExtensionSize > 0U &&
            DeviceExtensionSize <= (ULONG)WDM_DEV_EXT_SIZE) {
            dev->DeviceExtension = slot->extension;
        }
        /* Link into the driver's device chain. */
        if (DriverObject != NULL) {
            dev->NextDevice = DriverObject->DeviceObject;
            DriverObject->DeviceObject = dev;
        }
        slot->in_use = 1;
        *DeviceObject = dev;
        return STATUS_SUCCESS;
    }
    return STATUS_NO_MEMORY;
}

VOID IoDeleteDevice(PDEVICE_OBJECT DeviceObject) {
    int i;
    PDRIVER_OBJECT drv;
    PDEVICE_OBJECT* pp;
    if (DeviceObject == NULL) return;
    /* Unlink from the driver's device chain. */
    drv = DeviceObject->DriverObject;
    if (drv != NULL) {
        pp = &drv->DeviceObject;
        while (*pp != NULL) {
            if (*pp == DeviceObject) {
                *pp = DeviceObject->NextDevice;
                break;
            }
            pp = &(*pp)->NextDevice;
        }
    }
    /* Return the slot to the pool. */
    for (i = 0; i < WDM_MAX_DEVICES; i++) {
        if (&g_device_pool[i].obj == DeviceObject) {
            g_device_pool[i].in_use = 0;
            memset(&g_device_pool[i].obj, 0, sizeof(DEVICE_OBJECT));
            return;
        }
    }
}

/* ------------------------------------------------------------------ *
 * Symbolic links
 * ------------------------------------------------------------------ */

NTSTATUS IoCreateSymbolicLink(PUNICODE_STRING SymbolicLinkName,
                              PUNICODE_STRING DeviceName) {
    int i;
    char sym[256];
    char dev[256];
    if (SymbolicLinkName == NULL || DeviceName == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    wdm_unicode_to_ascii(SymbolicLinkName, sym, sizeof(sym));
    wdm_unicode_to_ascii(DeviceName, dev, sizeof(dev));
    for (i = 0; i < WDM_MAX_SYMLINKS; i++) {
        if (g_symlinks[i].in_use && strcmp(g_symlinks[i].sym, sym) == 0) {
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }
    for (i = 0; i < WDM_MAX_SYMLINKS; i++) {
        if (!g_symlinks[i].in_use) {
            g_symlinks[i].in_use = 1;
            strcpy(g_symlinks[i].sym, sym);
            strcpy(g_symlinks[i].dev, dev);
            return STATUS_SUCCESS;
        }
    }
    return STATUS_NO_MEMORY;
}

VOID IoDeleteSymbolicLink(PUNICODE_STRING SymbolicLinkName) {
    int i;
    char sym[256];
    if (SymbolicLinkName == NULL) return;
    wdm_unicode_to_ascii(SymbolicLinkName, sym, sizeof(sym));
    for (i = 0; i < WDM_MAX_SYMLINKS; i++) {
        if (g_symlinks[i].in_use && strcmp(g_symlinks[i].sym, sym) == 0) {
            g_symlinks[i].in_use = 0;
            g_symlinks[i].sym[0] = 0;
            g_symlinks[i].dev[0] = 0;
            return;
        }
    }
}

/* ------------------------------------------------------------------ *
 * IRP stack location helpers
 * ------------------------------------------------------------------ */

PVOID IoGetCurrentIrpStackLocation(PIRP Irp) {
    if (Irp == NULL) return NULL;
    return (PVOID)Irp->CurrentStackLocation;
}

PVOID IoGetNextIrpStackLocation(PIRP Irp) {
    if (Irp == NULL || Irp->CurrentStackLocation == NULL) return NULL;
    return (PVOID)(Irp->CurrentStackLocation - 1);
}

VOID IoSkipCurrentIrpStackLocation(PIRP Irp) {
    if (Irp == NULL || Irp->CurrentStackLocation == NULL) return;
    Irp->CurrentStackLocation--;
}

VOID IoMarkIrpPending(PIRP Irp) {
    if (Irp == NULL) return;
    Irp->Flags |= WDM_IRP_PENDING;
}

/* ------------------------------------------------------------------ *
 * IRP dispatch / completion
 * ------------------------------------------------------------------ */

NTSTATUS IoCallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stk;
    UCHAR major;
    PDRIVER_OBJECT drv;
    if (Irp == NULL) return STATUS_INVALID_PARAMETER;
    if (DeviceObject == NULL) {
        Irp->u.IoStatus.DUMMYUNIONNAME.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }
    Irp->DeviceObject = DeviceObject;
    stk = (PIO_STACK_LOCATION)IoGetCurrentIrpStackLocation(Irp);
    if (stk == NULL) {
        Irp->u.IoStatus.DUMMYUNIONNAME.Status = STATUS_INVALID_PARAMETER;
        return STATUS_INVALID_PARAMETER;
    }
    major = stk->MajorFunction;
    drv = DeviceObject->DriverObject;
    if (drv != NULL && major < 28 && drv->MajorFunction[major] != NULL) {
        NTSTATUS status = drv->MajorFunction[major](DeviceObject, Irp);
        Irp->u.IoStatus.DUMMYUNIONNAME.Status = status;
        return status;
    }
    Irp->u.IoStatus.DUMMYUNIONNAME.Status = STATUS_NOT_IMPLEMENTED;
    return STATUS_NOT_IMPLEMENTED;
}

VOID IoCompleteRequest(PIRP Irp, UCHAR PriorityBoost) {
    (void)PriorityBoost;
    if (Irp == NULL) return;
    Irp->Flags |= WDM_IRP_COMPLETED;
}

/* ------------------------------------------------------------------ *
 * IRP allocation
 * ------------------------------------------------------------------ */

PIRP IoAllocateIrp(CCHAR StackSize, BOOL ChargeQuota) {
    int i;
    int ss;
    (void)ChargeQuota;
    ss = (int)StackSize;
    if (ss < 1) ss = 1;
    if (ss > WDM_IRP_MAX_STACK) ss = WDM_IRP_MAX_STACK;
    for (i = 0; i < WDM_MAX_IRPS; i++) {
        wdm_irp_slot_t* slot;
        IRP* irp;
        if (g_irp_pool[i].in_use) continue;
        slot = &g_irp_pool[i];
        irp = &slot->irp;
        memset(slot, 0, sizeof(*slot));
        irp->Type = WDM_IO_TYPE_IRP;
        irp->Size = (USHORT)(sizeof(IRP) + (size_t)ss * sizeof(IO_STACK_LOCATION));
        /* Stack locations live in slot->stack; "current" starts at the
         * topmost location so IoGetNextIrpStackLocation() (current - 1)
         * and IoSkipCurrentIrpStackLocation() (decrement) behave like the
         * real downward-growing IRP stack. */
        irp->CurrentStackLocation = &slot->stack[ss - 1];
        slot->in_use = 1;
        return irp;
    }
    return NULL;
}

VOID IoFreeIrp(PIRP Irp) {
    int i;
    if (Irp == NULL) return;
    for (i = 0; i < WDM_MAX_IRPS; i++) {
        if (&g_irp_pool[i].irp == Irp) {
            g_irp_pool[i].in_use = 0;
            return;
        }
    }
}

/* ------------------------------------------------------------------ *
 * Driver registration / device stacks
 * ------------------------------------------------------------------ */

static NTSTATUS wdm_register_internal(const char* name_ascii,
                                      PUNICODE_STRING name_uni,
                                      PDRIVER_INITIALIZE init) {
    int slot;
    PDRIVER_OBJECT drv;
    wdm_driver_entry_t* entry;

    slot = wdm_find_free_driver_slot();
    if (slot < 0) return STATUS_UNSUCCESSFUL;

    drv = &g_driver_objects[slot];
    memset(drv, 0, sizeof(DRIVER_OBJECT));
    drv->Type = WDM_IO_TYPE_DRIVER;
    drv->Size = (USHORT)sizeof(DRIVER_OBJECT);
    drv->DriverInit = (PVOID)init;
    if (name_uni != NULL) {
        drv->DriverName = *name_uni;
    }

    entry = &g_drivers[slot];
    memset(entry, 0, sizeof(*entry));
    if (name_ascii != NULL) {
        size_t nlen = strlen(name_ascii);
        if (nlen < sizeof(entry->name)) {
            sprintf(entry->name, "%s", name_ascii);
        } else {
            size_t fit = sizeof(entry->name) - 1u;
            memcpy(entry->name, name_ascii, fit);
            entry->name[fit] = 0;
        }
    } else if (name_uni != NULL) {
        wdm_unicode_to_ascii(name_uni, entry->name, sizeof(entry->name));
    }
    entry->driver_obj  = drv;
    entry->driver_init = init;
    entry->loaded      = 1;
    g_driver_count++;

    if (init != NULL) {
        init(drv, name_uni);
    }
    return STATUS_SUCCESS;
}

NTSTATUS IoRegisterDriver(PDRIVER_INITIALIZE init, PUNICODE_STRING name) {
    return wdm_register_internal(NULL, name, init);
}

NTSTATUS IoAttachDeviceToDeviceStack(PDEVICE_OBJECT SourceDevice,
                                     PDEVICE_OBJECT TargetDevice) {
    if (SourceDevice == NULL || TargetDevice == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    SourceDevice->AttachedDevice = TargetDevice;
    return STATUS_SUCCESS;
}

PDEVICE_OBJECT IoGetAttachedDevice(PDEVICE_OBJECT DeviceObject) {
    PDEVICE_OBJECT top;
    if (DeviceObject == NULL) return NULL;
    top = DeviceObject;
    while (top->AttachedDevice != NULL) {
        top = top->AttachedDevice;
    }
    return top;
}

/* ------------------------------------------------------------------ *
 * Built-in dummy drivers
 * ------------------------------------------------------------------ */

static NTSTATUS wdm_disk_init(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    return STATUS_SUCCESS;
}
static NTSTATUS wdm_mouse_init(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    return STATUS_SUCCESS;
}
static NTSTATUS wdm_keyboard_init(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    return STATUS_SUCCESS;
}
static NTSTATUS wdm_network_init(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    return STATUS_SUCCESS;
}
static NTSTATUS wdm_display_init(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 * Public loader API
 * ------------------------------------------------------------------ */

NTSTATUS wdm_load_driver(const char* name, PDRIVER_INITIALIZE init_func) {
    if (name == NULL) return STATUS_INVALID_PARAMETER;
    return wdm_register_internal(name, NULL, init_func);
}

NTSTATUS wdm_unload_driver(const char* name) {
    int i;
    if (name == NULL) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < WDM_MAX_DRIVERS; i++) {
        if (g_drivers[i].loaded && strcmp(g_drivers[i].name, name) == 0) {
            if (g_drivers[i].unload != NULL) {
                g_drivers[i].unload(g_drivers[i].driver_obj);
            }
            g_drivers[i].loaded = 0;
            g_drivers[i].driver_obj = NULL;
            if (g_driver_count > 0) g_driver_count--;
            return STATUS_SUCCESS;
        }
    }
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

int wdm_get_driver_count(void) {
    return g_driver_count;
}

void wdm_list_drivers(void (*cb)(const char* name, int loaded)) {
    int i;
    if (cb == NULL) return;
    for (i = 0; i < WDM_MAX_DRIVERS; i++) {
        if (g_drivers[i].name[0] != 0) {
            cb(g_drivers[i].name, g_drivers[i].loaded);
        }
    }
}

/* ------------------------------------------------------------------ *
 * Subsystem initialization
 * ------------------------------------------------------------------ */

int wdm_init(void) {
    if (g_initialized) return 0;
    memset(g_drivers, 0, sizeof(g_drivers));
    memset(g_driver_objects, 0, sizeof(g_driver_objects));
    memset(g_device_pool, 0, sizeof(g_device_pool));
    memset(g_irp_pool, 0, sizeof(g_irp_pool));
    memset(g_symlinks, 0, sizeof(g_symlinks));
    g_driver_count = 0;

    wdm_load_driver("DiskDriver", wdm_disk_init);
    wdm_load_driver("MouseDriver", wdm_mouse_init);
    wdm_load_driver("KeyboardDriver", wdm_keyboard_init);
    wdm_load_driver("NetworkDriver", wdm_network_init);
    wdm_load_driver("DisplayDriver", wdm_display_init);

    g_initialized = TRUE;
    return 0;
}
