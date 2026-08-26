

#ifndef KAPI_TIME_H
#define KAPI_TIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t kapi_ktime_t;

kapi_ktime_t kapi_ktime_get(void);

int64_t kapi_ktime_get_ns(void);

uint64_t kapi_jiffies(void);

uint64_t kapi_jiffies_to_ms(uint64_t j);

uint64_t kapi_ms_to_jiffies(uint64_t ms);

/* ===== Fused LeonOS system.h (system/perf/time/machine/reboot) ===== */
#define KAPI_SYSTEM_NAME_LEN      32U
#define KAPI_SYSTEM_VERSION_LEN   32U
#define KAPI_SYSTEM_TIME_LEN      32U
#define KAPI_SYSTEM_COPYRIGHT_LEN 96U

#define KAPI_MACHINE_IDENTITY_VERSION        1U
#define KAPI_MACHINE_IDENTITY_SOURCE_LEN     32U
#define KAPI_MACHINE_IDENTITY_UUID_LEN       37U
#define KAPI_MACHINE_IDENTITY_VENDOR_LEN     48U
#define KAPI_MACHINE_IDENTITY_FLAG_PLATFORM_UUID 0x00000001U

typedef struct {
    char     kernel_name[KAPI_SYSTEM_NAME_LEN];
    char     kernel_version[KAPI_SYSTEM_VERSION_LEN];
    char     middlelayer_name[KAPI_SYSTEM_NAME_LEN];
    char     build_time[KAPI_SYSTEM_TIME_LEN];
    char     copyright[KAPI_SYSTEM_COPYRIGHT_LEN];
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint32_t build_number;
    uint32_t copyright_year;
} kapi_system_info_t;

typedef struct {
    uint64_t uptime_ms;
    uint64_t total_memory_kib;
    uint64_t free_memory_kib;
    uint64_t busy_ticks;
    uint64_t idle_ticks;
    uint32_t task_count;
    uint32_t running_tasks;
    uint32_t ready_tasks;
    uint32_t sleeping_tasks;
} kapi_perf_info_t;

typedef struct {
    uint64_t unix_seconds;
    uint64_t uptime_ms;
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t valid;
    uint32_t reserved;
} kapi_time_info_t;

typedef struct {
    uint32_t timeout_ms;
    uint32_t status;
    uint32_t server_ip;
    uint32_t valid;
    uint64_t unix_seconds;
    char     server[128];
} kapi_time_sync_t;

typedef struct {
    uint32_t version;
    uint32_t flags;
    char     source[KAPI_MACHINE_IDENTITY_SOURCE_LEN];
    char     platform_uuid[KAPI_MACHINE_IDENTITY_UUID_LEN];
    char     boot_disk_guid[KAPI_MACHINE_IDENTITY_UUID_LEN];
    char     boot_partition_guid[KAPI_MACHINE_IDENTITY_UUID_LEN];
    char     firmware_vendor[KAPI_MACHINE_IDENTITY_VENDOR_LEN];
    uint32_t firmware_revision;
    uint32_t reserved;
} kapi_machine_identity_t;

int kapi_system_info(kapi_system_info_t* info);
int kapi_perf_info(kapi_perf_info_t* info);
int kapi_time_info(kapi_time_info_t* info);
int kapi_time_ntp_sync(uint32_t timeout_ms, kapi_time_sync_t* result);
int kapi_machine_identity(kapi_machine_identity_t* identity);
int kapi_system_reboot(void);
int kapi_system_shutdown(void);

/* LeonOS compat aliases */
#define KAPI_System_Info(p)    kapi_system_info((p))
#define KAPI_Perf_Info(p)      kapi_perf_info((p))
#define KAPI_Time_Info(p)      kapi_time_info((p))
#define KAPI_Time_NTPSync(t,r) kapi_time_ntp_sync((t),(r))
#define KAPI_Machine_Identity(p) kapi_machine_identity((p))
#define KAPI_System_Reboot()   kapi_system_reboot()
#define KAPI_System_Shutdown() kapi_system_shutdown()
typedef kapi_system_info_t    KAPI_SYSTEM_INFO;
typedef kapi_perf_info_t      KAPI_PERF_INFO;
typedef kapi_time_info_t      KAPI_TIME_INFO;
typedef kapi_time_sync_t      KAPI_TIME_SYNC;
typedef kapi_machine_identity_t KAPI_MACHINE_IDENTITY;

#ifdef __cplusplus
}
#endif

#endif
