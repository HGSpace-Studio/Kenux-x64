#include "kapi.h"

int kapi_profiler_init(struct kapi_profiler** profiler)
{
    if (profiler) *profiler = NULL;
    return 0;
}

int kapi_memleak_init(void)
{
    return 0;
}

int kapi_logging_init(const kapi_logger_config_t* config)
{
    (void)config;
    return 0;
}

int kapi_trace_init(uint32_t buffer_size, uint32_t enabled_categories)
{
    (void)buffer_size;
    (void)enabled_categories;
    return 0;
}

int kapi_memory_ext_init(void)
{
    return 0;
}

int kapi_device_manager_init(void)
{
    return 0;
}