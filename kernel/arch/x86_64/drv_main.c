/*
 * drv_main.c - Device driver framework initialization
 *
 * Calls all built-in driver init functions and registers them
 * with the WDM driver framework.
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
#include <stdio.h>

/* WDM DriverEntry wrappers */
static NTSTATUS display_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    display_driver_init();
    return STATUS_SUCCESS;
}

static NTSTATUS disk_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    disk_driver_init();
    return STATUS_SUCCESS;
}

static NTSTATUS network_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    network_driver_init();
    return STATUS_SUCCESS;
}

static NTSTATUS hid_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    hid_driver_init();
    return STATUS_SUCCESS;
}

static NTSTATUS usb_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    usb_driver_init();
    return STATUS_SUCCESS;
}

static NTSTATUS audio_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    audio_driver_init();
    return STATUS_SUCCESS;
}

static NTSTATUS serial_drv_entry(PDRIVER_OBJECT drv, PUNICODE_STRING name) {
    (void)drv; (void)name;
    serial_driver_init();
    parallel_driver_init();
    return STATUS_SUCCESS;
}

/*
 * drivers_init_all - Initialize all built-in device drivers
 *
 * Called from kernel.c after WDM framework init.
 * Each driver is initialized and registered with WDM.
 */
int drivers_init_all(void) {
    int rc;
    int ok = 0;
    int total = 7;

    /* Display driver */
    rc = display_driver_init();
    if (rc == 0) {
        wdm_load_driver("\\Driver\\Display", display_drv_entry);
        ok++;
    }

    /* Disk/block driver */
    rc = disk_driver_init();
    if (rc == 0) {
        wdm_load_driver("\\Driver\\Disk", disk_drv_entry);
        ok++;
    }

    /* Network/NDIS driver */
    rc = network_driver_init();
    if (rc == 0) {
        wdm_load_driver("\\Driver\\NDIS", network_drv_entry);
        ok++;
    }

    /* HID class driver */
    rc = hid_driver_init();
    if (rc == 0) {
        wdm_load_driver("\\Driver\\HID", hid_drv_entry);
        ok++;
    }

    /* USB bus driver */
    rc = usb_driver_init();
    if (rc == 0) {
        wdm_load_driver("\\Driver\\USB", usb_drv_entry);
        ok++;
    }

    /* Audio driver */
    rc = audio_driver_init();
    if (rc == 0) {
        wdm_load_driver("\\Driver\\Audio", audio_drv_entry);
        ok++;
    }

    /* Serial/Parallel driver */
    rc = serial_driver_init();
    if (rc == 0) {
        parallel_driver_init();
        wdm_load_driver("\\Driver\\Serial", serial_drv_entry);
        ok++;
    }

    (void)total;
    return (ok > 0) ? 0 : -1;
}
