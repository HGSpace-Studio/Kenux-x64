#ifndef POWER_MANAGEMENT_ENHANCED_H
#define POWER_MANAGEMENT_ENHANCED_H

#include <stdint.h>
#include <stddef.h>

// 电源管理增强功能定义
typedef struct {
    uint32_t shutdown_flags;
    uint32_t reboot_flags;
    uint32_t halt_flags;
    uint64_t last_shutdown_time;
    uint64_t last_reboot_time;
    uint32_t shutdown_count;
    uint32_t reboot_count;
} power_management_t;

// 关机标志
#define SHUTDOWN_FLAG_SAFE_MODE    (1 << 0)
#define SHUTDOWN_FLAG_FORCE        (1 << 1)
#define SHUTDOWN_FLAG_PRESERVE_LOG (1 << 2)
#define SHUTDOWN_FLAG_SYNC_FS      (1 << 3)

// 重启标志
#define REBOOT_FLAG_SAFE_MODE      (1 << 0)
#define REBOOT_FLAG_FORCE          (1 << 1)
#define REBOOT_FLAG_WARM           (1 << 2)
#define REBOOT_FLAG_COLD           (1 << 3)

// 停机标志
#define HALT_FLAG_SAFE_MODE        (1 << 0)
#define HALT_FLAG_FORCE            (1 << 1)
#define HALT_FLAG_DUMP_STATE       (1 << 2)

// 函数声明
void power_management_init(void);
int power_management_check_permission(const char* operation);
int power_management_save_state(void);
int power_management_shutdown(uint32_t flags);
int power_management_reboot(uint32_t flags);
int power_management_halt(uint32_t flags);
int power_management_get_stats(char* buffer, size_t buffer_size);
int power_management_set_shutdown_flags(uint32_t flags);
int power_management_set_reboot_flags(uint32_t flags);
int power_management_set_halt_flags(uint32_t flags);
int power_management_schedule_shutdown(uint32_t delay_seconds, uint32_t flags);
int power_management_schedule_reboot(uint32_t delay_seconds, uint32_t flags);
int power_management_cancel_scheduled(void);
const char* power_management_get_status(void);

// 电源管理命令
int power_management_shell_commands(char** args, int arg_count);

#endif /* POWER_MANAGEMENT_ENHANCED_H */