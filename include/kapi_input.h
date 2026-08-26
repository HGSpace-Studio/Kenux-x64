#ifndef KAPI_INPUT_H
#define KAPI_INPUT_H

#include <stdint.h>
#include <stddef.h>
#include "kapi_window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Input device types */
#define KAPI_INPUT_TYPE_UNKNOWN     0
#define KAPI_INPUT_TYPE_KEYBOARD    1
#define KAPI_INPUT_TYPE_MOUSE       2
#define KAPI_INPUT_TYPE_TOUCH       3
#define KAPI_INPUT_TYPE_GAMEPAD    4
#define KAPI_INPUT_TYPE_JOYSTICK   5
#define KAPI_INPUT_TYPE_PEN        6
#define KAPI_INPUT_TYPE_TABLET     7
#define KAPI_INPUT_TYPE_REMOTE     8
#define KAPI_INPUT_TYPE_MICROPHONE 9
#define KAPI_INPUT_TYPE_CAMERA    10

/* Input device states */
#define KAPI_INPUT_STATE_UNKNOWN    0
#define KAPI_INPUT_STATE_CONNECTED  1
#define KAPI_INPUT_STATE_DISCONNECTED 2
#define KAPI_INPUT_STATE_ACTIVE    3
#define KAPI_INPUT_STATE_INACTIVE  4
#define KAPI_INPUT_STATE_ERROR     5

/* Input event types */
#define KAPI_INPUT_EVENT_NONE       0
#define KAPI_INPUT_EVENT_CONNECT    1
#define KAPI_INPUT_EVENT_DISCONNECT 2
#define KAPI_INPUT_EVENT_KEY       3
#define KAPI_INPUT_EVENT_MOUSE     4
#define KAPI_INPUT_EVENT_TOUCH     5
#define KAPI_INPUT_EVENT_GAMEPAD   6
#define KAPI_INPUT_EVENT_JOYSTICK  7
#define KAPI_INPUT_EVENT_PEN       8
#define KAPI_INPUT_EVENT_SCROLL    9
#define KAPI_INPUT_EVENT_GESTURE   10

/* Keyboard event types */
#define KAPI_KEY_EVENT_DOWN         0
#define KAPI_KEY_EVENT_UP           1
#define KAPI_KEY_EVENT_REPEAT      2

/* Mouse event types */
#define KAPI_MOUSE_EVENT_DOWN      0
#define KAPI_MOUSE_EVENT_UP        1
#define KAPI_MOUSE_EVENT_MOVE      2
#define KAPI_MOUSE_EVENT_DRAG      3
#define KAPI_MOUSE_EVENT_SCROLL    4
#define KAPI_MOUSE_EVENT_CLICK     5
#define KAPI_MOUSE_EVENT_DBL_CLICK 6
#define KAPI_MOUSE_EVENT_OVER      7
#define KAPI_MOUSE_EVENT_OUT       8

/* Touch event types */
#define KAPI_TOUCH_EVENT_DOWN       0
#define KAPI_TOUCH_EVENT_UP         1
#define KAPI_TOUCH_EVENT_MOVE      2
#define KAPI_TOUCH_EVENT_CANCEL    3
#define KAPI_TOUCH_EVENT_OVER      4
#define KAPI_TOUCH_EVENT_OUT       5

/* Gesture types */
#define KAPI_GESTURE_TYPE_NONE     0
#define KAPI_GESTURE_TYPE_TAP      1
#define KAPI_GESTURE_TYPE_DOUBLE_TAP 2
#define KAPI_GESTURE_TYPE_LONG_PRESS 3
#define KAPI_GESTURE_TYPE_SWIPE    4
#define KAPI_GESTURE_TYPE_PINCH    5
#define KAPI_GESTURE_TYPE_ROTATE   6
#define KAPI_GESTURE_TYPE_PAN      7
#define KAPI_GESTURE_TYPE_ZOOM     8

