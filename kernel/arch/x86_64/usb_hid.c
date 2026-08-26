

#include <arch/usb_hid.h>
#include <string.h>

static hid_device_t hid_devices[HID_MAX_DEVICES];

static uint32_t hid_total_reports = 0;
static uint32_t hid_error_count = 0;

static int hid_initialized = 0;

typedef struct {
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t  config_value;
    uint8_t  num_interfaces;

    uint8_t  interface_class;
    uint8_t  interface_subclass;
    uint8_t  interface_protocol;
    uint8_t  interface_num;
    uint8_t  interface_alt;

    uint8_t  ep_in_addr;
    uint8_t  ep_out_addr;
    uint16_t ep_in_max_packet;
    uint16_t ep_out_max_packet;

    uint8_t  hid_desc_length;
    const uint8_t* report_desc_data;
    uint16_t report_desc_length;

    void* handle;
} __attribute__((packed)) usb_device_desc_t;

static int usb_control_transfer(void* dev,
                                 uint8_t bmReqType, uint8_t bRequest,
                                 uint16_t wValue, uint16_t wIndex,
                                 uint16_t wLength, uint8_t* data)
{
    if (!dev) return -1;
    (void)bmReqType;
    (void)bRequest;
    (void)wValue;
    (void)wIndex;
    (void)wLength;
    (void)data;
    return 0;
}

static int usb_submit_interrupt(void* dev, uint8_t ep_addr,
                                 uint8_t* buf, uint32_t size,
                                 void (*complete)(void* context, int status,
                                                  uint32_t transferred),
                                 void* context)
{
    if (!dev) return -1;
    if (!buf || size == 0) return -1;
    (void)ep_addr;
    (void)complete;
    (void)context;
    return 0;
}

static int hid_find_free_slot(void)
{
    for (int i = 0; i < HID_MAX_DEVICES; i++) {
        if (hid_devices[i].state == HID_STATE_DISCONNECTED) {
            return i;
        }
    }
    return -1;
}

static hid_device_type_t hid_get_device_type(uint8_t subclass, uint8_t protocol)
{
    if (subclass == USB_HID_SUBCLASS_BOOT) {
        if (protocol == USB_HID_PROTOCOL_KEYBOARD) {
            return HID_DEV_KEYBOARD;
        } else if (protocol == USB_HID_PROTOCOL_MOUSE) {
            return HID_DEV_MOUSE;
        }
    }
    return HID_DEV_GENERIC;
}

static void hid_parse_keyboard_report(uint8_t dev_index, const uint8_t* data, uint32_t size)
{
    if (size < 8) return;

    hid_kbd_event_t event;
    memset(&event, 0, sizeof(event));

    event.modifiers = data[0];

    for (int i = 0; i < 6 && (i + 2) < (int)size; i++) {
        event.keycode[i] = data[2 + i];
    }

    if (hid_devices[dev_index].callback) {
        hid_devices[dev_index].callback(dev_index, (const uint8_t*)&event,
                                          sizeof(event));
    }
}

static void hid_parse_mouse_report(uint8_t dev_index, const uint8_t* data, uint32_t size)
{
    if (size < 4) return;

    hid_mouse_event_t event;
    memset(&event, 0, sizeof(event));

    event.buttons = data[0];

    event.x_delta = (int8_t)data[1];
    event.y_delta = (int8_t)data[2];

    if (size >= 4) {
        event.wheel = (int8_t)data[3];
    }

    if (hid_devices[dev_index].callback) {
        hid_devices[dev_index].callback(dev_index, (const uint8_t*)&event,
                                          sizeof(event));
    }
}

void hid_init(void)
{

    memset(hid_devices, 0, sizeof(hid_devices));

    for (int i = 0; i < HID_MAX_DEVICES; i++) {
        hid_devices[i].state = HID_STATE_DISCONNECTED;
        hid_devices[i].type  = HID_DEV_NONE;
    }

    hid_total_reports = 0;
    hid_error_count   = 0;

    hid_initialized = 1;
}

