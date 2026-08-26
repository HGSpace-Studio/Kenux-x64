

#include <arch/keyboard.h>
#include <arch/io.h>
#include <arch/spinlock.h>
#include <arch/vga.h>
#include <arch/interrupt.h>
#include <arch/pic.h>
#include <string.h>

static key_event_t kbd_buffer[KBD_BUFFER_SIZE];
static uint32_t kbd_head = 0;
static uint32_t kbd_tail = 0;
static spinlock_t kbd_lock = SPINLOCK_INIT;

static uint32_t kbd_modifiers = 0;
static uint8_t  kbd_led = 0;
static int kbd_extended = 0;
static kbd_callback_t kbd_callback = NULL;

static inline uint8_t kbd_read_data(void) { return inb(KBD_DATA_PORT); }
static inline void kbd_write_data(uint8_t val) { outb(KBD_DATA_PORT, val); }
static inline void kbd_write_cmd(uint8_t cmd) { outb(KBD_CMD_PORT, cmd); }

static inline void kbd_wait_input(void)
{
    while (inb(KBD_CMD_PORT) & 0x02);
}

static inline void kbd_wait_output(void)
{
    while (!(inb(KBD_CMD_PORT) & 0x01));
}

static const uint8_t scancode_set1_normal[128] = {
      KEY_NONE, KEY_ESC, '1',    '2',    '3',    '4',    '5',    '6',
      '7',    '8',    '9',    '0',    '-',    '=',    KEY_BACKSPACE, KEY_TAB,
      'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
      'o',    'p',    '[',    ']',    KEY_ENTER, KEY_LCTRL, 'a',    's',
      'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
      '\'',   '`',    KEY_LSHIFT, '\\',  'z',    'x',    'c',    'v',
      'b',    'n',    'm',    ',',    '.',    '/',    KEY_RSHIFT, '*',
      KEY_LALT, ' ',   KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
      KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLLLOCK, '7',
      '8',    '9',    '-',    '4',    '5',    '6',    '+',    '1',
      '2',    '3',    '0',    '.',   KEY_NONE, KEY_NONE, KEY_NONE, KEY_F11,
      KEY_F12, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
};

static const uint8_t scancode_set1_shift[128] = {
      KEY_NONE, KEY_ESC, '!',    '@',    '#',    '$',    '%',    '^',
      '&',    '*',    '(',    ')',    '_',    '+',    KEY_BACKSPACE, KEY_TAB,
      'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I',
      'O',    'P',    '{',    '}',    KEY_ENTER, KEY_LCTRL, 'A',    'S',
      'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':',
      '"',    '~',    KEY_LSHIFT, '|',   'Z',    'X',    'C',    'V',
      'B',    'N',    'M',    '<',    '>',    '?',    KEY_RSHIFT, '*',
      KEY_LALT, ' ',   KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
      KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLLLOCK, '7',
      '8',    '9',    '-',    '4',    '5',    '6',    '+',    '1',
      '2',    '3',    '0',    '.',   KEY_NONE, KEY_NONE, KEY_NONE, KEY_F11,
      KEY_F12, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
};

static const uint8_t scancode_e0_press[128] = {
      [0x00] = KEY_NONE,
      [0x09] = KP_ENTER,
      [0x0A] = KEY_RCTRL,
      [0x0B] = KP_SLASH,
      [0x0C] = KEY_PRINTSCREEN,
      [0x0D] = KEY_RALT,
      [0x0E] = KEY_HOME,
      [0x0F] = KEY_UP,
      [0x10] = KEY_PAGEUP,
      [0x11] = KEY_LEFT,
      [0x12] = KEY_RIGHT,
      [0x13] = KEY_END,
      [0x14] = KEY_DOWN,
      [0x15] = KEY_PAGEDOWN,
      [0x16] = KEY_INSERT,
      [0x17] = KEY_DELETE,
      /* 0x5B = Left Windows key, 0x5C = Right Windows key, 0x5D = Menu key */
      [0x5B] = KEY_LWIN,
      [0x5C] = KEY_RWIN,
      [0x5D] = KEY_MENU,
};

static void __kbd_update_led(void)
{
    kbd_wait_input();
    kbd_write_data(KBD_CMD_SET_LED);
    kbd_wait_input();
    kbd_write_data(kbd_led);
}

static int __kbd_buffer_put(const key_event_t* ev)
{
    uint32_t next = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next == kbd_tail) return -1;
    kbd_buffer[kbd_head] = *ev;
    kbd_head = next;
    return 0;
}