/* Key codes */
#define KAPI_KEY_UNKNOWN           0x00
#define KAPI_KEY_ESCAPE            0x01
#define KAPI_KEY_1                0x02
#define KAPI_KEY_2                0x03
#define KAPI_KEY_3                0x04
#define KAPI_KEY_4                0x05
#define KAPI_KEY_5                0x06
#define KAPI_KEY_6                0x07
#define KAPI_KEY_7                0x08
#define KAPI_KEY_8                0x09
#define KAPI_KEY_9                0x0A
#define KAPI_KEY_0                0x0B
#define KAPI_KEY_MINUS            0x0C
#define KAPI_KEY_EQUALS           0x0D
#define KAPI_KEY_BACKSPACE        0x0E
#define KAPI_KEY_TAB              0x0F
#define KAPI_KEY_Q                0x10
#define KAPI_KEY_W                0x11
#define KAPI_KEY_E                0x12
#define KAPI_KEY_R                0x13
#define KAPI_KEY_T                0x14
#define KAPI_KEY_Y                0x15
#define KAPI_KEY_U                0x16
#define KAPI_KEY_I                0x17
#define KAPI_KEY_O                0x18
#define KAPI_KEY_P                0x19
#define KAPI_KEY_LBRACKET         0x1A
#define KAPI_KEY_RBRACKET         0x1B
#define KAPI_KEY_RETURN           0x1C
#define KAPI_KEY_LCONTROL         0x1D
#define KAPI_KEY_SEMICOLON        0x1E
#define KAPI_KEY_APOSTROPHE       0x1F
#define KAPI_KEY_GRAVE            0x20
#define KAPI_KEY_LSHIFT           0x21
#define KAPI_KEY_BACKSLASH        0x22
#define KAPI_KEY_Z                0x23
#define KAPI_KEY_X                0x24
#define KAPI_KEY_C                0x25
#define KAPI_KEY_V                0x26
#define KAPI_KEY_B                0x27
#define KAPI_KEY_N                0x28
#define KAPI_KEY_M                0x29
#define KAPI_KEY_COMMA            0x2A
#define KAPI_KEY_PERIOD           0x2B
#define KAPI_KEY_SLASH            0x2C
#define KAPI_KEY_RSHIFT           0x2D
#define KAPI_KEY_MULTIPLY         0x2E
#define KAPI_KEY_ALT              0x2F
#define KAPI_KEY_SPACE            0x30
#define KAPI_KEY_CAPSLOCK         0x31
#define KAPI_KEY_F1               0x3A
#define KAPI_KEY_F2               0x3B
#define KAPI_KEY_F3               0x3C
#define KAPI_KEY_F4               0x3D
#define KAPI_KEY_F5               0x3E
#define KAPI_KEY_F6               0x3F
#define KAPI_KEY_F7               0x40
#define KAPI_KEY_F8               0x41
#define KAPI_KEY_F9               0x42
#define KAPI_KEY_F10              0x43
#define KAPI_KEY_F11              0x44
#define KAPI_KEY_F12              0x45
#define KAPI_KEY_NUMLOCK          0x46
#define KAPI_KEY_SCROLL           0x47
#define KAPI_KEY_LMENU            0x48
#define KAPI_KEY_RMENU            0x49
#define KAPI_KEY_PRINT            0x4A
#define KAPI_KEY_PAUSE            0x4B
#define KAPI_KEY_INSERT           0x4C
#define KAPI_KEY_HOME             0x4D
#define KAPI_KEY_PAGEUP           0x4E
#define KAPI_KEY_DELETE           0x4F
#define KAPI_KEY_END              0x50
#define KAPI_KEY_PAGEDOWN         0x51
#define KAPI_KEY_RIGHT            0x52
#define KAPI_KEY_LEFT             0x53
#define KAPI_KEY_DOWN             0x54
#define KAPI_KEY_UP               0x55
#define KAPI_KEY_NUMPAD0          0x60
#define KAPI_KEY_NUMPAD1          0x61
#define KAPI_KEY_NUMPAD2          0x62
#define KAPI_KEY_NUMPAD3          0x63
#define KAPI_KEY_NUMPAD4          0x64
#define KAPI_KEY_NUMPAD5          0x65
#define KAPI_KEY_NUMPAD6          0x66
#define KAPI_KEY_NUMPAD7          0x67
#define KAPI_KEY_NUMPAD8          0x68
#define KAPI_KEY_NUMPAD9          0x69
#define KAPI_KEY_MULTIPLY_PAD     0x6A
#define KAPI_KEY_ADD_PAD          0x6B
#define KAPI_KEY_SUBTRACT_PAD     0x6C
#define KAPI_KEY_DECIMAL_PAD      0x6D
#define KAPI_KEY_DIVIDE_PAD      0x6E
#define KAPI_KEY_F13              0x7A
#define KAPI_KEY_F14              0x7B
#define KAPI_KEY_F15              0x7C
#define KAPI_KEY_F16              0x7D
#define KAPI_KEY_F17              0x7E
#define KAPI_KEY_F18              0x7F
#define KAPI_KEY_F19              0x80
#define KAPI_KEY_F20              0x81
#define KAPI_KEY_F21              0x82
#define KAPI_KEY_F22              0x83
#define KAPI_KEY_F23              0x84
#define KAPI_KEY_F24              0x85
#define KAPI_KEY_MEDIA_PLAY       0x86
#define KAPI_KEY_MEDIA_PAUSE      0x87
#define KAPI_KEY_MEDIA_STOP       0x88
#define KAPI_KEY_MEDIA_NEXT       0x89
#define KAPI_KEY_MEDIA_PREVIOUS   0x8A
#define KAPI_KEY_MEDIA_VOLUME_UP  0x8B
#define KAPI_KEY_MEDIA_VOLUME_DOWN 0x8C
#define KAPI_KEY_MEDIA_MUTE       0x8D
#define KAPI_KEY_MEDIA_RECORD     0x8E