int hid_probe_device(void* usb_dev)
{
    if (!hid_initialized) return -1;
    if (!usb_dev) return -1;

    usb_device_desc_t* desc = (usb_device_desc_t*)usb_dev;

    if (desc->interface_class != USB_CLASS_HID) {
        return -1;
    }

    int slot = hid_find_free_slot();
    if (slot < 0) {
        hid_error_count++;
        return -1;
    }

    hid_device_t* dev = &hid_devices[slot];

    memset(dev, 0, sizeof(hid_device_t));

    dev->type = hid_get_device_type(desc->interface_subclass, desc->interface_protocol);

    dev->interface_num = desc->interface_num;
    dev->ep_in_addr    = desc->ep_in_addr;
    dev->ep_out_addr   = desc->ep_out_addr;
    dev->vid           = desc->vendor_id;
    dev->pid           = desc->product_id;

    switch (dev->type) {
        case HID_DEV_KEYBOARD:
            dev->report_size = 8;
            break;
        case HID_DEV_MOUSE:
            dev->report_size = 4;
            break;
        default:
            dev->report_size = HID_MAX_REPORT_SIZE;
            break;
    }

    dev->state = HID_STATE_CONNECTED;

    uint8_t hid_desc_buf[32];
    int ret = usb_control_transfer(desc->handle,
                                    0x81, 0x06,
                                    (HID_DESC_TYPE_HID << 8) | 0,
                                    desc->interface_num,
                                    sizeof(hid_desc_buf),
                                    hid_desc_buf);
    if (ret < 0) {
        dev->state = HID_STATE_ERROR;
        hid_error_count++;
        return -1;
    }

    if (desc->report_desc_data && desc->report_desc_length > 0) {
        dev->report_desc = desc->report_desc_data;
        dev->report_desc_len = desc->report_desc_length;
        hid_parse_report_desc(slot);
    }

    if (desc->interface_subclass == USB_HID_SUBCLASS_BOOT) {
        usb_control_transfer(desc->handle,
                              HID_REQTYPE_SET_PROTOCOL,
                              HID_REQUEST_SET_PROTOCOL,
                              0,
                              desc->interface_num,
                              0, NULL);
        dev->protocol = 0;
    }

    dev->state = HID_STATE_CONFIGURED;

    return slot;
}

int hid_register_callback(uint8_t dev_index,
                          void (*callback)(uint8_t, const uint8_t*, uint32_t))
{
    if (dev_index >= HID_MAX_DEVICES) return -1;
    if (hid_devices[dev_index].state == HID_STATE_DISCONNECTED) return -1;

    hid_devices[dev_index].callback = callback;
    return 0;
}

int hid_set_idle(uint8_t dev_index, uint8_t duration, uint8_t report_id)
{
    if (dev_index >= HID_MAX_DEVICES) return -1;
    if (!hid_initialized) return -1;

    hid_device_t* dev = &hid_devices[dev_index];
    if (dev->state == HID_STATE_DISCONNECTED) return -1;

    uint16_t wValue = ((uint16_t)duration << 8) | report_id;
    int ret = usb_control_transfer(NULL,
                                    HID_REQTYPE_SET_IDLE,
                                    HID_REQUEST_SET_IDLE,
                                    wValue,
                                    dev->interface_num,
                                    0, NULL);

    if (ret == 0) {
        dev->idle_rate = duration;
    } else {
        hid_error_count++;
    }

    return ret;
}

int hid_set_protocol(uint8_t dev_index, uint8_t protocol)
{
    if (dev_index >= HID_MAX_DEVICES) return -1;
    if (!hid_initialized) return -1;

    hid_device_t* dev = &hid_devices[dev_index];
    if (dev->state == HID_STATE_DISCONNECTED) return -1;

    int ret = usb_control_transfer(NULL,
                                    HID_REQTYPE_SET_PROTOCOL,
                                    HID_REQUEST_SET_PROTOCOL,
                                    protocol,
                                    dev->interface_num,
                                    0, NULL);

    if (ret == 0) {
        dev->protocol = protocol;
    } else {
        hid_error_count++;
    }

    return ret;
}

int hid_get_report(uint8_t dev_index, uint8_t report_id, uint8_t report_type,
                   uint8_t* buf, uint32_t size)
{
    if (dev_index >= HID_MAX_DEVICES) return -1;
    if (!hid_initialized) return -1;
    if (!buf || size == 0) return -1;

    hid_device_t* dev = &hid_devices[dev_index];
    if (dev->state == HID_STATE_DISCONNECTED) return -1;

    uint16_t wValue = ((uint16_t)report_type << 8) | report_id;

    int ret = usb_control_transfer(NULL,
                                    HID_REQTYPE_GET_INPUT,
                                    HID_REQUEST_GET_REPORT,
                                    wValue,
                                    dev->interface_num,
                                    (uint16_t)size,
                                    buf);

    if (ret < 0) {
        hid_error_count++;
    }

    return ret;
}

