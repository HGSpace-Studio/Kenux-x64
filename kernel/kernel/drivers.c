#include "drivers.h"
#include <memory.h>
#include <string.h>

static driver_t drivers[DRIVER_MAX];
static int driver_count = 0;

void drivers_init(void)
{
    memset(drivers, 0, sizeof(drivers));
    driver_count = 0;
}

int driver_register(driver_t* driver)
{
    if (driver_count >= DRIVER_MAX) {
        return -1;
    }

    drivers[driver_count] = *driver;
    driver_count++;
    return 0;
}

int driver_init(const char* name)
{
    for (int i = 0; i < driver_count; i++) {
        if (strcmp(drivers[i].name, name) == 0) {
            return drivers[i].init();
        }
    }
    return -1;
}

void drivers_register(const char* name, void* init_func)
{
    if (driver_count >= DRIVER_MAX) return;
    drivers[driver_count].name = name;
    drivers[driver_count].init = (int (*)(void))init_func;
    drivers[driver_count].private_data = NULL;
    driver_count++;
}

void drivers_start(void)
{
    for (int i = 0; i < driver_count; i++) {
        if (drivers[i].init) {
            drivers[i].init();
        }
    }
}