/* Mouse buttons */
#define KAPI_MOUSE_BUTTON_NONE     0x00
#define KAPI_MOUSE_BUTTON_LEFT    0x01
#define KAPI_MOUSE_BUTTON_RIGHT   0x02
#define KAPI_MOUSE_BUTTON_MIDDLE  0x03
#define KAPI_MOUSE_BUTTON_X1      0x04
#define KAPI_MOUSE_BUTTON_X2      0x05
#define KAPI_MOUSE_BUTTON_WHEEL_UP 0x06
#define KAPI_MOUSE_BUTTON_WHEEL_DOWN 0x07

/* Input device structure */
typedef struct kapi_input_device kapi_input_device_t;

/* Input event structure */
typedef struct {
    uint32_t type;
    uint32_t timestamp;
    kapi_input_device_t* device;
    kapi_window_t* window;
    union {
        struct {
            uint32_t key;
            uint32_t scan_code;
            uint32_t modifiers;
            uint32_t event_type;
        } key;
        struct {
            uint32_t button;
            int32_t x;
            int32_t y;
            int32_t dx;
            int32_t dy;
            uint32_t event_type;
        } mouse;
        struct {
            uint32_t touch_id;
            int32_t x;
            int32_t y;
            int32_t pressure;
            uint32_t event_type;
        } touch;
        struct {
            uint32_t button;
            int32_t x;
            int32_t y;
            int32_t z;
            uint32_t event_type;
        } gamepad;
        struct {
            uint32_t gesture_type;
            uint32_t touch_count;
            kapi_point_t* touches;
            float delta_x;
            float delta_y;
            float scale;
            float rotation;
        } gesture;
    } data;
} kapi_input_event_t;

/* Input device properties */
typedef struct {
    char name[256];
    char unique_id[256];
    uint32_t type;
    uint32_t state;
    uint32_t vendor_id;
    uint32_t product_id;
    uint32_t version;
    uint32_t capabilities;
    kapi_rect_t bounds;
    uint32_t max_touch_points;
    uint32_t max_buttons;
    uint32_t max_axes;
    uint8_t report_rate;
    uint8_t power_mode;
} kapi_input_device_properties_t;

/* Input device operations */
typedef struct {
    int (*connect)(kapi_input_device_t* device);
    int (*disconnect)(kapi_input_device_t* device);
    int (*reset)(kapi_input_device_t* device);
    int (*set_mode)(kapi_input_device_t* device, uint32_t mode);
    int (*get_calibration)(kapi_input_device_t* device, void* calibration_data);
    int (*set_calibration)(kapi_input_device_t* device, const void* calibration_data);
    int (*get_raw_data)(kapi_input_device_t* device, void* data, size_t size);
    int (*set_led)(kapi_input_device_t* device, uint32_t led, uint8_t brightness);
} kapi_input_ops_t;

/* Input device structure */
struct kapi_input_device {
    kapi_input_device_t* next;
    kapi_input_device_properties_t properties;
    kapi_input_ops_t* ops;
    void* private_data;
    uint32_t ref_count;
    uint64_t last_event_time;
    uint64_t connect_time;
    uint64_t disconnect_time;
    uint32_t event_count;
    int enabled;
};

/* Input manager structure */
typedef struct kapi_input_manager kapi_input_manager_t;

/* Input filter structure */
typedef struct {
    uint32_t device_type_mask;
    uint32_t event_type_mask;
    uint32_t key_mask;
    uint32_t button_mask;
    int enabled;
    kapi_window_t* target_window;
} kapi_input_filter_t;

/* Input focus structure */
typedef struct {
    kapi_window_t* focused_window;
    kapi_window_t* active_window;
    kapi_window_t* capture_window;
    kapi_input_device_t* active_device;
    uint32_t last_focus_time;
} kapi_input_focus_t;

/* Input manager API */
int kapi_input_manager_init(void);
void kapi_input_manager_cleanup(void);

int kapi_input_manager_update(void);

/* Input device management */
int kapi_input_device_register(kapi_input_device_t* device);
int kapi_input_device_unregister(kapi_input_device_t* device);
kapi_input_device_t* kapi_input_device_find(const char* unique_id);
kapi_input_device_t* kapi_input_device_find_by_type(uint32_t device_type, uint32_t index);
kapi_input_device_t** kapi_input_device_list_all(uint32_t* count);

