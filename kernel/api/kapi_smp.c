

#include "kapi_smp.h"
#include "kapi.h"

int kapi_smp_processor_id(void)
{
    return 0;
}

int kapi_on_each_cpu(void (*func)(void*), void* arg)
{
    if (func) {
        func(arg);
    }
    return 0;
}
