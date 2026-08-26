#include <kex_format.h>
#include <arch/elf.h>
#include <arch/memory.h>
#include <arch/process.h>
#include <arch/fs.h>
#include <string.h>
#include <slab.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>

#include "kex_loader.h"

// Kex程序加载器增强功能
typedef struct {
    kex_header_t header;
    uint32_t load_flags;
    uint8_t* code_base;
    uint8_t* data_base;
    uint8_t* heap_base;
    uint8_t* stack_base;
    uint64_t entry_point;
    uint64_t program_info;
    uint32_t memory_usage;
} kex_loader_info_t;

// 加载标志
#define KEX_LOAD_HEAP_ALLOC    (1 << 0)
#define KEX_LOAD_DATA_MAPPED    (1 << 1)
#define KEX_LOAD_RELRO_ENABLED  (1 << 2)
#define KEX_LOAD_CANARY_ENABLED (1 << 3)

// Kex安装系统
typedef struct {
    char install_path[256];
    char install_name[64];
    uint32_t install_version;
    uint32_t install_size;
    uint32_t install_flags;
    char dependencies[256];
} kex_install_info_t;

// 初始化Kex加载器
int kex_loader_init(void) {
    // 初始化必要的子系统
    memory_init_allocator();
    security_init();
    return 0;
}

// 加载Kex文件的头部信息
int kex_load_header(const char* path, kex_loader_info_t* info) {
    if (!path || !info) return -1;
    
    int fd = fs_open_file(path, "r");
    if (fd < 0) return -1;
    
    // 读取Kex头部
    if (fs_read_file_data(fd, &info->header, sizeof(kex_header_t)) != sizeof(kex_header_t)) {
        fs_close_file(fd);
        return -1;
    }
    
    // 验证Kex文件
    if (info->header.magic[0] != KEX_MAGIC0 ||
        info->header.magic[1] != KEX_MAGIC1 ||
        info->header.magic[2] != KEX_MAGIC2 ||
        info->header.magic[3] != KEX_MAGIC3) {
        fs_close_file(fd);
        return -1;
    }
    
    // 验证版本
    if (info->header.version != KEX_VERSION) {
        fs_close_file(fd);
        return -1;
    }
    
    // 验证头部大小
    if (info->header.header_size != KEX_HEADER_SIZE) {
        fs_close_file(fd);
        return -1;
    }
    
    fs_close_file(fd);
    return 0;
}

// 加载Kex程序的代码段
int kex_load_code_segment(const char* path, kex_loader_info_t* info) {
    if (!path || !info) return -1;
    
    int fd = fs_open_file(path, "r");
    if (fd < 0) return -1;
    
    // 定位到代码段
    fs_seek(fd, KEX_CODE_REGION_OFFSET, SEEK_SET);
    
    // 读取代码段
    size_t code_size = info->header.code_size;
    info->code_base = kzalloc(code_size);
    if (!info->code_base) {
        fs_close_file(fd);
        return -1;
    }
    
    if (fs_read_file_data(fd, info->code_base, code_size) != code_size) {
        kfree(info->code_base);
        fs_close_file(fd);
        return -1;
    }
    
    // 设置入口点
    info->entry_point = (uint64_t)info->code_base;
    
    fs_close_file(fd);
    return 0;
}

// 加载Kex程序的.rodata段
int kex_load_rodata_segment(const char* path, kex_loader_info_t* info) {
    if (!path || !info) return -1;
    
    if (info->header.rodata_offset == 0) {
        return 0; // 没有.rodata段
    }
    
    int fd = fs_open_file(path, "r");
    if (fd < 0) return -1;
    
    // 定位到.rodata段
    fs_seek(fd, info->header.rodata_offset, SEEK_SET);
    
    // 读取.rodata段
    size_t rodata_size = info->header.rodata_size;
    info->data_base = kzalloc(rodata_size);
    if (!info->data_base) {
        fs_close_file(fd);
        return -1;
    }
    
    if (fs_read_file_data(fd, info->data_base, rodata_size) != rodata_size) {
        kfree(info->data_base);
        fs_close_file(fd);
        return -1;
    }
    
    fs_close_file(fd);
    return 0;
}

// 应用本地重定位表
int kex_apply_local_relocations(kex_loader_info_t* info) {
    if (!info) return -1;
    
    if (info->header.local_reloc_off == 0) {
        return 0; // 没有本地重定位
    }
    
    // 读取本地重定位表
    uint32_t local_reloc_size = info->header.local_reloc_cnt * sizeof(kex_local_reloc_t);
    kex_local_reloc_t* local_relocs = kzalloc(local_reloc_size);
    if (!local_relocs) {
        return -1;
    }
    
    // TODO: 实现本地重定位的修补逻辑
    // 这里需要读取文件中的本地重定位表并应用修补
    
    kfree(local_relocs);
    return 0;
}

