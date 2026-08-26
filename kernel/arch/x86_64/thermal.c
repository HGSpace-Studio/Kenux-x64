#include <arch/thermal.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

thermal_zone_device_t *thermal_zone_list = NULL;
thermal_cooling_device_t *thermal_cdev_list = NULL;

static thermal_governor_t *governor_list = NULL;
static thermal_notify_entry_t thermal_notifies[THERMAL_MAX_NOTIFIES];
static u32 thermal_notify_count = 0;
static spinlock_t thermal_global_lock = SPINLOCK_INIT;
static u32 thermal_zone_next_id = 1;
static u32 thermal_cdev_next_id = 1;

static u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static u64 read_tsc(void)
{
    u32 lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

int thermal_msrtmp_read(int *temp)
{
    u64 msr_val = rdmsr(MSR_IA32_TEMPERATURE_TARGET);
    int target = (int)((msr_val >> 16) & 0xFF);

    u64 pkg_therm = rdmsr(MSR_IA32_PACKAGE_THERM_STATUS);
    int reading = (int)((pkg_therm >> 16) & 0x7F);

    *temp = target - reading;
    return 0;
}

static int default_get_temp(thermal_zone_device_t *tz, int *temp)
{
    (void)tz;
    int t = 0;
    thermal_msrtmp_read(&t);
    *temp = t;
    return 0;
}

static int default_get_mode(thermal_zone_device_t *tz, int *mode)
{
    *mode = tz->mode;
    return 0;
}

static int default_set_mode(thermal_zone_device_t *tz, int mode)
{
    tz->mode = mode;
    return 0;
}

static int default_get_trip_temp(thermal_zone_device_t *tz, int trip, int *temp)
{
    if (trip < 0 || trip >= tz->num_trips) return -1;
    *temp = tz->trips[trip].temperature;
    return 0;
}

static int default_get_trip_type(thermal_zone_device_t *tz, int trip, int *type)
{
    if (trip < 0 || trip >= tz->num_trips) return -1;
    *type = tz->trips[trip].type;
    return 0;
}

static thermal_zone_ops_t default_zone_ops = {
    .get_temp = default_get_temp,
    .get_mode = default_get_mode,
    .set_mode = default_set_mode,
    .get_trip_temp = default_get_trip_temp,
    .set_trip_temp = NULL,
    .get_trip_type = default_get_trip_type
};

static void thermal_notify_all(int temp)
{
    for (u32 i = 0; i < thermal_notify_count; i++) {
        if (thermal_notifies[i].callback) {
            thermal_notifies[i].callback(temp, thermal_notifies[i].data);
        }
    }
}

static int gov_fair_share_throttle(thermal_zone_device_t *tz, int trip)
{
    (void)trip;
    if (!tz) return -1;
    for (int i = 0; i < tz->cdev_num; i++) {
        thermal_cooling_device_t *cdev = tz->cdevs[i];
        if (cdev && cdev->ops && cdev->ops->set_cur_state) {
            unsigned long max_state = cdev->max_state;
            unsigned long target = max_state / 2;
            cdev->ops->set_cur_state(cdev, target);
        }
    }
    return 0;
}

static void gov_fair_share_update(thermal_zone_device_t *tz)
{
    (void)tz;
}

static thermal_governor_t gov_fair_share = {
    .name = "fair_share",
    .throttle = gov_fair_share_throttle,
    .update = gov_fair_share_update,
    .next = NULL
};

static int gov_step_wise_throttle(thermal_zone_device_t *tz, int trip)
{
    if (!tz || trip < 0 || trip >= tz->num_trips) return -1;
    thermal_trip_t *t = &tz->trips[trip];

    for (int i = 0; i < tz->cdev_num; i++) {
        thermal_cooling_device_t *cdev = tz->cdevs[i];
        if (cdev && cdev->ops && cdev->ops->set_cur_state && cdev->ops->get_cur_state) {
            unsigned long cur;
            cdev->ops->get_cur_state(cdev, &cur);
            if (tz->temperature >= t->temperature) {
                if (cur < cdev->max_state) {
                    cdev->ops->set_cur_state(cdev, cur + 1);
                }
            } else if (tz->temperature < t->temperature - t->hysteresis) {
                if (cur > 0) {
                    cdev->ops->set_cur_state(cdev, cur - 1);
                }
            }
        }
    }
    return 0;
}

static void gov_step_wise_update(thermal_zone_device_t *tz)
{
    (void)tz;
}

static thermal_governor_t gov_step_wise = {
    .name = "step_wise",
    .throttle = gov_step_wise_throttle,
    .update = gov_step_wise_update,
    .next = NULL
};

static int gov_user_space_throttle(thermal_zone_device_t *tz, int trip)
{
    (void)tz;
    (void)trip;
    return 0;
}

static void gov_user_space_update(thermal_zone_device_t *tz)
{
    (void)tz;
}

static thermal_governor_t gov_user_space = {
    .name = "user_space",
    .throttle = gov_user_space_throttle,
    .update = gov_user_space_update,
    .next = NULL
};

void thermal_init(void)
{
    thermal_zone_list = NULL;
    thermal_cdev_list = NULL;
    governor_list = NULL;
    thermal_notify_count = 0;
    thermal_zone_next_id = 1;
    thermal_cdev_next_id = 1;
    spin_init(&thermal_global_lock);

    thermal_register_governor(&gov_step_wise);
    thermal_register_governor(&gov_fair_share);
    thermal_register_governor(&gov_user_space);
}

thermal_zone_device_t *thermal_zone_register(const char *type, int trips, thermal_zone_ops_t *ops, void *data)
{
    if (!type || trips <= 0) return NULL;

    thermal_zone_device_t *tz = kzalloc(sizeof(thermal_zone_device_t));
    if (!tz) return NULL;

    strncpy(tz->type, type, THERMAL_NAME_MAX - 1);
    tz->type[THERMAL_NAME_MAX - 1] = '\0';
    tz->id = (int)thermal_zone_next_id++;
    tz->temperature = 0;
    tz->last_temperature = 0;
    tz->mode = 1;
    tz->passive = 0;
    tz->forced_passive = 0;
    tz->num_trips = trips > THERMAL_TRIP_MAX ? THERMAL_TRIP_MAX : trips;
    tz->cdev_num = 0;
    tz->devdata = data;
    tz->polling_delay = 1000;
    tz->passive_delay = 250;
    tz->next = NULL;
    spin_init(&tz->lock);

    if (ops) {
        tz->ops = ops;
    } else {
        tz->ops = &default_zone_ops;
    }

    tz->governor = thermal_get_governor("step_wise");

    spin_lock(&thermal_global_lock);
    tz->next = thermal_zone_list;
    thermal_zone_list = tz;
    spin_unlock(&thermal_global_lock);
    return tz;
}

void thermal_zone_unregister(thermal_zone_device_t *tz)
{
    if (!tz) return;
    spin_lock(&thermal_global_lock);
    thermal_zone_device_t **prev = &thermal_zone_list;
    while (*prev && *prev != tz) {
        prev = &(*prev)->next;
    }
    if (*prev) {
        *prev = tz->next;
    }
    spin_unlock(&thermal_global_lock);
    kfree(tz);
}

int thermal_zone_get_temp(thermal_zone_device_t *tz, int *temp)
{
    if (!tz || !temp) return -1;
    if (tz->ops && tz->ops->get_temp) {
        return tz->ops->get_temp(tz, temp);
    }
    return -1;
}

int thermal_zone_device_update(thermal_zone_device_t *tz)
{
    if (!tz) return -1;

    int temp;
    int ret = thermal_zone_get_temp(tz, &temp);
    if (ret != 0) return ret;

    spin_lock(&tz->lock);
    tz->last_temperature = tz->temperature;
    tz->temperature = temp;
    spin_unlock(&tz->lock);

    if (temp != tz->last_temperature) {
        thermal_notify_all(temp);
    }

    for (int i = 0; i < tz->num_trips; i++) {
        thermal_trip_t *trip = &tz->trips[i];
        if (tz->temperature >= trip->temperature) {
            if (trip->type == THERMAL_TRIP_CRITICAL) {
                continue;
            }
            if (trip->type == THERMAL_TRIP_PASSIVE) {
                tz->passive = 1;
            }
            if (trip->type == THERMAL_TRIP_ACTIVE || trip->type == THERMAL_TRIP_PASSIVE) {
                if (tz->governor && tz->governor->throttle) {
                    tz->governor->throttle(tz, i);
                }
            }
        } else if (tz->temperature < trip->temperature - trip->hysteresis) {
            if (trip->type == THERMAL_TRIP_PASSIVE) {
                tz->passive = 0;
            }
            if (tz->governor && tz->governor->update) {
                tz->governor->update(tz);
            }
        }
    }

    return 0;
}

void thermal_zone_device_set_polling(thermal_zone_device_t *tz, int delay)
{
    if (!tz) return;
    tz->polling_delay = delay;
}

thermal_cooling_device_t *thermal_cooling_device_register(const char *type, void *devdata, thermal_cooling_ops_t *ops)
{
    if (!type || !ops) return NULL;

    thermal_cooling_device_t *cdev = kzalloc(sizeof(thermal_cooling_device_t));
    if (!cdev) return NULL;

    strncpy(cdev->name, type, THERMAL_NAME_MAX - 1);
    cdev->name[THERMAL_NAME_MAX - 1] = '\0';
    cdev->cdev_type = THERMAL_CDEV_TYPE_FAN;
    cdev->max_state = 10;
    cdev->cur_state = 0;
    cdev->ops = ops;
    cdev->devdata = devdata;
    cdev->id = (int)thermal_cdev_next_id++;
    spin_init(&cdev->lock);

    if (ops->get_max_state) {
        unsigned long max_st = 0;
        ops->get_max_state(cdev, &max_st);
        cdev->max_state = max_st;
    }

    spin_lock(&thermal_global_lock);
    (void)cdev;
    spin_unlock(&thermal_global_lock);

    return cdev;
}

void thermal_cooling_device_unregister(thermal_cooling_device_t *cdev)
{
    if (!cdev) return;
    kfree(cdev);
}

int thermal_register_governor(thermal_governor_t *gov)
{
    if (!gov) return -1;
    spin_lock(&thermal_global_lock);
    gov->next = governor_list;
    governor_list = gov;
    spin_unlock(&thermal_global_lock);
    return 0;
}

thermal_governor_t *thermal_get_governor(const char *name)
{
    if (!name) return NULL;
    spin_lock(&thermal_global_lock);
    thermal_governor_t *gov = governor_list;
    while (gov) {
        if (strncmp(gov->name, name, THERMAL_NAME_MAX) == 0) {
            spin_unlock(&thermal_global_lock);
            return gov;
        }
        gov = gov->next;
    }
    spin_unlock(&thermal_global_lock);
    return NULL;
}

int thermal_cooling_set_cur_state(thermal_cooling_device_t *cdev, unsigned long state)
{
    if (!cdev || !cdev->ops || !cdev->ops->set_cur_state) return -1;
    if (state > cdev->max_state) return -1;
    spin_lock(&cdev->lock);
    cdev->cur_state = state;
    int ret = cdev->ops->set_cur_state(cdev, state);
    spin_unlock(&cdev->lock);
    return ret;
}

int thermal_cooling_get_cur_state(thermal_cooling_device_t *cdev, unsigned long *state)
{
    if (!cdev || !state) return -1;
    if (!cdev->ops || !cdev->ops->get_cur_state) return -1;
    spin_lock(&cdev->lock);
    int ret = cdev->ops->get_cur_state(cdev, state);
    spin_unlock(&cdev->lock);
    return ret;
}

void thermal_notify_register(thermal_notify_t callback, void *data)
{
    if (!callback || thermal_notify_count >= THERMAL_MAX_NOTIFIES) return;
    thermal_notifies[thermal_notify_count].callback = callback;
    thermal_notifies[thermal_notify_count].data = data;
    thermal_notify_count++;
}

void thermal_notify_unregister(thermal_notify_t callback)
{
    if (!callback) return;
    for (u32 i = 0; i < thermal_notify_count; i++) {
        if (thermal_notifies[i].callback == callback) {
            thermal_notifies[i] = thermal_notifies[thermal_notify_count - 1];
            thermal_notify_count--;
            return;
        }
    }
}
