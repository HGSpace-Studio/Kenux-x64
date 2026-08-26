#ifndef DRIVERS_H
#define DRIVERS_H

#include <arch/types.h>

#define DRIVER_MAX 32

typedef struct driver {
    const char* name;
    int (*init)(void);
    void* private_data;
} driver_t;

void drivers_init(void);
int driver_register(driver_t* driver);
int driver_init(const char* name);

#endif
