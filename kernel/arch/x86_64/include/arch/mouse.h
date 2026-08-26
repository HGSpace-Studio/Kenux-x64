#ifndef ARCH_X86_64_MOUSE_H
#define ARCH_X86_64_MOUSE_H

#include <arch/types.h>

#define MOUSE_DATA_PORT   0x60
#define MOUSE_CMD_PORT    0x64

#define MOUSE_CMD_AUX_ENABLE      0xA8
#define MOUSE_CMD_AUX_DISABLE     0xA7
#define MOUSE_CMD_AUX_WRITE       0xD4

#define MOUSE_CMD_ENABLE         0xF4
#define MOUSE_CMD_DISABLE        0xF5
#define MOUSE_CMD_RESET          0xFF
#define MOUSE_CMD_SET_RATE        0xF3
#define MOUSE_CMD_SET_SCALE_1     0xE6
#define MOUSE_CMD_SET_SCALE_2     0xE7
#define MOUSE_CMD_SET_RESOLUTION  0xE8
#define MOUSE_CMD_GET_STATUS      0xE9
#define MOUSE_CMD_SET_STREAM      0xEA
#define MOUSE_CMD_READ_DATA       0xEB
#define MOUSE_CMD_SET_WRAP        0xEC
#define MOUSE_CMD_REMOTE_MODE     0xF0
#define MOUSE_CMD_GET_DEVICE_ID   0xF2

#define MOUSE_RESP_ACK            0xFA
#define MOUSE_RESP_NAK            0xFE
#define MOUSE_RESP_ERROR          0xFC
#define MOUSE_RESP_RESEND         0xFE
#define MOUSE_RESP_BAT_OK         0xAA
#define MOUSE_RESP_ID_PS2         0x00
#define MOUSE_RESP_ID_WHEEL       0x03
#define MOUSE_RESP_ID_5BTN        0x04

#define MOUSE_IRQ                 12

#define MOUSE_BTN_LEFT            0x01
#define MOUSE_BTN_RIGHT           0x02
#define MOUSE_BTN_MIDDLE          0x04
#define MOUSE_BTN_4               0x08
#define MOUSE_BTN_5               0x10

#define MOUSE_BUFFER_SIZE         256

#define MOUSE_SAMPLE_RATE_40     40
#define MOUSE_SAMPLE_RATE_60     60
#define MOUSE_SAMPLE_RATE_80     80
#define MOUSE_SAMPLE_RATE_100    100
#define MOUSE_SAMPLE_RATE_200    200

typedef struct {
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    int8_t wheel;
} mouse_packet_t;

typedef void (*mouse_callback_t)(const mouse_packet_t* packet);

void mouse_init(void);
void mouse_irq_handler(void);
int mouse_poll(mouse_packet_t* packet);
void mouse_set_callback(mouse_callback_t cb);
void mouse_set_sample_rate(uint8_t rate);
int mouse_get_device_id(uint8_t* id);
void mouse_set_resolution(uint8_t resolution);

#endif
