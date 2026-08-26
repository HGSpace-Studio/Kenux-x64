#ifndef KENUX_RENDER_H
#define KENUX_RENDER_H

#include "types.h"
#include "window.h"
#include "widget.h"
#include "icon.h"
#include "color.h"
#include "kapi_netdevice.h"

/* ============================================================
 * KENUX Render API — Curve Icon & Resource Visualization Layer
 * All rendering calls use KENUX_Render_* prefix
 * All data calls use KAPI_* prefix
 * ============================================================ */

/* Curve icon package IDs */
typedef enum {
    CURVE_ICON_PKG_NONE = 0,
    CURVE_ICON_PKG_DEFAULT = 1,
    CURVE_ICON_PKG_DARK = 2,
    CURVE_ICON_PKG_FLUENT = 3
} curve_icon_package_id_t;

/* Resource type for curve binding */
typedef enum {
    KENUX_RESOURCE_CPU = 0,
    KENUX_RESOURCE_MEM = 1,
    KENUX_RESOURCE_DISK = 2,
    KENUX_RESOURCE_NET = 3,
    KENUX_RESOURCE_GPU = 4,
    KENUX_RESOURCE_TEMP = 5
} kenux_resource_t;

/* Event types for event-driven rendering */
typedef enum {
    KAPI_EVENT_NONE = 0,
    KAPI_EVENT_RESOURCE_CHANGE = 1,
    KAPI_EVENT_LOG_CHUNK = 2,
    KAPI_EVENT_PROCESS_CHANGE = 3,
    KAPI_EVENT_MOUNT_CHANGE = 4,
    KAPI_EVENT_REFRESH = 5
} kapi_event_type_t;

/* Error codes */
typedef enum {
    KAPI_OK = 0,
    KAPI_ERROR_INVALID_PATH = -1,
    KAPI_ERROR_ICON_PACKAGE_LOAD_FAILED = -2,
    KAPI_ERROR_ICON_VERSION_MISMATCH = -3,
    KAPI_ERROR_NOT_FOUND = -4,
    KAPI_ERROR_PERMISSION_DENIED = -5,
    KAPI_ERROR_OUT_OF_MEMORY = -6
} kapi_error_t;

/* Curve data point */
typedef struct {
    uint32_t timestamp;
    uint32_t value;  /* 0-100 percentage */
} curve_point_t;

#define CURVE_HISTORY_MAX 60

typedef struct {
    curve_point_t points[CURVE_HISTORY_MAX];
    uint32_t count;
    uint32_t current_value;
} curve_history_t;

/* ---- KENUX Render API functions ---- */

/* Icon package lifecycle */
int32_t KENUX_Render_LoadIconPackage(curve_icon_package_id_t pkg_id);
void KENUX_Render_UnloadIconPackage(curve_icon_package_id_t pkg_id);
void KENUX_Render_SetIconVersion(curve_icon_package_id_t pkg_id, uint32_t version);

/* Icon rendering */
void KENUX_Render_SetIcon(icon_id_t icon_id);
void KENUX_Render_DrawIcon(uint32_t x, uint32_t y, icon_id_t icon_id, uint32_t size);
void KENUX_Render_AdaptCurveIcon(icon_id_t icon_id, uint32_t container_size);

/* Curve binding & drawing */
void KENUX_Render_BindCurveIcon(uint32_t column_id, kapi_event_type_t event,
                                curve_icon_package_id_t pkg_id);
void KENUX_Render_DrawCurve(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                            const curve_history_t* history, uint32_t color,
                            kenux_resource_t resource_type);
void KENUX_Render_DrawDashboardCard(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                    kenux_resource_t resource, uint32_t value,
                                    curve_icon_package_id_t pkg_id);
void KENUX_Render_BindDashboardCurve(uint32_t card_id, kapi_event_type_t event,
                                     curve_icon_package_id_t pkg_id);
void KENUX_Render_BindStorageCurve(uint32_t disk_id, kapi_event_type_t event,
                                   curve_icon_package_id_t pkg_id);
void KENUX_Render_BindLogCurve(uint32_t sandbox_id, kapi_event_type_t event,
                               curve_icon_package_id_t pkg_id);
