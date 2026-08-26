/*
 * HID class driver (keyboard/mouse) - freestanding kernel implementation.
 *
 * A fixed-size pool of HID devices is maintained.  Each connected device
 * owns a private input-report ring buffer, and every report is also pushed
 * into a global queue so consumers can drain input from all devices
 * through a single unified interface.  Two built-in devices (a standard
 * keyboard and a standard mouse) are registered at init time.
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

/* ------------------------------------------------------------------ *
 * Constants
 * ------------------------------------------------------------------ */
#define HID_MAX_DEVICES     16
#define HID_DEVICE_QUEUE    64      /* per-device report ring buffer    */

#define GLOBAL_QUEUE_SIZE   256
#define HID_TYPE_KEYBOARD   1
#define HID_TYPE_MOUSE      2

#define HID_KEYBOARD_ID     0       /* built-in keyboard device id      */
#define HID_MOUSE_ID        1       /* built-in mouse device id         */

/* ------------------------------------------------------------------ *
 * Device pool
 * ------------------------------------------------------------------ */
static HID_DEVICE g_hid_pool[HID_MAX_DEVICES];
static int        g_hid_count;

/* ------------------------------------------------------------------ *
 * Global input-report queue (unified delivery)
 *
 * head = read cursor (oldest report), tail = write cursor (next free
 * slot), count = number of reports currently queued.  When the buffer
 * is full the oldest report is overwritten.
 * ------------------------------------------------------------------ */
static HID_INPUT_REPORT g_global_queue[GLOBAL_QUEUE_SIZE];
static uint32_t g_queue_head, g_queue_tail, g_queue_count;
static uint32_t g_timestamp_counter;

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

/* Ring-buffer push for a device's private report queue.  When the queue
 * is full the oldest report is overwritten. */
static void queue_push(HID_INPUT_REPORT* queue, uint32_t size,
                       uint32_t* head, uint32_t* count,
                       const HID_INPUT_REPORT* report) {
    if (*count >= size) {
        /* overwrite oldest */
        queue[*head] = *report;
        *head = (*head + 1) % size;
    } else {
        uint32_t tail = (*head + *count) % size;
        queue[tail] = *report;
        (*count)++;
    }
}

/* Push a report onto the global queue.  Overwrites the oldest entry
 * when full. */
static void global_queue_push(const HID_INPUT_REPORT* report) {
    g_global_queue[g_queue_tail] = *report;
    g_queue_tail = (g_queue_tail + 1) % GLOBAL_QUEUE_SIZE;
    if (g_queue_count < GLOBAL_QUEUE_SIZE) {
        g_queue_count++;
    } else {
        /* full: drop the oldest by advancing the read cursor */
        g_queue_head = (g_queue_head + 1) % GLOBAL_QUEUE_SIZE;
    }
}

static HID_DEVICE* hid_lookup(uint32_t device_id) {
    if (device_id >= (uint32_t)HID_MAX_DEVICES) return NULL;
    if (!g_hid_pool[device_id].connected) return NULL;
    return &g_hid_pool[device_id];
}

/* ------------------------------------------------------------------ *
 * Public API
 * ------------------------------------------------------------------ */

int hid_driver_init(void) {
    int id;

    memset(g_hid_pool, 0, sizeof(g_hid_pool));
    memset(g_global_queue, 0, sizeof(g_global_queue));
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
    g_timestamp_counter = 0;
    g_hid_count = 0;

    /* Standard Keyboard: vid=0x0001, pid=0x0001, type=1 (keyboard). */
    id = hid_register_device("Standard Keyboard", 0x0001, 0x0001,
                             HID_TYPE_KEYBOARD);
    if (id < 0) return -1;

    /* Standard Mouse: vid=0x0001, pid=0x0002, type=2 (mouse). */
    id = hid_register_device("Standard Mouse", 0x0001, 0x0002,
                             HID_TYPE_MOUSE);
    if (id < 0) return -1;

    return 0;
}

int hid_register_device(const char* name, uint16_t vid, uint16_t pid,
                        uint16_t type) {
    int slot;
    HID_DEVICE* dev;
    size_t nlen;

    if (name == NULL) return -1;

    for (slot = 0; slot < HID_MAX_DEVICES; slot++) {
        if (!g_hid_pool[slot].connected) break;
    }
    if (slot >= HID_MAX_DEVICES) return -1;

    dev = &g_hid_pool[slot];
    memset(dev, 0, sizeof(*dev));

    nlen = strlen(name);
    if (nlen >= sizeof(dev->name)) nlen = sizeof(dev->name) - 1u;
    memcpy(dev->name, name, nlen);
    dev->name[nlen] = 0;

    dev->device_id   = (uint32_t)slot;
    dev->vendor_id   = vid;
    dev->product_id  = pid;
    dev->device_type = type;
    dev->connected   = 1;

    g_hid_count++;
    return slot;
}

int hid_unregister_device(uint32_t device_id) {
    HID_DEVICE* dev;

    if (device_id >= (uint32_t)HID_MAX_DEVICES) return -1;
    dev = &g_hid_pool[device_id];
    if (!dev->connected) return -1;

    dev->connected = 0;
    if (g_hid_count > 0) g_hid_count--;
    return 0;
}

int hid_input_report(uint32_t device_id, const HID_INPUT_REPORT* report) {
    HID_DEVICE* dev;

    if (report == NULL) return -1;
    dev = hid_lookup(device_id);
    if (dev == NULL) return -1;

    /* Per-device ring buffer. */
    queue_push(dev->report_queue, HID_DEVICE_QUEUE,
               &dev->queue_head, &dev->queue_count, report);

    /* Unified delivery queue. */
    global_queue_push(report);
    return 0;
}

int hid_get_report(HID_INPUT_REPORT* report) {
    if (report == NULL) return 0;
    if (g_queue_count == 0) return 0;

    *report = g_global_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1) % GLOBAL_QUEUE_SIZE;
    g_queue_count--;

    if (g_queue_count == 0) {
        /* reset cursors when drained */
        g_queue_head = 0;
        g_queue_tail = 0;
    }
    return 1;
}

int hid_keyboard_event(uint8_t scancode, uint8_t keycode, int pressed) {
    HID_INPUT_REPORT report;

    memset(&report, 0, sizeof(report));
    g_timestamp_counter++;
    report.timestamp   = g_timestamp_counter;
    report.device_type = HID_TYPE_KEYBOARD;
    report.keyboard.scancode = scancode;
    report.keyboard.keycode  = keycode;
    report.keyboard.pressed  = pressed;

    return hid_input_report(HID_KEYBOARD_ID, &report);
}

int hid_mouse_event(int x_delta, int y_delta, uint8_t buttons) {
    HID_INPUT_REPORT report;

    memset(&report, 0, sizeof(report));
    g_timestamp_counter++;
    report.timestamp   = g_timestamp_counter;
    report.device_type = HID_TYPE_MOUSE;
    report.mouse.x_delta = x_delta;
    report.mouse.y_delta = y_delta;
    report.mouse.buttons = buttons;

    return hid_input_report(HID_MOUSE_ID, &report);
}

int hid_get_device_count(void) {
    return g_hid_count;
}

HID_DEVICE* hid_get_device(uint32_t device_id) {
    if (device_id >= (uint32_t)HID_MAX_DEVICES) return NULL;
    if (!g_hid_pool[device_id].connected) return NULL;
    return &g_hid_pool[device_id];
}
