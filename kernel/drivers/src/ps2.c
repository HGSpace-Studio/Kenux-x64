#include <arch/types.h>
#include <arch/spinlock.h>
#include <arch/io.h>
#include <string.h>

#define PS2_DATA_PORT    0x60
#define PS2_CMD_PORT     0x64
#define PS2_STATUS_PORT  0x64

#define PS2_CMD_READ_CONFIG    0x20
#define PS2_CMD_WRITE_CONFIG   0x60
#define PS2_CMD_DISABLE_PORT2  0xA7
#define PS2_CMD_ENABLE_PORT2   0xA8
#define PS2_CMD_TEST_CTRL      0xAA
#define PS2_CMD_TEST_PORT1     0xAB
#define PS2_CMD_TEST_PORT2     0xA9
#define PS2_CMD_DISABLE_PORT1  0xAD
#define PS2_CMD_ENABLE_PORT1   0xAE
#define PS2_CMD_WRITE_PORT2    0xD4

#define PS2_KBD_SET_LEDS       0xED
#define PS2_KBD_SET_SCANCODE   0xF0
#define PS2_KBD_IDENTIFY       0xF2
#define PS2_KBD_ENABLE         0xF4
#define PS2_KBD_DISABLE        0xF5
#define PS2_KBD_RESET          0xFF

#define PS2_MOUSE_SET_SCALE    0xE6
#define PS2_MOUSE_SET_STREAM   0xEA
#define PS2_MOUSE_ENABLE       0xF4
#define PS2_MOUSE_DISABLE      0xF5
#define PS2_MOUSE_RESET        0xFF
#define PS2_MOUSE_SET_SAMPLE   0xF3
#define PS2_MOUSE_SET_RES      0xE8
#define PS2_MOUSE_GET_STATUS   0xE9

#define PS2_CONFIG_INT1        0x01
#define PS2_CONFIG_INT2        0x02
#define PS2_CONFIG_SYS         0x04
#define PS2_CONFIG_CLK1        0x10
#define PS2_CONFIG_CLK2        0x20
#define PS2_CONFIG_KBD_TRANS   0x40

typedef struct {
    int      present;
    int      has_scroll;
    int      has_5btn;
    int      packet_idx;
    uint8_t  packet[4];
    int32_t  dx;
    int32_t  dy;
    int32_t  dz;
    uint8_t  buttons;
    spinlock_t lock;
} ps2_mouse_t;

typedef struct {
    int      present;
    int      extended;
    uint8_t  leds;
    uint8_t  scancode_set;
    spinlock_t lock;
} ps2_kbd_t;

typedef struct {
    ps2_kbd_t   kbd;
    ps2_mouse_t mouse;
    int         dual_channel;
    spinlock_t  lock;
} ps2_ctrl_t;

static int ps2_wait_read(void)
{
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS_PORT) & 0x01) return 0;
    }
    return -1;
}

static int ps2_wait_write(void)
{
    for (int i = 0; i < 100000; i++) {
        if (!(inb(PS2_STATUS_PORT) & 0x02)) return 0;
    }
    return -1;
}

static uint8_t ps2_read_data(void)
{
    ps2_wait_read();
    return inb(PS2_DATA_PORT);
}

static void ps2_write_cmd(uint8_t cmd)
{
    ps2_wait_write();
    outb(PS2_CMD_PORT, cmd);
}

static void ps2_write_data(uint8_t data)
{
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);
}

static int ps2_send_kbd(uint8_t cmd)
{
    spinlock_acquire(&_ps2_ctrl.lock);
    int retries = 0;
    while (retries < 3) {
        ps2_write_data(cmd);
        uint8_t resp = ps2_read_data();
        if (resp == 0xFA) { spinlock_release(&_ps2_ctrl.lock); return 0; }
        if (resp == 0xFE) { retries++; continue; }
        break;
    }
    spinlock_release(&_ps2_ctrl.lock);
    return -1;
}

static int ps2_send_mouse(uint8_t cmd)
{
    spinlock_acquire(&_ps2_ctrl.lock);
    int retries = 0;
    while (retries < 3) {
        ps2_write_cmd(PS2_CMD_WRITE_PORT2);
        ps2_write_data(cmd);
        uint8_t resp = ps2_read_data();
        if (resp == 0xFA) { spinlock_release(&_ps2_ctrl.lock); return 0; }
        if (resp == 0xFE) { retries++; continue; }
        break;
    }
    spinlock_release(&_ps2_ctrl.lock);
    return -1;
}

static ps2_ctrl_t _ps2_ctrl;

