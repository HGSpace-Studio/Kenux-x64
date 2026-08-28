#ifndef KANVAS_SYSTEM_TRAY_H
#define KANVAS_SYSTEM_TRAY_H

#include "kapi_kanvasui.h"
#include <stdint.h>
#include <stdbool.h>

#define KANVAS_TRAY_ICON_SZ     20
#define KANVAS_TRAY_SPACING     4
#define KANVAS_TRAY_PAD         8
#define KANVAS_TRAY_POPUP_W     280
#define KANVAS_TRAY_POPUP_H     200
#define KANVAS_TRAY_POPUP_R     12
#define KANVAS_TRAY_MAX_IND     8

typedef enum {
    KANVAS_IND_NETWORK = 0,
    KANVAS_IND_BLUETOOTH,
    KANVAS_IND_VOLUME,
    KANVAS_IND_BRIGHTNESS,
    KANVAS_IND_BATTERY,
    KANVAS_IND_NOTIFICATION,
    KANVAS_IND_CLIPBOARD,
    KANVAS_IND_COUNT
} kanvas_tray_ind_id_t;

typedef enum {
    KANVAS_NET_OFF = 0,
    KANVAS_NET_WIFI,
    KANVAS_NET_ETHERNET,
    KANVAS_NET_CONNECTING
} kanvas_net_state_t;

typedef enum {
    KANVAS_BT_OFF = 0,
    KANVAS_BT_ON,
    KANVAS_BT_CONNECTED,
    KANVAS_BT_PAIRING
} kanvas_bt_state_t;

typedef struct {
    kanvas_net_state_t net_state;
    char wifi_ssid[32];
    int wifi_signal;
    bool wifi_list_visible;
    char wifi_networks[8][32];
    int wifi_network_count;
    int wifi_selected;
} kanvas_network_ind_t;

typedef struct {
    kanvas_bt_state_t bt_state;
    char paired_device[32];
    bool discoverable;
} kanvas_bluetooth_ind_t;

typedef struct {
    int volume;
    bool muted;
    int mic_volume;
    bool mic_muted;
} kanvas_volume_ind_t;

typedef struct {
    int brightness;
    int min_brightness;
    int max_brightness;
    bool auto_brightness;
    bool night_mode;
} kanvas_brightness_ind_t;

typedef struct {
    int percentage;
    bool charging;
    bool power_save;
    int time_remaining_min;
} kanvas_battery_ind_t;

typedef struct kanvas_system_tray_s {
    bool visible;
    int x, y;
    kanvas_tray_ind_id_t type;
    kui_color_t bg_color;
    kui_color_t fg_color;
    kui_color_t accent_color;
    kui_color_t hover_color;
    kanvas_network_ind_t network;
    kanvas_bluetooth_ind_t bluetooth;
    kanvas_volume_ind_t volume;
    kanvas_brightness_ind_t brightness;
    kanvas_battery_ind_t battery;
    bool popup_visible;
    kanvas_tray_ind_id_t popup_type;
    int popup_x, popup_y;
    int hovered_indicator;
    uint32_t anim_id;
} kanvas_system_tray_t;

kanvas_system_tray_t* kanvas_system_tray_create(void);
void kanvas_system_tray_destroy(kanvas_system_tray_t* tray);

void kanvas_system_tray_paint(kanvas_system_tray_t* tray, uint32_t* fb, int stride, int fw, int fh, int tray_x, int tray_y);
void kanvas_system_tray_paint_popup(kanvas_system_tray_t* tray, uint32_t* fb, int stride, int fw, int fh);
void kanvas_system_tray_handle_mouse(kanvas_system_tray_t* tray, int mx, int my, bool left_down, bool left_up);
void kanvas_system_tray_update(kanvas_system_tray_t* tray, uint64_t now_ms);

int kanvas_system_tray_get_width(kanvas_system_tray_t* tray);

void kanvas_system_tray_set_network(kanvas_system_tray_t* tray, kanvas_net_state_t state, const char* ssid, int signal);
void kanvas_system_tray_set_bluetooth(kanvas_system_tray_t* tray, kanvas_bt_state_t state, const char* device);
void kanvas_system_tray_set_volume(kanvas_system_tray_t* tray, int volume, bool muted);
void kanvas_system_tray_set_brightness(kanvas_system_tray_t* tray, int brightness);
void kanvas_system_tray_set_battery(kanvas_system_tray_t* tray, int percentage, bool charging);

void kanvas_system_tray_toggle_popup(kanvas_system_tray_t* tray, kanvas_tray_ind_id_t type);
void kanvas_system_tray_close_popup(kanvas_system_tray_t* tray);

#endif