void KENUX_Render_BindUptimeCurve(uint32_t uptime_sec, curve_icon_package_id_t pkg_id);
void KENUX_Render_LoadCurveHistory(uint32_t pid, curve_icon_package_id_t pkg_id);
void KENUX_Render_ExportReportCurve(uint32_t sandbox_id, curve_icon_package_id_t pkg_id);

/* Error icon display */
void KENUX_Render_ShowErrorIcon(int32_t error_code, curve_icon_package_id_t pkg_id);

/* ---- KAPI data access functions ---- */

/* Resource monitoring */
uint32_t KAPI_System_GetCPUUsage(void);
uint32_t KAPI_System_GetMemUsage(void);
uint32_t KAPI_System_GetDiskUsage(void);
uint32_t KAPI_System_GetNetUsage(void);
uint32_t KAPI_System_Uptime(void);
uint32_t KAPI_System_GetMemTotal(void);
uint32_t KAPI_System_GetMemFree(void);
void KAPI_System_GetCPUModel(char* buf, uint32_t buf_size);
void KAPI_System_GetOSVersion(char* buf, uint32_t buf_size);

/* Hardware icons */
icon_id_t KAPI_System_GetHardwareIcon(uint32_t hardware_id);
icon_id_t KAPI_System_GetRefreshIcon(void);

/* Process management */
typedef struct {
    uint32_t pid;
    char name[32];
    uint32_t cpu_usage;
    uint32_t mem_usage;
    uint32_t status;  /* 0=running, 1=suspended, 2=terminated */
} kapi_process_info_t;

uint32_t KAPI_Process_GetCount(void);
int32_t KAPI_Process_GetList(kapi_process_info_t* list, uint32_t max_count);
int32_t KAPI_Process_Details(uint32_t pid, kapi_process_info_t* info);
icon_id_t KAPI_Process_GetStatusIcon(uint32_t pid);
int32_t KAPI_Process_Kill(uint32_t pid);

/* VFS */
icon_id_t KAPI_VFS_GetFileIcon(const char* path, curve_icon_package_id_t pkg_id);
icon_id_t KAPI_VFS_GetMountStatusIcon(uint32_t mount_id);
int32_t KAPI_VFS_PathParse(const char* path);

/* Sandbox */
typedef struct {
    uint32_t id;
    char name[32];
    uint32_t status;  /* 0=starting, 1=running, 2=stopped, 3=error */
    uint32_t uptime;
} kapi_sandbox_info_t;

uint32_t KAPI_Sandbox_GetCount(void);
int32_t KAPI_Sandbox_GetList(kapi_sandbox_info_t* list, uint32_t max_count);
icon_id_t KAPI_Sandbox_GetStatusIcon(uint32_t sandbox_id);
int32_t KAPI_Sandbox_SetEnv(uint32_t sandbox_id, const char* key, const char* value);
int32_t KAPI_Sandbox_SetWorkDir(uint32_t sandbox_id, const char* path);

/* ---- Network interface info ---- */
typedef struct {
    char name[32];
    uint32_t ip_addr;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_packets;
    uint32_t tx_packets;
} kapi_net_info_t;

uint32_t KAPI_Net_GetInterfaceCount(void);
int32_t KAPI_Net_GetInterfaceList(kapi_net_info_t* list, uint32_t max_count);
int32_t KAPI_Connectivity_GetStatus(kapi_connectivity_status_t* status);

/* ---- Disk info ---- */
typedef struct {
    char name[32];
    uint64_t total_sectors;
    uint32_t sector_size;
    uint64_t capacity_bytes;
    bool readonly;
} kapi_disk_info_t;

uint32_t KAPI_Disk_GetCount(void);
int32_t KAPI_Disk_GetList(kapi_disk_info_t* list, uint32_t max_count);

/* ---- VFS mount point info ---- */
typedef struct {
    char mountpoint[64];
    char fstype[16];
    char device[32];
    bool mounted;
} kapi_mount_info_t;

uint32_t KAPI_VFS_GetMountCount(void);
int32_t KAPI_VFS_GetMountList(kapi_mount_info_t* list, uint32_t max_count);

/* ---- Curve history management (internal helper) ---- */
void kenux_render_update_history(void);
const curve_history_t* kenux_render_get_history(kenux_resource_t resource);

#endif /* KENUX_RENDER_H */
