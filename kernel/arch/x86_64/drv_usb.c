/*
 * USB bus driver - freestanding kernel implementation.
 *
 * Host controllers and attached devices are managed through fixed-size static
 * pools so the code needs no dynamic allocator.  All transfers are simulated:
 * no real hardware is touched, every operation is data-structure management.
 *
 * The public API and the USB_SPEED / USB_DEVICE / USB_HOST_CONTROLLER types
 * are declared in <arch/win32.h>; this translation unit provides the
 * implementations.
 */

/* win32.h references a handful of pointer/char typedefs that are not yet
 * defined in the header itself.  Provide them up front so the header parses
 * cleanly (these are the standard Windows spellings). */
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

/* ------------------------------------------------------------------ *
 * Constants
 * ------------------------------------------------------------------ */
#define USB_MAX_CONTROLLERS         4
#define USB_MAX_DEVICES_PER_CTRL    32      /* must match USB_HOST_CONTROLLER.devices[] */

/* Host controller types (see USB_HOST_CONTROLLER.type comment in win32.h) */
#define USB_CTRL_TYPE_UHCI          0u
#define USB_CTRL_TYPE_OHCI          1u
#define USB_CTRL_TYPE_EHCI          2u
#define USB_CTRL_TYPE_XHCI          3u

/* USB standard device requests (bmRequestType.Type=Standard) */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09

/* USB descriptor types (high byte of wValue in GET_DESCRIPTOR) */
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIGURATION 0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05

/* ------------------------------------------------------------------ *
 * Pools
 * ------------------------------------------------------------------ */
static USB_HOST_CONTROLLER g_controllers[USB_MAX_CONTROLLERS];
static int                 g_controller_count;
static uint32_t            g_next_device_id = 1;

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

/* Locate a device by its global device id across every registered
 * controller.  Returns NULL when the id is unknown.  A device slot whose
 * device_id is 0 is considered free. */
static USB_DEVICE* usb_find_device_internal(uint32_t device_id) {
    int c, d;
    if (device_id == 0) return NULL;
    for (c = 0; c < USB_MAX_CONTROLLERS; c++) {
        USB_HOST_CONTROLLER* ctrl = &g_controllers[c];
        if (!ctrl->initialized) continue;
        for (d = 0; d < USB_MAX_DEVICES_PER_CTRL; d++) {
            USB_DEVICE* dev = &ctrl->devices[d];
            if (dev->device_id == device_id) {
                return dev;
            }
        }
    }
    return NULL;
}

/* Fill in the identifying attributes of a previously enumerated device. */
static void usb_set_device_info(uint32_t device_id, uint16_t vid, uint16_t pid,
                                uint8_t dev_class, const char* product) {
    USB_DEVICE* dev = usb_find_device_internal(device_id);
    if (dev == NULL) return;
    dev->vendor_id    = vid;
    dev->product_id   = pid;
    dev->device_class = dev_class;
    if (product != NULL) {
        strncpy(dev->product, product, sizeof(dev->product) - 1);
        dev->product[sizeof(dev->product) - 1] = 0;
    }
}

/* ------------------------------------------------------------------ *
 * Controller management
 * ------------------------------------------------------------------ */

int usb_register_controller(const char* name, uint32_t type, uint32_t num_ports) {
    int i;
    for (i = 0; i < USB_MAX_CONTROLLERS; i++) {
        USB_HOST_CONTROLLER* ctrl;
        if (g_controllers[i].initialized) continue;
        ctrl = &g_controllers[i];
        memset(ctrl, 0, sizeof(*ctrl));
        if (name != NULL) {
            strncpy(ctrl->name, name, sizeof(ctrl->name) - 1);
            ctrl->name[sizeof(ctrl->name) - 1] = 0;
        }
        ctrl->controller_id = (uint32_t)i;
        ctrl->type          = type;
        ctrl->num_ports     = num_ports;
        ctrl->initialized   = 1;
        ctrl->num_devices   = 0;
        if (i >= g_controller_count) g_controller_count = i + 1;
        return (int)ctrl->controller_id;
    }
    return -1;
}

USB_HOST_CONTROLLER* usb_get_controller(uint32_t controller_id) {
    if (controller_id >= (uint32_t)USB_MAX_CONTROLLERS) return NULL;
    if (!g_controllers[controller_id].initialized) return NULL;
    return &g_controllers[controller_id];
}

/* ------------------------------------------------------------------ *
 * Device enumeration
 * ------------------------------------------------------------------ */

int usb_enumerate_device(uint32_t controller_id, uint8_t port) {
    USB_HOST_CONTROLLER* ctrl;
    int i;
    if (controller_id >= (uint32_t)USB_MAX_CONTROLLERS) return -1;
    ctrl = &g_controllers[controller_id];
    if (!ctrl->initialized) return -1;
    if ((uint32_t)port >= ctrl->num_ports) return -1;

    for (i = 0; i < USB_MAX_DEVICES_PER_CTRL; i++) {
        USB_DEVICE* dev;
        if (ctrl->devices[i].device_id != 0) continue;
        dev = &ctrl->devices[i];
        memset(dev, 0, sizeof(*dev));
        dev->device_id = g_next_device_id++;
        dev->address   = (uint8_t)(ctrl->num_devices + 1);  /* next available */
        dev->hub_port  = port;
        dev->speed     = USB_SPEED_FULL;
        dev->connected = 1;
        dev->configured = 0;
        ctrl->num_devices++;
        return (int)dev->device_id;
    }
    return -1;
}