static void __kbd_handle_key(uint8_t scancode)
{
    key_event_t ev;
    memset(&ev, 0, sizeof(ev));

    if (scancode == 0xE0) {
        kbd_extended = 1;
        return;
    }

    if (scancode == 0xE1) {
        return;
    }

    int release = 0;

    if (scancode & 0x80) {
        release = 1;
        scancode &= 0x7F;
    }

    if (!kbd_extended) {
        switch (scancode) {
            case 0x2A:
                if (release) kbd_modifiers &= ~KEYMOD_LSHIFT;
                else kbd_modifiers |= KEYMOD_LSHIFT;
                return;
            case 0x36:
                if (release) kbd_modifiers &= ~KEYMOD_RSHIFT;
                else kbd_modifiers |= KEYMOD_RSHIFT;
                return;
            case 0x1D:
                if (kbd_extended) {
                    if (release) kbd_modifiers &= ~KEYMOD_RCTRL;
                    else kbd_modifiers |= KEYMOD_RCTRL;
                } else {
                    if (release) kbd_modifiers &= ~KEYMOD_LCTRL;
                    else kbd_modifiers |= KEYMOD_LCTRL;
                }
                kbd_extended = 0;
                return;
            case 0x38:
                if (kbd_extended) {
                    if (release) kbd_modifiers &= ~KEYMOD_RALT;
                    else kbd_modifiers |= KEYMOD_RALT;
                } else {
                    if (release) kbd_modifiers &= ~KEYMOD_LALT;
                    else kbd_modifiers |= KEYMOD_LALT;
                }
                kbd_extended = 0;
                return;
            case 0x3A:
                if (!release) {
                    kbd_modifiers ^= KEYMOD_CAPSLOCK;
                    kbd_led = (kbd_modifiers & KEYMOD_CAPSLOCK) ? (kbd_led | KBD_LED_CAPS) : (kbd_led & ~KBD_LED_CAPS);
                    __kbd_update_led();
                }
                return;
            case 0x45:
                if (!release) {
                    kbd_modifiers ^= KEYMOD_NUMLOCK;
                    kbd_led = (kbd_modifiers & KEYMOD_NUMLOCK) ? (kbd_led | KBD_LED_NUM) : (kbd_led & ~KBD_LED_NUM);
                    __kbd_update_led();
                }
                return;
            case 0x46:
                if (!release) {
                    kbd_modifiers ^= KEYMOD_SCROLLLOCK;
                    kbd_led = (kbd_modifiers & KEYMOD_SCROLLLOCK) ? (kbd_led | KBD_LED_SCROLL) : (kbd_led & ~KBD_LED_SCROLL);
                    __kbd_update_led();
                }
                return;
        }
    }

    /* Handle Win key modifier state (E0 5B / E0 5C) */
    if (kbd_extended && (scancode == 0x5B || scancode == 0x5C)) {
        if (scancode == 0x5B) {
            if (release) kbd_modifiers &= ~KEYMOD_LWIN;
            else kbd_modifiers |= KEYMOD_LWIN;
        } else {
            if (release) kbd_modifiers &= ~KEYMOD_RWIN;
            else kbd_modifiers |= KEYMOD_RWIN;
        }
        /* Still emit key event so GUI can use it */
    }

    int was_extended = kbd_extended;
    kbd_extended = 0;

    if (was_extended) {
        ev.keycode = (scancode < 128) ? scancode_e0_press[scancode] : KEY_NONE;
    } else {
        ev.keycode = (scancode < 128) ? scancode_set1_normal[scancode] : KEY_NONE;
    }

    ev.pressed = !release;
    ev.modifiers = kbd_modifiers;

    ev.ascii = 0;
    if (ev.pressed && ev.keycode < 128) {
        if (kbd_modifiers & (KEYMOD_LSHIFT | KEYMOD_RSHIFT)) {
            ev.ascii = scancode_set1_shift[scancode];
        } else {
            ev.ascii = scancode_set1_normal[scancode];
        }

        if (kbd_modifiers & KEYMOD_CAPSLOCK) {
            if (ev.ascii >= 'a' && ev.ascii <= 'z') {
                ev.ascii -= 32;
            } else if (ev.ascii >= 'A' && ev.ascii <= 'Z') {
                ev.ascii += 32;
            }
        }
    }

    __kbd_buffer_put(&ev);

    if (kbd_callback) {
        kbd_callback(&ev);
    }

    if (ev.pressed && ev.ascii >= 32 && ev.ascii < 127) {
        vga_putchar(ev.ascii);
    }
}

void keyboard_init(void)
{
    #define KBD_SERIAL_DBG(c) do { __asm__ volatile ("outb %0, %1" : : "a"((char)(c)), "d"((unsigned short)0x3F8)); } while (0)
    KBD_SERIAL_DBG('k');
    kbd_head = kbd_tail = 0;
    kbd_modifiers = 0;
    kbd_led = 0;
    kbd_extended = 0;
    kbd_callback = NULL;
    spin_init(&kbd_lock);

    for (volatile int i = 0; i < 100000; i++) {
        if (inb(KBD_CMD_PORT) & 0x01) (void)kbd_read_data();
        else break;
    }
    KBD_SERIAL_DBG('1');

    interrupt_register(1, (void*)keyboard_irq_handler);
    pic_enable(1);
    KBD_SERIAL_DBG('2');
    #undef KBD_SERIAL_DBG
}

void keyboard_irq_handler(void)
{
    uint8_t status = inb(KBD_CMD_PORT);

    if (!(status & 0x01)) return;
    if (status & 0x20) {

        kbd_wait_input();
        kbd_write_data(0xFE);
        return;
    }

    uint8_t scancode = inb(KBD_DATA_PORT);
    __kbd_handle_key(scancode);
}

key_event_t keyboard_read(void)
{
    key_event_t ev;
    ev.keycode = KEY_NONE;
    ev.pressed = 0;

    while (kbd_head == kbd_tail) {

        __asm__ volatile ("hlt");
    }

    spin_lock(&kbd_lock);
    if (kbd_head != kbd_tail) {
        ev = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    }
    spin_unlock(&kbd_lock);
    return ev;
}

int keyboard_poll(key_event_t* event)
{
    if (kbd_head == kbd_tail) return 0;

    spin_lock(&kbd_lock);
    *event = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    spin_unlock(&kbd_lock);
    return 1;
}

void keyboard_set_callback(kbd_callback_t cb)
{
    kbd_callback = cb;
}

uint32_t keyboard_get_modifiers(void)
{
    return kbd_modifiers;
}

void keyboard_set_led(uint8_t led_mask)
{
    kbd_led = led_mask & (KBD_LED_SCROLL | KBD_LED_NUM | KBD_LED_CAPS);
    __kbd_update_led();
}
