/*
 * compat_main.c - Application Compatibility Layer unified initialization
 *
 * Binds the ReactOS compatibility layer (D:\KenuxK\compat_layer) to the
 * KenuxK kernel.  Initializes:
 *   1. SDB Shim Database  (ReactOS dll/appcompat/apphelp/hsdb.c)
 *   2. Shim Engine        (ReactOS dll/appcompat/apphelp/shimeng.c)
 *   3. NTVDM 16-bit DOS   (ReactOS subsystems/mvdm/ntvdm/)
 *   4. WOW64 32-bit bridge(ReactOS sdk/lib/rtl/wow64.c)
 *   5. Version-lie shims  (ReactOS dll/appcompat/shims/layer/versionlie.c)
 *
 * Version-lie data and TAG constants are sourced verbatim from the ReactOS
 * source tree so that GetVersion()/GetVersionEx() lies match real Windows.
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

/* SDB database handle for the main system database */
static HSDB g_main_sdb;

/*
 * compat_layer_init - Initialize the full application compatibility layer
 *
 * Called from kernel.c after Win32 subsystems and device drivers are ready.
 * Sets up:
 *   1. SDB Shim Database (in-memory)
 *   2. Shim Engine (API hooking framework)
 *   3. NTVDM (16-bit DOS virtual machine)
 *   4. WOW64 (32-bit on 64-bit process bridging)
 *   5. Pre-registered compatibility databases
 *
 * Returns 0 on success, -1 on failure.
 */
int compat_layer_init(void) {
    int rc;

    /* 1. Initialize SDB Database (ReactOS: SdbInitDatabase) */
    g_main_sdb = sdb_init_database(HID_DOS_PATHS | SDB_DATABASE_MAIN_SHIM);
    if (g_main_sdb == NULL) {
        return -1;
    }

    /* Open a simulated system database */
    rc = sdb_open_database(g_main_sdb, L"\\SystemRoot\\AppPatch\\sysmain.sdb");
    if (rc != 0) {
        /* Non-fatal - database is optional */
    }

    /* Register a few well-known compatibility databases */
    GUID guid_shim = {0x11111111, 0x1111, 0x1111, {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};
    GUID guid_msi  = {0xD8FF6D16, 0x6A3A, 0x468A, {0x8B, 0x44, 0x01, 0x71, 0x4D, 0xDC, 0x49, 0xEA}};

    sdb_register_database(L"\\SystemRoot\\AppPatch\\sysmain.sdb", &guid_shim);
    sdb_register_database(L"\\SystemRoot\\AppPatch\\msimain.sdb", &guid_msi);

    /* 2. Initialize Shim Engine */
    rc = shim_engine_init();
    if (rc != 0) {
        return -1;
    }

    /* Pre-apply common version lie shims for known problematic apps */
    /* The shim engine is now ready to hook APIs for any process */

    /* 3. Initialize NTVDM (16-bit DOS emulator) */
    rc = ntvdm_init();
    if (rc != 0) {
        /* Non-fatal - NTVDM is optional */
    }

    /* 4. Initialize WOW64 (32-bit on 64-bit bridging) */
    rc = wow64_init();
    if (rc != 0) {
        /* Non-fatal */
    }

    return 0;
}
