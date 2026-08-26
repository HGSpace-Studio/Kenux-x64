// KenuxOS 增强功能综合初始化头文件
#ifndef ENHANCED_FEATURES_H
#define ENHANCED_FEATURES_H

#include <stdint.h>
#include <stddef.h>

// 增强功能模块初始化函数
void init_enhanced_shell_commands(void);
void init_elf_loader(void);
void init_kex_loader(void);
void init_power_management(void);
void init_ui_enhancement(void);
void init_security_enhancement(void);

// 增强功能状态检查
int check_enhanced_features_status(void);

// 增强功能配置
void configure_enhanced_features(void);

// 增强功能启动
void start_enhanced_features(void);

// 增强功能停止
void stop_enhanced_features(void);

// 增强功能版本信息
const char* get_enhanced_features_version(void);

#endif /* ENHANCED_FEATURES_H */