int hid_set_report(uint8_t dev_index, uint8_t report_id, uint8_t report_type,
                   const uint8_t* buf, uint32_t size)
{
    if (dev_index >= HID_MAX_DEVICES) return -1;
    if (!hid_initialized) return -1;
    if (!buf || size == 0) return -1;

    hid_device_t* dev = &hid_devices[dev_index];
    if (dev->state == HID_STATE_DISCONNECTED) return -1;

    uint16_t wValue = ((uint16_t)report_type << 8) | report_id;

    int ret = usb_control_transfer(NULL,
                                    HID_REQTYPE_SET_OUTPUT,
                                    HID_REQUEST_SET_REPORT,
                                    wValue,
                                    dev->interface_num,
                                    (uint16_t)size,
                                    (uint8_t*)buf);

    if (ret < 0) {
        hid_error_count++;
    }

    return ret;
}

int hid_parse_report_desc(uint8_t dev_index)
{
    if (dev_index >= HID_MAX_DEVICES) return -1;
    if (!hid_initialized) return -1;

    hid_device_t* dev = &hid_devices[dev_index];
    if (dev->state == HID_STATE_DISCONNECTED) return -1;
    if (!dev->report_desc || dev->report_desc_len == 0) return -1;

    uint16_t report_size = 0;
    uint16_t report_count = 0;
    uint16_t total_bits = 0;

    for (uint16_t i = 0; i < dev->report_desc_len; ) {
        uint8_t item = dev->report_desc[i];
        uint8_t bSize = item & 0x03;
        if (bSize == 3) bSize = 4;
        uint8_t bType = (item >> 2) & 0x03;
        uint8_t bTag = (item >> 4) & 0x0F;

        if (i + 1 + bSize > dev->report_desc_len) break;

        uint32_t data = 0;
        for (uint8_t j = 0; j < bSize; j++) {
            data |= (uint32_t)dev->report_desc[i + 1 + j] << (j * 8);
        }

        if (bType == 1) {
            if (bTag == 7) report_size = (uint16_t)data;
            else if (bTag == 8) report_count = (uint16_t)data;
        } else if (bType == 0 && (bTag == 8 || bTag == 9 || bTag == 10)) {
            total_bits += report_size * report_count;
        }

        i += 1 + bSize;
    }

    if (total_bits > 0) {
        dev->report_size = (uint8_t)((total_bits + 7) / 8);
        if (dev->report_size > HID_MAX_REPORT_SIZE) {
            dev->report_size = HID_MAX_REPORT_SIZE;
        }
    }

    return 0;
}

void hid_irq_handler(uint8_t dev_index, const uint8_t* data, uint32_t size)
{
    if (dev_index >= HID_MAX_DEVICES) return;
    if (!hid_initialized) return;
    if (!data || size == 0) return;

    hid_device_t* dev = &hid_devices[dev_index];

    if (dev->state == HID_STATE_DISCONNECTED) return;

    uint32_t copy_size = size;
    if (copy_size > HID_MAX_REPORT_SIZE) {
        copy_size = HID_MAX_REPORT_SIZE;
    }
    memcpy(dev->data, data, copy_size);

    hid_total_reports++;

    switch (dev->type) {
        case HID_DEV_KEYBOARD:
            hid_parse_keyboard_report(dev_index, data, size);
            break;

        case HID_DEV_MOUSE:
            hid_parse_mouse_report(dev_index, data, size);
            break;

        case HID_DEV_GENERIC:

            if (dev->callback) {
                dev->callback(dev_index, data, size);
            }
            break;

        default:
            hid_error_count++;
            break;
    }
}

int hid_get_info(hid_stats_t* stats)
{
    if (!hid_initialized) return -1;
    if (!stats) return -1;

    memset(stats, 0, sizeof(hid_stats_t));

    uint32_t keyboards = 0, mice = 0, generics = 0;

    for (int i = 0; i < HID_MAX_DEVICES; i++) {
        if (hid_devices[i].state != HID_STATE_DISCONNECTED) {
            switch (hid_devices[i].type) {
                case HID_DEV_KEYBOARD: keyboards++; break;
                case HID_DEV_MOUSE:    mice++;     break;
                case HID_DEV_GENERIC:  generics++; break;
                default: break;
            }
        }
    }

    stats->keyboard_count = keyboards;
    stats->mouse_count    = mice;
    stats->generic_count  = generics;
    stats->total_devices  = keyboards + mice + generics;
    stats->total_reports  = hid_total_reports;
    stats->errors         = hid_error_count;

    return 0;
}