/* Input event processing */
int kapi_input_process_event(const kapi_input_event_t* event);
int kapi_input_dispatch_event(kapi_window_t* window, const kapi_input_event_t* event);
int kapi_input_enqueue_event(const kapi_input_event_t* event);
int kapi_input_dequeue_event(kapi_input_event_t* event);

/* Input focus management */
int kapi_input_set_focused_window(kapi_window_t* window);
int kapi_input_get_focused_window(kapi_window_t** window);
int kapi_input_set_capture_window(kapi_window_t* window);
int kapi_input_get_capture_window(kapi_window_t** window);
int kapi_input_release_capture(void);

/* Input filters */
int kapi_input_filter_create(kapi_input_filter_t** filter);
int kapi_input_filter_destroy(kapi_input_filter_t* filter);
int kapi_input_filter_set_device_type_mask(kapi_input_filter_t* filter, uint32_t mask);
int kapi_input_filter_set_event_type_mask(kapi_input_filter_t* filter, uint32_t mask);
int kapi_input_filter_set_target_window(kapi_input_filter_t* filter, kapi_window_t* window);
int kapi_input_filter_enable(kapi_input_filter_t* filter);
int kapi_input_filter_disable(kapi_input_filter_t* filter);
int kapi_input_filter_add(kapi_input_filter_t* filter);
int kapi_input_filter_remove(kapi_input_filter_t* filter);

/* Input mapping */
int kapi_input_map_key(uint32_t physical_key, uint32_t virtual_key);
int kapi_input_unmap_key(uint32_t physical_key);
uint32_t kapi_input_map_key_to_virtual(uint32_t physical_key);
uint32_t kapi_input_map_key_from_virtual(uint32_t virtual_key);

/* Input calibration */
int kapi_input_calibrate_device(kapi_input_device_t* device, const void* calibration_data);
int kapi_input_get_device_calibration(kapi_input_device_t* device, void* calibration_data);

/* Input configuration */
int kapi_input_set_device_mode(kapi_input_device_t* device, uint32_t mode);
int kapi_input_get_device_mode(kapi_input_device_t* device, uint32_t* mode);
int kapi_input_set_device_sensitivity(kapi_input_device_t* device, float sensitivity);
int kapi_input_get_device_sensitivity(kapi_input_device_t* device, float* sensitivity);

/* Input hotkeys */
typedef void (*kapi_input_hotkey_callback_t)(uint32_t hotkey_id, uint32_t modifiers);

int kapi_input_register_hotkey(uint32_t hotkey_id, uint32_t modifiers, kapi_input_hotkey_callback_t callback);
int kapi_input_unregister_hotkey(uint32_t hotkey_id);
int kapi_input_trigger_hotkey(uint32_t hotkey_id, uint32_t modifiers);

/* Input debugging */
int kapi_input_dump_devices(void);
int kapi_input_dump_events(void);
int kapi_input_dump_focus(void);

/* Input statistics */
typedef struct {
    uint64_t total_events;
    uint64_t key_events;
    uint64_t mouse_events;
    uint64_t touch_events;
    uint64_t gesture_events;
    uint64_t device_connects;
    uint64_t device_disconnects;
    uint32_t active_devices;
    uint32_t active_windows;
    uint32_t event_queue_size;
} kapi_input_stats_t;

int kapi_input_get_stats(kapi_input_stats_t* stats);

/* Input context management */
int kapi_input_context_create(void);
int kapi_input_context_destroy(void);
int kapi_input_context_set_global_state(void);
int kapi_input_context_get_global_state(void);

/* Input event generation */
int kapi_input_simulate_key(uint32_t key, uint32_t event_type);
int kapi_input_simulate_mouse(int32_t x, int32_t y, uint32_t button, uint32_t event_type);
int kapi_input_simulate_touch(uint32_t touch_id, int32_t x, int32_t y, uint32_t pressure, uint32_t event_type);
int kapi_input_simulate_gesture(uint32_t gesture_type, uint32_t touch_count, kapi_point_t* touches, 
                                float delta_x, float delta_y, float scale, float rotation);

/* Input device configuration */
int kapi_input_device_set_repeat_rate(kapi_input_device_t* device, uint32_t rate);
int kapi_input_device_get_repeat_rate(kapi_input_device_t* device, uint32_t* rate);
int kapi_input_device_set_repeat_delay(kapi_input_device_t* device, uint32_t delay);
int kapi_input_device_get_repeat_delay(kapi_input_device_t* device, uint32_t* delay);

/* Input system configuration */
int kapi_input_set_global_repeat_rate(uint32_t rate);
int kapi_input_get_global_repeat_rate(uint32_t* rate);
int kapi_input_set_global_repeat_delay(uint32_t delay);
int kapi_input_get_global_repeat_delay(uint32_t* delay);

#ifdef __cplusplus
}
#endif

#endif