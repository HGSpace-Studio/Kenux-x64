#ifndef KERNEL_DRIVERS_USB_XHCI_H
#define KERNEL_DRIVERS_USB_USB_H

#include <arch/types.h>
#include <arch/spinlock.h>
#include "pci.h"

#define USB_DESC_DEVICE       1
#define USB_DESC_CONFIG       2
#define USB_DESC_STRING       3
#define USB_DESC_INTERFACE    4
#define USB_DESC_ENDPOINT     5
#define USB_DESC_SS_ENDPOINT  6
#define USB_DESC_HUB          0x29
#define USB_DESC_HID          0x21

#define USB_EP_CONTROL       0
#define USB_EP_ISOCHRONOUS   1
#define USB_EP_BULK          2
#define USB_EP_INTERRUPT     3

#define USB_DIR_OUT          0
#define USB_DIR_IN           0x80

#define USB_REQ_GET_STATUS   0
#define USB_REQ_CLEAR_FEATURE 1
#define USB_REQ_SET_FEATURE  3
#define USB_REQ_SET_ADDRESS  5
#define USB_REQ_GET_DESCRIPTOR 6
#define USB_REQ_SET_DESCRIPTOR 7
#define USB_REQ_GET_CONFIG   8
#define USB_REQ_SET_CONFIG   9

#define USB_SPEED_FULL       0
#define USB_SPEED_LOW        1
#define USB_SPEED_HIGH       2
#define USB_SPEED_SUPER      3

#define USB_MAX_DEVICES      128
#define USB_MAX_ENDPOINTS    32
#define USB_MAX_INTERFACES   32

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
} usb_desc_header_t;

typedef struct {
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
} __attribute__((packed)) usb_device_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) usb_interface_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;
    uint8_t  bHubContrCurrent;
    uint8_t  var_data[1];
} __attribute__((packed)) usb_hub_desc_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wDescriptorLength;
} __attribute__((packed)) usb_hid_desc_t;

typedef struct usb_device usb_device_t;

struct usb_device {
    int              address;
    int              speed;
    int              port;
    int              hub_addr;
    usb_device_desc_t  desc;
    usb_config_desc_t  config;
    usb_device_t*    children[16];
    int              child_count;
    int              configured;
    void*            driver_ctx;
    void*            hc_priv;
    spinlock_t       lock;
};

typedef struct {
    int (*control)(usb_device_t* dev, uint8_t request_type, uint8_t request,
                   uint16_t value, uint16_t index, void* data, uint16_t length);
    int (*bulk)(usb_device_t* dev, int endpoint, void* data, uint32_t length, int direction);
    int (*interrupt)(usb_device_t* dev, int endpoint, void* data, uint32_t length);
    int (*isochronous)(usb_device_t* dev, int endpoint, void* data, uint32_t length);
} usb_hc_ops_t;

typedef struct {
    int  dev_num;
    int  speed;
    int  max_packet;
    int  interval;
    int  type;
    int  direction;
    void* hc_priv;
} usb_endpoint_t;

int  usb_control_msg(usb_device_t* dev, uint8_t request_type, uint8_t request,
                     uint16_t value, uint16_t index, void* data, uint16_t length);
int  usb_get_descriptor(usb_device_t* dev, uint8_t type, uint8_t index, uint16_t lang, void* buf, uint16_t size);
int  usb_set_address(usb_device_t* dev, uint8_t addr);
int  usb_set_configuration(usb_device_t* dev, uint8_t config);
int  usb_clear_feature(usb_device_t* dev, uint8_t feature, uint16_t index);
int  usb_set_feature(usb_device_t* dev, uint8_t feature, uint16_t index);
int  usb_bulk_transfer(usb_device_t* dev, int endpoint, void* data, uint32_t length, int direction);
int  usb_interrupt_transfer(usb_device_t* dev, int endpoint, void* data, uint32_t length);

#endif

#ifndef KERNEL_DRIVERS_USB_XHCI_H_EXTRA
#define KERNEL_DRIVERS_USB_XHCI_H_EXTRA

#define XHCI_PCI_CLASS    0x0C
#define XHCI_PCI_SUBCLASS 0x03
#define XHCI_PCI_PROGIF   0x30

#define XHCI_CAPLENGTH    0x00
#define XHCI_HCIVERSION  0x02
#define XHCI_HCSPARAMS1   0x04
#define XHCI_HCSPARAMS2   0x08
#define XHCI_HCSPARAMS3   0x0C
#define XHCI_HCCPARAMS1   0x10
#define XHCI_DBOFF        0x14
#define XHCI_HCCPARAMS2   0x18

#define XHCI_USBCMD       0x00
#define XHCI_USBSTS       0x04
#define XHCI_PAGESIZE     0x08
#define XHCI_DNCTRL       0x14
#define XHCI_CRCR_LOW     0x18
#define XHCI_CRCR_HIGH    0x1C
#define XHCI_DCBAAP_LOW   0x30
#define XHCI_DCBAAP_HIGH  0x34
#define XHCI_CONFIG        0x38

#define XHCI_CMD_RUN      (1 << 0)
#define XHCI_CMD_HCRST    (1 << 1)
#define XHCI_CMD_INTE     (1 << 2)
#define XHCI_CMD_CME      (1 << 6)

#define XHCI_STS_HCH      (1 << 0)
#define XHCI_STS_HSE      (1 << 2)
#define XHCI_STS_EINT     (1 << 3)
#define XHCI_STS_PCD      (1 << 4)
#define XHCI_STS_SSS      (1 << 8)
#define XHCI_STS_RSS      (1 << 9)
#define XHCI_STS_SRE      (1 << 10)
#define XHCI_STS_CNR      (1 << 11)

#define XHCI_MAX_PORTS    256
#define XHCI_MAX_SLOTS    256
#define XHCI_MAX_RINGS    64

typedef struct {
    uint32_t dword[4];
} xhci_trb_t;

typedef struct {
    xhci_trb_t*  trbs;
    uint32_t     cycle;
    uint32_t     enqueue_idx;
    uint32_t     dequeue_idx;
    uint32_t     ring_size;
    uint64_t     phys;
    spinlock_t   lock;
} xhci_ring_t;

typedef struct {
    uint64_t dev_ctx_addr;
    uint32_t reserved[2];
} xhci_slot_ctx_entry_t;

typedef struct {
    pci_device_t*       pci_dev;
    volatile uint8_t*   mmio;
    uint32_t            cap_length;
    uint16_t            hci_version;
    uint32_t            hcs_params1;
    uint32_t            hcs_params2;
    uint32_t            hcs_params3;
    uint32_t            hcc_params1;
    uint32_t            dboff;
    uint32_t            hcc_params2;
    int                 max_slots;
    int                 max_ports;
    int                 max_intrs;
    int                 max_scratchpad;
    int                 context_size;
    volatile uint32_t*  op_regs;
    volatile uint32_t*  doorbells;
    xhci_ring_t         cmd_ring;
    xhci_ring_t         event_rings[XHCI_MAX_RINGS];
    xhci_slot_ctx_entry_t* dcbaa;
    usb_device_t*       devices[XHCI_MAX_SLOTS];
    int                 port_speeds[XHCI_MAX_PORTS];
    spinlock_t          lock;
} xhci_hc_t;

int  xhci_init(xhci_hc_t* hc, pci_device_t* pci_dev);
void xhci_shutdown(xhci_hc_t* hc);
int  xhci_reset(xhci_hc_t* hc);
int  xhci_start(xhci_hc_t* hc);
int  xhci_stop(xhci_hc_t* hc);
void xhci_irq_handler(int irq, void* ctx);

#endif