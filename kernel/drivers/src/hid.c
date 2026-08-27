#include "hid.h"
#include <string.h>

void hid_init(hid_device_t* hid, int dev_type)
{
    if (!hid) return;
    memset(hid, 0, sizeof(hid_device_t));
    hid->dev_type = dev_type;
    spin_init(&hid->lock);
}

int hid_parse_report_descriptor(hid_device_t* hid, const uint8_t* desc, uint32_t size)
{
    if (!hid || !desc || size == 0) return -1;

    hid->report_descriptor = (uint8_t*)desc;
    hid->report_desc_size = size;

    uint32_t pos = 0;
    int report_type = 0;
    uint8_t usage_page = 0;
    uint32_t report_size = 0;
    uint32_t report_count = 0;
    int32_t logical_min = 0;
    int32_t logical_max = 0;
    hid_report_t* current_report = NULL;
    int field_idx = 0;

    while (pos < size) {
        uint8_t prefix = desc[pos++];
        if (prefix == 0) continue;

        uint8_t item_type = (prefix >> 2) & 0x03;
        uint8_t item_tag = (prefix >> 4) & 0x0F;
        uint8_t item_size = prefix & 0x03;
        if (item_size == 3) item_size = 4;

        uint32_t value = 0;
        for (uint8_t i = 0; i < item_size; i++) {
            if (pos < size) value |= (uint32_t)desc[pos++] << (i * 8);
        }
        if (item_size == 1 && (value & 0x80)) value |= 0xFFFFFF00;
        if (item_size == 2 && (value & 0x8000)) value |= 0xFFFF0000;

        if (item_type == 0) {
            switch (item_tag) {
            case 0x00: break;
            case 0x01: usage_page = (uint8_t)value; break;
            case 0x02: logical_min = (int32_t)value; break;
            case 0x03: logical_max = (int32_t)value; break;
            case 0x04: break;
            case 0x05: break;
            case 0x06: break;
            case 0x07: break;
            case 0x08: break;
            case 0x09: break;
            case 0x0A: break;
            case 0x0B: break;
            default: break;
            }
        } else if (item_type == 1) {
            switch (item_tag) {
            case 0x00: break;
            case 0x01: report_size = value; break;
            case 0x02: report_count = value; break;
            case 0x03: break;
            case 0x04: break;
            case 0x05: report_type = HID_INPUT; current_report = &hid->input_report; break;
            case 0x06: report_type = HID_OUTPUT; current_report = &hid->output_report; break;
            case 0x07: report_type = HID_FEATURE; current_report = &hid->feature_report; break;
            case 0x08: break;
            case 0x09: break;
            case 0x0A: break;
            case 0x0B: break;
            default: break;
            }
        } else if (item_type == 2) {
            switch (item_tag) {
            case 0x00: break;
            case 0x01:
                if (current_report && field_idx < 64) {
                    hid_field_t* field = &current_report->fields[field_idx];
                    field->usage_page = usage_page;
                    field->usage = (uint16_t)value;
                    field->report_size = report_size;
                    field->report_count = report_count;
                    field->logical_min = (uint32_t)logical_min;
                    field->logical_max = (uint32_t)logical_max;
                    field_idx++;
                    current_report->field_count = field_idx;
                }
                break;
            case 0x02: break;
            default: break;
            }
        } else if (item_type == 3) {
            switch (item_tag) {
            case 0x00: break;
            case 0x01:
                if (usage_page == HID_USAGE_PAGE_GENERIC && value == HID_USAGE_KEYBOARD) {
                    hid->dev_type = 1;
                    hid->usage_page = usage_page;
                    hid->usage = (uint16_t)value;
                } else if (usage_page == HID_USAGE_PAGE_GENERIC && value == HID_USAGE_MOUSE) {
                    hid->dev_type = 2;
                    hid->usage_page = usage_page;
                    hid->usage = (uint16_t)value;
                }
                field_idx = 0;
                break;
            case 0x02: field_idx = 0; break;
            default: break;
            }
        }
    }

    return 0;
}

int hid_process_report(hid_device_t* hid, const uint8_t* data, uint32_t size)
{
    if (!hid || !data || size == 0) return -1;

    spinlock_acquire(&hid->lock);

    if (hid->dev_type == 1) {
        uint8_t modifiers = data[0];
        hid->leds = 0;
        if (modifiers & 0x01) hid->leds |= 0x04;
        if (modifiers & 0x02) hid->leds |= 0x01;
        if (modifiers & 0x04) hid->leds |= 0x02;

        hid->key_count = 0;
        for (uint32_t i = 2; i < size && i < 8; i++) {
            if (data[i] != 0 && hid->key_count < HID_MAX_KEYS) {
                hid->keys[hid->key_count++] = data[i];
            }
        }
    } else if (hid->dev_type == 2) {
        if (size >= 1) {
            hid->button_count = 0;
            for (int b = 0; b < 8; b++) {
                if (data[0] & (1 << b) && hid->button_count < HID_MAX_BUTTONS) {
                    hid->buttons[hid->button_count++] = (uint8_t)(b + 1);
                }
            }
        }
        if (size >= 3) {
            hid->axes[0] = (int8_t)data[1];
            hid->axes[1] = (int8_t)data[2];
            hid->axis_count = 2;
        }
        if (size >= 4) {
            hid->axes[2] = (int8_t)data[3];
            hid->axis_count = 3;
        }
    }

    if (hid->on_input) {
        hid_report_data_t report;
        report.size = size > HID_MAX_REPORT ? HID_MAX_REPORT : size;
        report.report_id = data[0];
        memcpy(report.data, data, report.size);
        hid->on_input(hid, &report);
    }

    spinlock_release(&hid->lock);
    return 0;
}

void hid_set_leds(hid_device_t* hid, uint8_t leds)
{
    if (!hid) return;
    spinlock_acquire(&hid->lock);
    hid->leds = leds;
    spinlock_release(&hid->lock);
}

int hid_get_key(hid_device_t* hid)
{
    if (!hid || hid->key_count == 0) return -1;
    int key = hid->keys[0];
    for (int i = 1; i < hid->key_count; i++) hid->keys[i - 1] = hid->keys[i];
    hid->key_count--;
    return key;
}

int hid_get_axis(hid_device_t* hid, int axis)
{
    if (!hid || axis < 0 || axis >= hid->axis_count) return 0;
    return hid->axes[axis];
}

int hid_get_button(hid_device_t* hid, int button)
{
    if (!hid || button < 0 || button >= hid->button_count) return 0;
    return hid->buttons[button];
}