int usb_get_device_descriptor(uint32_t device_id, USB_DEVICE* desc) {
    USB_DEVICE* dev;
    if (desc == NULL) return -1;
    dev = usb_find_device_internal(device_id);
    if (dev == NULL) return -1;
    memcpy(desc, dev, sizeof(USB_DEVICE));
    return 0;
}

int usb_get_device_count(void) {
    int c, d, count = 0;
    for (c = 0; c < USB_MAX_CONTROLLERS; c++) {
        USB_HOST_CONTROLLER* ctrl = &g_controllers[c];
        if (!ctrl->initialized) continue;
        for (d = 0; d < USB_MAX_DEVICES_PER_CTRL; d++) {
            USB_DEVICE* dev = &ctrl->devices[d];
            if (dev->device_id != 0 && dev->connected) {
                count++;
            }
        }
    }
    return count;
}

/* ------------------------------------------------------------------ *
 * Transfers (simulated)
 * ------------------------------------------------------------------ */

int usb_control_transfer(uint32_t device_id, uint8_t request_type,
                         uint8_t request, uint16_t value, uint16_t index,
                         void* buf, uint16_t length) {
    USB_DEVICE* dev;
    (void)request_type;  /* not used by the simulated standard requests */
    (void)index;         /* not used by the simulated standard requests */

    dev = usb_find_device_internal(device_id);
    if (dev == NULL) return -1;

    switch (request) {
    case USB_REQ_GET_DESCRIPTOR: {
        /* High byte of wValue selects the descriptor type. */
        uint8_t desc_type = (uint8_t)((value >> 8) & 0xFFu);
        if (desc_type == USB_DESC_TYPE_DEVICE && buf != NULL) {
            /* Simplified 18-byte USB device descriptor. */
            struct __attribute__((packed)) {
                uint8_t  bLength;
                uint8_t  bDescriptorType;
                uint16_t bcdUSB;
                uint8_t  bDeviceClass;
                uint8_t  bDeviceSubClass;
                uint8_t  bDeviceProtocol;
                uint8_t  bMaxPacketSize0;
                uint16_t idVendor;
                uint16_t idProduct;
                uint16_t bcdDevice;
                uint8_t  iManufacturer;
                uint8_t  iProduct;
                uint8_t  iSerialNumber;
                uint8_t  bNumConfigurations;
            } desc;
            uint16_t to_copy;
            desc.bLength            = (uint8_t)sizeof(desc);
            desc.bDescriptorType    = USB_DESC_TYPE_DEVICE;
            desc.bcdUSB             = 0x0200;   /* USB 2.0 */
            desc.bDeviceClass       = dev->device_class;
            desc.bDeviceSubClass    = dev->device_subclass;
            desc.bDeviceProtocol    = dev->device_protocol;
            desc.bMaxPacketSize0    = 64;
            desc.idVendor           = dev->vendor_id;
            desc.idProduct          = dev->product_id;
            desc.bcdDevice          = 0x0100;
            desc.iManufacturer      = 0;
            desc.iProduct           = 1;
            desc.iSerialNumber      = 0;
            desc.bNumConfigurations = dev->num_configurations ? dev->num_configurations : 1;
            to_copy = (length < (uint16_t)sizeof(desc))
                      ? length : (uint16_t)sizeof(desc);
            memcpy(buf, &desc, to_copy);
        }
        /* Simulation: the full transfer always completes. */
        break;
    }
    case USB_REQ_SET_ADDRESS:
        dev->address = (uint8_t)(value & 0xFFu);
        break;
    case USB_REQ_SET_CONFIGURATION:
        dev->configured = 1;
        break;
    default:
        /* Other standard requests are accepted in simulation mode. */
        break;
    }
    return 0;
}

int usb_bulk_transfer(uint32_t device_id, uint8_t endpoint,
                      void* buf, uint32_t length, int direction) {
    USB_DEVICE* dev;
    (void)endpoint;
    (void)buf;
    (void)length;
    (void)direction;

    dev = usb_find_device_internal(device_id);
    if (dev == NULL) return -1;
    /* Simulation: the whole transfer succeeds (actual_length == length,
     * status == 0). */
    return 0;
}

/* ------------------------------------------------------------------ *
 * Initialisation
 * ------------------------------------------------------------------ */

int usb_driver_init(void) {
    int ctrl_xhci;
    int ctrl_ehci;

    memset(g_controllers, 0, sizeof(g_controllers));
    g_controller_count = 0;
    g_next_device_id   = 1;

    /* Register the two built-in host controllers. */
    ctrl_xhci = usb_register_controller("xHCI Controller 0",
                                        USB_CTRL_TYPE_XHCI, 8u);
    ctrl_ehci = usb_register_controller("EHCI Controller 0",
                                        USB_CTRL_TYPE_EHCI, 6u);

    /* Pre-enumerate virtual devices on the xHCI controller. */
    if (ctrl_xhci >= 0) {
        int dev1 = usb_enumerate_device((uint32_t)ctrl_xhci, 0);
        int dev2 = usb_enumerate_device((uint32_t)ctrl_xhci, 1);
        if (dev1 > 0) {
            usb_set_device_info((uint32_t)dev1,
                                0x046D, 0xC534, 0x09, "USB Hub");
        }
        if (dev2 > 0) {
            usb_set_device_info((uint32_t)dev2,
                                0x046D, 0xC077, 0x03, "USB Mouse");
        }
    }

    /* Pre-enumerate a virtual device on the EHCI controller. */
    if (ctrl_ehci >= 0) {
        int dev3 = usb_enumerate_device((uint32_t)ctrl_ehci, 0);
        if (dev3 > 0) {
            usb_set_device_info((uint32_t)dev3,
                                0x04D9, 0x1203, 0x03, "USB Keyboard");
        }
    }

    return 0;
}