// 设置Kex程序的内存布局
int kex_setup_memory_layout(kex_loader_info_t* info) {
    if (!info) return -1;
    
    // 设置堆
    info->heap_base = kzalloc(info->header.heap_size);
    if (!info->heap_base) {
        return -1;
    }
    
    // 设置栈
    info->stack_base = kzalloc(info->header.stack_size);
    if (!info->stack_base) {
        kfree(info->heap_base);
        return -1;
    }
    
    // 计算内存使用量
    info->memory_usage = info->header.code_size + 
                         info->header.rodata_size + 
                         info->header.heap_size + 
                         info->header.stack_size;
    
    return 0;
}

// 运行Kex程序
int kex_run_program(const char* path, const char* argv[], const char* envp[]) {
    if (!path) return -1;
    
    kex_loader_info_t info;
    memset(&info, 0, sizeof(info));
    
    // 加载头部
    if (kex_load_header(path, &info) != 0) {
        return -1;
    }
    
    // 加载代码段
    if (kex_load_code_segment(path, &info) != 0) {
        return -1;
    }
    
    // 加载.rodata段
    if (kex_load_rodata_segment(path, &info) != 0) {
        kfree(info.code_base);
        return -1;
    }
    
    // 应用本地重定位
    if (kex_apply_local_relocations(&info) != 0) {
        kfree(info.code_base);
        kfree(info.data_base);
        return -1;
    }
    
    // 设置内存布局
    if (kex_setup_memory_layout(&info) != 0) {
        kfree(info.code_base);
        kfree(info.data_base);
        return -1;
    }
    
    // 创建新进程
    process_t* proc = process_create(path);
    if (!proc) {
        kfree(info.code_base);
        kfree(info.data_base);
        kfree(info.heap_base);
        kfree(info.stack_base);
        return -1;
    }
    
    // 设置进程的Kex信息
    proc->entry_point = info.entry_point;
    proc->stack_top = (uint64_t)info.stack_base + info.header.stack_size;
    
    // 启动进程
    int pid = process_start(proc);
    if (pid < 0) {
        process_destroy(proc);
        kfree(info.code_base);
        kfree(info.data_base);
        kfree(info.heap_base);
        kfree(info.stack_base);
        return -1;
    }
    
    // 清理内存
    kfree(info.code_base);
    kfree(info.data_base);
    kfree(info.heap_base);
    kfree(info.stack_base);
    
    return pid;
}

// Kex安装系统
int kex_install_program(const char* source_path, const char* target_path) {
    if (!source_path || !target_path) return -1;
    
    // 检查源文件是否存在
    if (fs_file_exists(source_path) == 0) {
        return -1;
    }
    
    // 验证Kex文件
    kex_loader_info_t info;
    if (kex_load_header(source_path, &info) != 0) {
        return -1;
    }
    
    // 创建目标目录
    char target_dir[256];
    strncpy(target_dir, target_path, sizeof(target_dir));
    char* last_slash = strrchr(target_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        fs_create_directory(target_dir);
    }
    
    // 复制文件
    if (fs_copy_file(source_path, target_path) != 0) {
        return -1;
    }
    
    // 设置权限
    fs_set_permissions(target_path, 0755);
    
    return 0;
}

// Kex程序管理
int kex_uninstall_program(const char* path) {
    if (!path) return -1;
    
    // 删除文件
    if (fs_remove_file(path) != 0) {
        return -1;
    }
    
    return 0;
}

// 列出已安装的Kex程序
int kex_list_installed_programs(char* buffer, size_t buffer_size) {
    if (!buffer) return -1;
    
    snprintf(buffer, buffer_size, "Installed Kex Programs:\n");
    buffer += strlen(buffer);
    
    // 扫描/System/kex目录
    char path[256];
    snprintf(path, sizeof(path), "/System/kex/");
    
    // TODO: 实现目录扫描和程序信息获取
    strcat(buffer, "  hello.kex - Hello World program\n");
    strcat(buffer, "  sysinfo.kex - System information tool\n");
    
    return 0;
}

// Kex程序更新
int kex_update_program(const char* source_path, const char* target_path) {
    if (!source_path || !target_path) return -1;
    
    // 先卸载
    if (kex_uninstall_program(target_path) != 0) {
        return -1;
    }
    
    // 再安装
    return kex_install_program(source_path, target_path);
}