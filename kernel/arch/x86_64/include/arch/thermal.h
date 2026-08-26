#ifndef _ARCH_THERMAL_H
#define _ARCH_THERMAL_H

#include <arch/types.h>
#include <arch/spinlock.h>

#define THERMAL_NAME_MAX       32
#define THERMAL_TRIP_MAX       12
#define THERMAL_MAX_ZONES      16
#define THERMAL_MAX_CDEV       16
#define THERMAL_MAX_GOV        8

#define MSR_IA32_TEMPERATURE_TARGET 0x1A2
#define MSR_IA32_PACKAGE_THERM_STATUS 0x1B1
#define MSR_IA32_PACKAGE_THERM_INTERRUPT 0x1B2

#define THERMAL_TRIP_CRITICAL     0
#define THERMAL_TRIP_HOT          1
#define THERMAL_TRIP_PASSIVE      2
#define THERMAL_TRIP_ACTIVE       3
#define THERMAL_TRIP_POINT_SENSOR 4

#define THERMAL_CDEV_TYPE_FAN     0
#define THERMAL_CDEV_TYPE_FREQ    1
#define THERMAL_CDEV_TYPE_LCD     2
#define THERMAL_CDEV_TYPE_PROC    3
#define THERMAL_CDEV_TYPE_RAW     4

typedef struct {
    int type;
    int temperature;
    int hysteresis;
    void *priv;
} thermal_trip_t;

typedef struct thermal_cooling_device thermal_cooling_device_t;

typedef struct {
    int (*get_max_state)(thermal_cooling_device_t *cdev, unsigned long *state);
    int (*get_cur_state)(thermal_cooling_device_t *cdev, unsigned long *state);
    int (*set_cur_state)(thermal_cooling_device_t *cdev, unsigned long state);
} thermal_cooling_ops_t;

struct thermal_cooling_device {
    char name[THERMAL_NAME_MAX];
    int cdev_type;
    unsigned long max_state;
    unsigned long cur_state;
    thermal_cooling_ops_t *ops;
    void *devdata;
    int id;
    spinlock_t lock;
};

typedef struct thermal_zone_device thermal_zone_device_t;

typedef struct thermal_zone_ops {
    int (*get_temp)(thermal_zone_device_t *tz, int *temp);
    int (*get_mode)(thermal_zone_device_t *tz, int *mode);
    int (*set_mode)(thermal_zone_device_t *tz, int mode);
    int (*get_trip_temp)(thermal_zone_device_t *tz, int trip, int *temp);
    int (*set_trip_temp)(thermal_zone_device_t *tz, int trip, int temp);
    int (*get_trip_type)(thermal_zone_device_t *tz, int trip, int *type);
} thermal_zone_ops_t;

typedef struct thermal_governor {
    char name[THERMAL_NAME_MAX];
    int (*throttle)(thermal_zone_device_t *tz, int trip);
    void (*update)(thermal_zone_device_t *tz);
    struct thermal_governor *next;
} thermal_governor_t;

struct thermal_zone_device {
    char type[THERMAL_NAME_MAX];
    int id;
    int temperature;
    int last_temperature;
    int mode;
    int passive;
    int forced_passive;
    thermal_trip_t trips[THERMAL_TRIP_MAX];
    int num_trips;
    thermal_cooling_device_t *cdevs[THERMAL_MAX_CDEV];
    int cdev_num;
    thermal_zone_ops_t *ops;
    thermal_governor_t *governor;
    void *devdata;
    spinlock_t lock;
    int polling_delay;
    int passive_delay;
    struct thermal_zone_device *next;
};

typedef void (*thermal_notify_t)(int temp, void *data);

typedef struct {
    thermal_notify_t callback;
    void *data;
} thermal_notify_entry_t;

#define THERMAL_MAX_NOTIFIES     16

void thermal_init(void);

thermal_zone_device_t *thermal_zone_register(const char *type, int trips, thermal_zone_ops_t *ops, void *data);
void thermal_zone_unregister(thermal_zone_device_t *tz);

int thermal_zone_get_temp(thermal_zone_device_t *tz, int *temp);
int thermal_zone_device_update(thermal_zone_device_t *tz);
void thermal_zone_device_set_polling(thermal_zone_device_t *tz, int delay);

thermal_cooling_device_t *thermal_cooling_device_register(const char *type, void *devdata, thermal_cooling_ops_t *ops);
void thermal_cooling_device_unregister(thermal_cooling_device_t *cdev);

int thermal_register_governor(thermal_governor_t *gov);
thermal_governor_t *thermal_get_governor(const char *name);

int thermal_cooling_set_cur_state(thermal_cooling_device_t *cdev, unsigned long state);
int thermal_cooling_get_cur_state(thermal_cooling_device_t *cdev, unsigned long *state);

void thermal_notify_register(thermal_notify_t callback, void *data);
void thermal_notify_unregister(thermal_notify_t callback);

int thermal_msrtmp_read(int *temp);

extern thermal_zone_device_t *thermal_zone_list;
extern thermal_cooling_device_t *thermal_cdev_list;

#endif
