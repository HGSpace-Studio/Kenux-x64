#include <arch/vga.h>
#include <string.h>
#include <memory.h>
#include <syscall.h>
#include <process.h>
#include <arch/security.h>
#include <arch/fs.h>

// 开关机系统增强功能
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

static power_management_t power_manager;

// 初始化电源管理系统
void power_management_init(void) {
    memset(&power_manager, 0, sizeof(power_manager));
    power_manager.shutdown_flags = SHUTDOWN_FLAG_PRESERVE_LOG | SHUTDOWN_FLAG_SYNC_FS;
    power_manager.reboot_flags = REBOOT_FLAG_COLD;
    power_manager.halt_flags = HALT_FLAG_DUMP_STATE;
}

// 检查是否有足够的权限执行关机操作
int power_management_check_permission(const char* operation) {
    // 检查当前用户权限
    if (security_check_cap(current_process(), CAP_SYS_ADMIN) != 0) {
        return -1;
    }
    
    return 0;
}

// 保存系统状态
int power_management_save_state(void) {
    // 保存当前进程状态
    if (current_process()) {
        process_save_state(current_process());
    }
    
    // 保存文件系统状态
    fs_sync();
    
    // 保存内存状态
    memory_dump_state();
    
    return 0;
}

// 执行关机操作
int power_management_shutdown(uint32_t flags) {
    if (power_management_check_permission("shutdown") != 0) {
        return -1;
    }
    
    // 检查是否强制关机
    if (!(flags & SHUTDOWN_FLAG_FORCE)) {
        // 通知所有进程系统即将关机
        process_broadcast_shutdown();
        
        // 等待进程清理
        int timeout = 1000000; // 1秒超时
        while (process_count_running() > 0 && timeout > 0) {
            timeout--;
            process_yield();
        }
        
        if (process_count_running() > 0) {
            // 有进程没有响应，强制关机
            flags |= SHUTDOWN_FLAG_FORCE;
        }
    }
    
    // 保存系统状态
    power_management_save_state();
    
    // 更新统计信息
    power_manager.shutdown_count++;
    power_manager.last_shutdown_time = get_current_time();
    power_manager.shutdown_flags = flags;
    
    // 安全模式下关机
    if (flags & SHUTDOWN_FLAG_SAFE_MODE) {
        vga_print("Entering safe mode shutdown...\n");
        vga_print("Saving essential system data...\n");
        // 执行安全模式关机
        kapi_safe_shutdown();
    } else {
        // 正常关机
        vga_print("Shutting down system...\n");
        vga_print("Thank you for using KenuxOS!\n");
        kapi_poweroff();
    }
    
    return 0;
}

// 执行重启操作
int power_management_reboot(uint32_t flags) {
    if (power_management_check_permission("reboot") != 0) {
        return -1;
    }
    
    // 检查是否强制重启
    if (!(flags & REBOOT_FLAG_FORCE)) {
        // 通知所有进程系统即将重启
        process_broadcast_reboot();
        
        // 等待进程清理
        int timeout = 1000000; // 1秒超时
        while (process_count_running() > 0 && timeout > 0) {
            timeout--;
            process_yield();
        }
        
        if (process_count_running() > 0) {
            // 有进程没有响应，强制重启
            flags |= REBOOT_FLAG_FORCE;
        }
    }
    
    // 保存系统状态
    power_management_save_state();
    
    // 更新统计信息
    power_manager.reboot_count++;
    power_manager.last_reboot_time = get_current_time();
    power_manager.reboot_flags = flags;
    
    // 根据标志选择重启方式
    if (flags & REBOOT_FLAG_SAFE_MODE) {
        vga_print("Entering safe mode reboot...\n");
        kapi_safe_reboot();
    } else if (flags & REBOOT_FLAG_WARM) {
        vga_print("Performing warm reboot...\n");
        kapi_warm_reboot();
    } else if (flags & REBOOT_FLAG_COLD) {
        vga_print("Performing cold reboot...\n");
        kapi_cold_reboot();
    } else {
        // 默认冷重启
        vga_print("Performing cold reboot...\n");
        kapi_cold_reboot();
    }
    
    return 0;
}