void ps2_init(void)
{
    memset(&_ps2_ctrl, 0, sizeof(ps2_ctrl_t));
    spin_init(&_ps2_ctrl.lock);
    spin_init(&_ps2_ctrl.kbd.lock);
    spin_init(&_ps2_ctrl.mouse.lock);

    ps2_write_cmd(PS2_CMD_DISABLE_PORT1);
    ps2_write_cmd(PS2_CMD_DISABLE_PORT2);

    while (inb(PS2_STATUS_PORT) & 0x01) inb(PS2_DATA_PORT);

    ps2_write_cmd(PS2_CMD_READ_CONFIG);
    uint8_t config = ps2_read_data();
    config &= ~(PS2_CONFIG_INT1 | PS2_CONFIG_INT2 | PS2_CONFIG_KBD_TRANS);
    ps2_write_cmd(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config);

    ps2_write_cmd(PS2_CMD_TEST_CTRL);
    uint8_t test = ps2_read_data();
    if (test != 0x55) return;

    ps2_write_cmd(PS2_CMD_ENABLE_PORT2);
    ps2_write_cmd(PS2_CMD_READ_CONFIG);
    config = ps2_read_data();
    _ps2_ctrl.dual_channel = !(config & PS2_CONFIG_CLK2);
    ps2_write_cmd(PS2_CMD_DISABLE_PORT2);

    ps2_write_cmd(PS2_CMD_TEST_PORT1);
    test = ps2_read_data();
    _ps2_ctrl.kbd.present = (test == 0x00);

    if (_ps2_ctrl.dual_channel) {
        ps2_write_cmd(PS2_CMD_TEST_PORT2);
        test = ps2_read_data();
        _ps2_ctrl.mouse.present = (test == 0x00);
    }

    if (_ps2_ctrl.kbd.present) {
        ps2_send_kbd(PS2_KBD_RESET);
        ps2_send_kbd(PS2_KBD_ENABLE);
        ps2_send_kbd(PS2_KBD_SET_SCANCODE);
        ps2_send_kbd(2);
    }

    if (_ps2_ctrl.mouse.present) {
        ps2_send_mouse(PS2_MOUSE_RESET);
        ps2_send_mouse(PS2_MOUSE_SET_SAMPLE);
        ps2_send_mouse(200);
        ps2_send_mouse(PS2_MOUSE_SET_SAMPLE);
        ps2_send_mouse(100);
        ps2_send_mouse(PS2_MOUSE_SET_SAMPLE);
        ps2_send_mouse(80);
        ps2_send_mouse(PS2_MOUSE_IDENTIFY);
        uint8_t id = ps2_read_data();
        if (id == 0x03) _ps2_ctrl.mouse.has_scroll = 1;
        ps2_send_mouse(PS2_MOUSE_ENABLE);
    }

    ps2_write_cmd(PS2_CMD_READ_CONFIG);
    config = ps2_read_data();
    if (_ps2_ctrl.kbd.present) config |= PS2_CONFIG_INT1;
    if (_ps2_ctrl.mouse.present) config |= PS2_CONFIG_INT2;
    ps2_write_cmd(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config);

    ps2_write_cmd(PS2_CMD_ENABLE_PORT1);
    if (_ps2_ctrl.mouse.present) ps2_write_cmd(PS2_CMD_ENABLE_PORT2);
}

void ps2_kbd_irq(void)
{
    if (!_ps2_ctrl.kbd.present) return;
    uint8_t scancode = inb(PS2_DATA_PORT);
    (void)scancode;
}

void ps2_mouse_irq(void)
{
    if (!_ps2_ctrl.mouse.present) return;

    spinlock_acquire(&_ps2_ctrl.mouse.lock);
    uint8_t data = inb(PS2_DATA_PORT);
    ps2_mouse_t* m = &_ps2_ctrl.mouse;

    m->packet[m->packet_idx++] = data;
    int needed = m->has_scroll ? 4 : 3;

    if (m->packet_idx >= needed) {
        m->packet_idx = 0;
        uint8_t flags = m->packet[0];

        if (flags & 0x40) { spinlock_release(&m->lock); return; }
        if (flags & 0x80) { spinlock_release(&m->lock); return; }

        m->buttons = flags & 0x07;
        m->dx = (flags & 0x10) ? (int32_t)(m->packet[1] | 0xFFFFFF00) : m->packet[1];
        m->dy = (flags & 0x20) ? (int32_t)(m->packet[2] | 0xFFFFFF00) : m->packet[2];
        m->dy = -m->dy;

        if (m->has_scroll && needed >= 4) {
            m->dz = (int8_t)m->packet[3];
            if (m->dz == -1) m->dz = 1;
            else if (m->dz == 1) m->dz = -1;
            else m->dz = 0;
        }
    }
    spinlock_release(&_ps2_ctrl.mouse.lock);
}

void ps2_kbd_set_leds(uint8_t leds)
{
    if (!_ps2_ctrl.kbd.present) return;
    ps2_send_kbd(PS2_KBD_SET_LEDS);
    ps2_send_kbd(leds & 0x07);
    _ps2_ctrl.kbd.leds = leds & 0x07;
}