#ifndef KEX_LOADER_ENHANCED_H
#define KEX_LOADER_ENHANCED_H

#include <kex_format.h>

// Kex加载器增强功能定义
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

// Kex安装系统结构体
typedef struct {
    char install_path[256];
    char install_name[64];
    uint32_t install_version;
    uint32_t install_size;
    uint32_t install_flags;
    char dependencies[256];
} kex_install_info_t;

// 函数声明
int kex_loader_init(void);
int kex_load_header(const char* path, kex_loader_info_t* info);
int kex_load_code_segment(const char* path, kex_loader_info_t* info);
int kex_load_rodata_segment(const char* path, kex_loader_info_t* info);
int kex_apply_local_relocations(kex_loader_info_t* info);
int kex_setup_memory_layout(kex_loader_info_t* info);
int kex_run_program(const char* path, const char* argv[], const char* envp[]);
int kex_install_program(const char* source_path, const char* target_path);
int kex_uninstall_program(const char* path);
int kex_list_installed_programs(char* buffer, size_t buffer_size);
int kex_update_program(const char* source_path, const char* target_path);

// Kex程序管理命令
int kex_manage_shell_commands(char** args, int arg_count);

#endif /* KEX_LOADER_ENHANCED_H */