

#include "kapi_time.h"
#include "kapi.h"

#include <arch/hpet.h>
#include <timer.h>
#include <string.h>

kapi_ktime_t kapi_ktime_get(void)
{
    return (kapi_ktime_t)hpet_get_ticks();
}

int64_t kapi_ktime_get_ns(void)
{
    return (int64_t)hpet_get_ticks();
}

uint64_t kapi_jiffies(void)
{
    return timer_get_jiffies();
}

uint64_t kapi_jiffies_to_ms(uint64_t j)
{
    return timer_jiffies_to_ms(j);
}

uint64_t kapi_ms_to_jiffies(uint64_t ms)
{
    return timer_ms_to_jiffies(ms);
}

static void kapi_time_copy_string(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    if (!dst || cap == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int kapi_system_info(kapi_system_info_t* info)
{
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    kapi_time_copy_string(info->kernel_name, sizeof(info->kernel_name), "Kenux");
    kapi_time_copy_string(info->kernel_version, sizeof(info->kernel_version), "26.7.9K");
    kapi_time_copy_string(info->middlelayer_name, sizeof(info->middlelayer_name), "KAPI");
    kapi_time_copy_string(info->build_time, sizeof(info->build_time), "static");
    kapi_time_copy_string(info->copyright, sizeof(info->copyright), "Kenux kernel API");
    info->version_major = 26;
    info->version_minor = 7;
    info->version_patch = 9;
    info->build_number = 1;
    info->copyright_year = 2026;
    return KAPI_OK;
}

int kapi_perf_info(kapi_perf_info_t* info)
{
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    info->uptime_ms = kapi_jiffies_to_ms(kapi_jiffies());
    return KAPI_OK;
}

int kapi_time_info(kapi_time_info_t* info)
{
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    info->uptime_ms = kapi_jiffies_to_ms(kapi_jiffies());
    return KAPI_OK;
}

int kapi_time_ntp_sync(uint32_t timeout_ms, kapi_time_sync_t* result)
{
    if (!result) return KAPI_EINVAL;
    memset(result, 0, sizeof(*result));
    result->timeout_ms = timeout_ms;
    result->status = KAPI_NET_STATUS_NO_DEVICE;
    return KAPI_ENOSYS;
}

int kapi_machine_identity(kapi_machine_identity_t* identity)
{
    if (!identity) return KAPI_EINVAL;
    memset(identity, 0, sizeof(*identity));
    identity->version = KAPI_MACHINE_IDENTITY_VERSION;
    identity->flags = KAPI_MACHINE_IDENTITY_FLAG_PLATFORM_UUID;
    kapi_time_copy_string(identity->source, sizeof(identity->source), "kenux");
    kapi_time_copy_string(identity->platform_uuid, sizeof(identity->platform_uuid),
                          "00000000-0000-4000-8000-4b454e555800");
    kapi_time_copy_string(identity->firmware_vendor, sizeof(identity->firmware_vendor),
                          "Kenux");
    return KAPI_OK;
}

int kapi_system_reboot(void)
{
    return KAPI_ENOSYS;
}

int kapi_system_shutdown(void)
{
    return KAPI_ENOSYS;
}