// 执行停机操作
int power_management_halt(uint32_t flags) {
    if (power_management_check_permission("halt") != 0) {
        return -1;
    }
    
    // 检查是否强制停机
    if (!(flags & HALT_FLAG_FORCE)) {
        // 通知所有进程系统即将停机
        process_broadcast_halt();
        
        // 等待进程清理
        int timeout = 1000000; // 1秒超时
        while (process_count_running() > 0 && timeout > 0) {
            timeout--;
            process_yield();
        }
        
        if (process_count_running() > 0) {
            // 有进程没有响应，强制停机
            flags |= HALT_FLAG_FORCE;
        }
    }
    
    // 保存系统状态
    if (flags & HALT_FLAG_DUMP_STATE) {
        power_management_save_state();
        vga_print("System state saved to disk.\n");
    }
    
    // 更新统计信息
    power_manager.halt_flags = flags;
    
    // 安全模式下停机
    if (flags & HALT_FLAG_SAFE_MODE) {
        vga_print("Entering safe mode halt...\n");
        kapi_safe_halt();
    } else {
        // 正常停机
        vga_print("System halted.\n");
        vga_print("Press any key to reboot...\n");
        kapi_halt();
    }
    
    return 0;
}

// 获取电源管理统计信息
int power_management_get_stats(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    snprintf(buffer, buffer_size, 
        "Power Management Statistics:\n"
        "  Shutdown count: %u\n"
        "  Reboot count: %u\n"
        "  Last shutdown: %llu\n"
        "  Last reboot: %llu\n"
        "  Current shutdown flags: 0x%08x\n"
        "  Current reboot flags: 0x%08x\n"
        "  Current halt flags: 0x%08x\n",
        power_manager.shutdown_count,
        power_manager.reboot_count,
        power_manager.last_shutdown_time,
        power_manager.last_reboot_time,
        power_manager.shutdown_flags,
        power_manager.reboot_flags,
        power_manager.halt_flags);
    
    return 0;
}

// 设置关机标志
int power_management_set_shutdown_flags(uint32_t flags) {
    power_manager.shutdown_flags = flags;
    return 0;
}

// 设置重启标志
int power_management_set_reboot_flags(uint32_t flags) {
    power_manager.reboot_flags = flags;
    return 0;
}

// 设置停机标志
int power_management_set_halt_flags(uint32_t flags) {
    power_manager.halt_flags = flags;
    return 0;
}

// 延迟关机
int power_management_schedule_shutdown(uint32_t delay_seconds, uint32_t flags) {
    if (delay_seconds == 0) {
        return power_management_shutdown(flags);
    }
    
    // 创建定时器进程
    process_t* timer = process_create("shutdown_timer");
    if (!timer) {
        return -1;
    }
    
    // 设置定时器回调
    timer->timer_callback = (process_timer_callback_t)power_management_shutdown;
    timer->timer_data = &flags;
    timer->timeout = delay_seconds * 1000000ULL; // 转换为微秒
    
    // 启动定时器
    int pid = process_start(timer);
    if (pid < 0) {
        process_destroy(timer);
        return -1;
    }
    
    return 0;
}

// 延迟重启
int power_management_schedule_reboot(uint32_t delay_seconds, uint32_t flags) {
    if (delay_seconds == 0) {
        return power_management_reboot(flags);
    }
    
    // 创建定时器进程
    process_t* timer = process_create("reboot_timer");
    if (!timer) {
        return -1;
    }
    
    // 设置定时器回调
    timer->timer_callback = (process_timer_callback_t)power_management_reboot;
    timer->timer_data = &flags;
    timer->timeout = delay_seconds * 1000000ULL; // 转换为微秒
    
    // 启动定时器
    int pid = process_start(timer);
    if (pid < 0) {
        process_destroy(timer);
        return -1;
    }
    
    return 0;
}

// 取消计划的关机/重启
int power_management_cancel_scheduled(void) {
    // 查找并取消所有定时器进程
    process_cancel_all_timers();
    return 0;
}

// 获取电源状态
const char* power_management_get_status(void) {
    // 检查当前系统状态
    if (process_count_running() == 0) {
        return "System idle";
    } else if (process_count_running() > 0) {
        return "System running";
    } else {
        return "System unknown";
    }
}