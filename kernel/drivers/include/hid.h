#ifndef KERNEL_DRIVERS_INPUT_HID_H
#define KERNEL_DRIVERS_INPUT_HID_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define HID_USAGE_PAGE_GENERIC    0x01
#define HID_USAGE_PAGE_KEYBOARD   0x07
#define HID_USAGE_PAGE_MOUSE      0x01
#define HID_USAGE_PAGE_BUTTON     0x09
#define HID_USAGE_PAGE_LED        0x08
#define HID_USAGE_PAGE_CONSUMER   0x0C

#define HID_USAGE_POINTER         0x01
#define HID_USAGE_MOUSE           0x02
#define HID_USAGE_KEYBOARD        0x06
#define HID_USAGE_X               0x30
#define HID_USAGE_Y               0x31
#define HID_USAGE_WHEEL           0x38
#define HID_USAGE_KEY_A           0x04
#define HID_USAGE_KEY_LEFTCTRL    0xE0
#define HID_USAGE_KEY_LEFTSHIFT   0xE1
#define HID_USAGE_KEY_LEFTALT     0xE2
#define HID_USAGE_KEY_CAPSLOCK    0x39

#define HID_INPUT     0x01
#define HID_OUTPUT    0x02
#define HID_FEATURE   0x03
#define HID_COLLECTION 0xA0
#define HID_END_COLLECTION 0xC0

#define HID_MAX_KEYS     256
#define HID_MAX_AXES     16
#define HID_MAX_BUTTONS  32
#define HID_MAX_REPORT   256

typedef struct {
    uint8_t  report_id;
    uint8_t  report_type;
    uint8_t  usage_page;
    uint16_t usage;
    uint32_t logical_min;
    uint32_t logical_max;
    uint32_t physical_min;
    uint32_t physical_max;
    uint32_t unit;
    uint32_t unit_exponent;
    uint32_t report_size;
    uint32_t report_count;
    uint32_t flags;
} hid_field_t;

typedef struct {
    hid_field_t fields[64];
    int         field_count;
    uint32_t    report_size;
    uint8_t     report_id;
} hid_report_t;

typedef struct {
    uint8_t  data[HID_MAX_REPORT];
    uint32_t size;
    uint8_t  report_id;
} hid_report_data_t;

typedef struct hid_device hid_device_t;

struct hid_device {
    int              dev_type;
    int              usage_page;
    uint16_t         usage;
    hid_report_t     input_report;
    hid_report_t     output_report;
    hid_report_t     feature_report;
    uint8_t*         report_descriptor;
    uint32_t         report_desc_size;
    int32_t          axes[HID_MAX_AXES];
    int              axis_count;
    uint8_t          buttons[HID_MAX_BUTTONS];
    int              button_count;
    uint8_t          keys[HID_MAX_KEYS];
    int              key_count;
    uint8_t          leds;
    void*            usb_dev;
    void*            ps2_dev;
    void (*on_input)(hid_device_t* hid, hid_report_data_t* report);
    spinlock_t       lock;
};

void  hid_init(hid_device_t* hid, int dev_type);
int   hid_parse_report_descriptor(hid_device_t* hid, const uint8_t* desc, uint32_t size);
int   hid_process_report(hid_device_t* hid, const uint8_t* data, uint32_t size);
void  hid_set_leds(hid_device_t* hid, uint8_t leds);
int   hid_get_key(hid_device_t* hid);
int   hid_get_axis(hid_device_t* hid, int axis);
int   hid_get_button(hid_device_t* hid, int button);

#endif