

#ifndef ARCH_KEYBOARD_H
#define ARCH_KEYBOARD_H

#include <arch/types.h>

#define KBD_DATA_PORT    0x60
#define KBD_CMD_PORT     0x64

#define KBD_CMD_SET_LED       0xED
#define KBD_CMD_GET_LED       0xED
#define KBD_CMD_ECHO         0xEE
#define KBD_CMD_SET_SCANCODE  0xF0
#define KBD_CMD_IDENTIFY      0xF2
#define KBD_CMD_SET_RATE      0xF3
#define KBD_CMD_ENABLE        0xF4
#define KBD_CMD_DISABLE       0xF5
#define KBD_CMD_RESET         0xFF

#define KBD_RESP_ACK           0xFA
#define KBD_RESP_RESEND       0xFE
#define KBD_RESP_BAT_OK       0xAA
#define KBD_RESP_ECHO         0xEE
#define KBD_RESP_IDENTIFY     0xAB

#define KBD_LED_SCROLL   0x01
#define KBD_LED_NUM      0x02
#define KBD_LED_CAPS     0x04

#define KEYMOD_NONE      0x00
#define KEYMOD_LSHIFT    0x01
#define KEYMOD_RSHIFT    0x02
#define KEYMOD_LCTRL     0x04
#define KEYMOD_RCTRL     0x08
#define KEYMOD_LALT      0x10
#define KEYMOD_RALT      0x20
#define KEYMOD_CAPSLOCK  0x40
#define KEYMOD_NUMLOCK   0x80
#define KEYMOD_SCROLLLOCK 0x100
#define KEYMOD_LWIN       0x200
#define KEYMOD_RWIN       0x400

#define KEY_NONE        0x00
#define KEY_ESC         0x1B
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0D
#define KEY_LCTRL       0x00
#define KEY_LSHIFT      0x01
#define KEY_RSHIFT      0x02
#define KEY_LALT        0x03
#define KEY_RALT        0x04
#define KEY_RCTRL       0x05
#define KEY_SPACE       0x20
#define KP_ENTER        KEY_KP_ENTER
#define KP_SLASH        KEY_KP_SLASH
#define KEY_F1          0x80
#define KEY_F2          0x81
#define KEY_F3          0x82
#define KEY_F4          0x83
#define KEY_F5          0x84
#define KEY_F6          0x85
#define KEY_F7          0x86
#define KEY_F8          0x87
#define KEY_F9          0x88
#define KEY_F10         0x89
#define KEY_F11         0x8A
#define KEY_F12         0x8B
#define KEY_INSERT      0x8C
#define KEY_DELETE      0x8D
#define KEY_HOME        0x8E
#define KEY_END         0x8F
#define KEY_PAGEUP      0x90
#define KEY_PAGEDOWN    0x91
#define KEY_UP          0x92
#define KEY_DOWN        0x93
#define KEY_LEFT        0x94
#define KEY_RIGHT       0x95
#define KEY_CAPSLOCK    0x96
#define KEY_NUMLOCK     0x97
#define KEY_SCROLLLOCK  0x98
#define KEY_PRINTSCREEN 0x99
#define KEY_PAUSE       0x9A
#define KEY_KP_0        0xA0
#define KEY_KP_1        0xA1
#define KEY_KP_2        0xA2
#define KEY_KP_3        0xA3
#define KEY_KP_4        0xA4
#define KEY_KP_5        0xA5
#define KEY_KP_6        0xA6
#define KEY_KP_7        0xA7
#define KEY_KP_8        0xA8
#define KEY_KP_9        0xA9
#define KEY_KP_ENTER    0xAA
#define KEY_KP_PLUS     0xAB
#define KEY_KP_MINUS    0xAC
#define KEY_KP_STAR     0xAD
#define KEY_KP_SLASH    0xAE
#define KEY_KP_PERIOD   0xAF
#define KEY_KP_COMMA    0xB0
#define KEY_LWIN        0xB1
#define KEY_RWIN        0xB2
#define KEY_MENU        0xB3

typedef struct key_event {
    uint8_t  keycode;
    uint32_t modifiers;
    int      pressed;
    char     ascii;
} key_event_t;

typedef void (*kbd_callback_t)(const key_event_t* event);

#define KBD_BUFFER_SIZE  1024

void keyboard_init(void);

void keyboard_irq_handler(void);

key_event_t keyboard_read(void);

int keyboard_poll(key_event_t* event);

void keyboard_set_callback(kbd_callback_t cb);

uint32_t keyboard_get_modifiers(void);

void keyboard_set_led(uint8_t led_mask);

#endif
