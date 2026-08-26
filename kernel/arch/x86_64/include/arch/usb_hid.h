#ifndef ARCH_X86_64_USB_HID_H
#define ARCH_X86_64_USB_HID_H

#include <arch/types.h>

#define USB_CLASS_HID              0x03

#define USB_HID_SUBCLASS_NONE      0x00
#define USB_HID_SUBCLASS_BOOT      0x01

#define USB_HID_PROTOCOL_NONE     0x00
#define USB_HID_PROTOCOL_KEYBOARD 0x01
#define USB_HID_PROTOCOL_MOUSE    0x02

#define HID_REPORT_TYPE_INPUT     1
#define HID_REPORT_TYPE_OUTPUT    2
#define HID_REPORT_TYPE_FEATURE   3

#define HID_REQUEST_GET_REPORT     0x01
#define HID_REQUEST_GET_IDLE       0x02
#define HID_REQUEST_GET_PROTOCOL   0x03
#define HID_REQUEST_SET_REPORT     0x09
#define HID_REQUEST_SET_IDLE       0x0A
#define HID_REQUEST_SET_PROTOCOL   0x0B

#define HID_REQTYPE_SET_OUTPUT    0x21

#define HID_REQTYPE_SET_IDLE      0x21
#define HID_REQTYPE_SET_PROTOCOL   0x21

#define HID_REQTYPE_SET_FEATURE   0x21

#define HID_REQTYPE_GET_INPUT     0xA1
#define HID_REQTYPE_GET_OUTPUT    0xA1
#define HID_REQTYPE_GET_FEATURE   0xA1
#define HID_REQTYPE_GET_IDLE      0xA1
#define HID_REQTYPE_GET_PROTOCOL   0xA1

#define HID_DESC_TYPE_HID          0x21
#define HID_DESC_TYPE_REPORT       0x22
#define HID_DESC_TYPE_PHYSICAL     0x23

#define HID_MAX_DEVICES           16
#define HID_MAX_REPORT_SIZE       64

typedef enum {
    HID_DEV_NONE     = 0,
    HID_DEV_KEYBOARD = 1,
    HID_DEV_MOUSE    = 2,
    HID_DEV_GENERIC  = 3
} hid_device_type_t;

typedef enum {
    HID_STATE_DISCONNECTED = 0,
    HID_STATE_CONNECTED    = 1,
    HID_STATE_CONFIGURED   = 2,
    HID_STATE_ERROR        = 3
} hid_device_state_t;

typedef struct {
    hid_device_type_t  type;
    uint8_t            interface_num;
    uint8_t            ep_in_addr;
    uint8_t            ep_out_addr;
    uint8_t            protocol;
    uint8_t            report_size;
    uint8_t            idle_rate;
    hid_device_state_t state;
    uint8_t            data[HID_MAX_REPORT_SIZE];
    uint16_t           vid;
    uint16_t           pid;
    uint32_t           flags;

    void (*callback)(uint8_t dev_index, const uint8_t* data, uint32_t size);

    const uint8_t* report_desc;
    uint16_t       report_desc_len;
} hid_device_t;

typedef struct {
    const uint8_t* data;
    uint16_t       length;
} hid_report_desc_t;

typedef struct {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycode[6];
} __attribute__((packed)) hid_kbd_event_t;

#define HID_MOD_LCTRL   (1 << 0)
#define HID_MOD_LSHIFT  (1 << 1)
#define HID_MOD_LALT    (1 << 2)
#define HID_MOD_LGUI    (1 << 3)
#define HID_MOD_RCTRL   (1 << 4)
#define HID_MOD_RSHIFT  (1 << 5)
#define HID_MOD_RALT    (1 << 6)
#define HID_MOD_RGUI    (1 << 7)

typedef struct {
    uint8_t  buttons;
    int8_t   x_delta;
    int8_t   y_delta;
    int8_t   wheel;
} __attribute__((packed)) hid_mouse_event_t;

#define HID_MOUSE_BTN_LEFT   (1 << 0)
#define HID_MOUSE_BTN_RIGHT  (1 << 1)
#define HID_MOUSE_BTN_MIDDLE (1 << 2)
#define HID_MOUSE_BTN_SIDE   (1 << 3)
#define HID_MOUSE_BTN_EXTRA  (1 << 4)

typedef struct {
    uint32_t total_devices;
    uint32_t keyboard_count;
    uint32_t mouse_count;
    uint32_t generic_count;
    uint32_t total_reports;
    uint32_t errors;
} hid_stats_t;

void hid_init(void);

int hid_probe_device(void* usb_dev);

int hid_register_callback(uint8_t dev_index,
                          void (*callback)(uint8_t, const uint8_t*, uint32_t));

int hid_set_idle(uint8_t dev_index, uint8_t duration, uint8_t report_id);

int hid_set_protocol(uint8_t dev_index, uint8_t protocol);

int hid_get_report(uint8_t dev_index, uint8_t report_id, uint8_t report_type,
                   uint8_t* buf, uint32_t size);

int hid_set_report(uint8_t dev_index, uint8_t report_id, uint8_t report_type,
                   const uint8_t* buf, uint32_t size);

int hid_parse_report_desc(uint8_t dev_index);

void hid_irq_handler(uint8_t dev_index, const uint8_t* data, uint32_t size);

int hid_get_info(hid_stats_t* stats);

#endif
