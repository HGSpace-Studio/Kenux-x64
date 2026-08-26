#include "text_editor.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// 前向声明spatiotemporal API函数
#ifdef SPATIOTEMPORAL_API_AVAILABLE
extern int spatiotemporal_api_init();
extern void spatiotemporal_api_cleanup();
extern int spatiotemporal_create_dataset(unsigned int* dataset_id);
extern int spatiotemporal_add_point(unsigned int dataset_id, double x, double y, double z, long timestamp, double value);
extern int spatiotemporal_plot_2d_scatter(unsigned int dataset_id, const char* output_path, int width, int height);
extern int spatiotemporal_plot_heatmap(unsigned int dataset_id, const char* output_path, int width, int height);
extern int spatiotemporal_plot_time_series(unsigned int dataset_id, const char* output_path, int width, int height);
extern int spatiotemporal_export_data(unsigned int dataset_id, const char* output_path);
extern void spatiotemporal_free_result(void* result);

// 生成示例数据
void generate_sample_data(unsigned int dataset_id) {
    printf("生成示例数据...\n");
    
    // 生成温度数据（随时间和空间变化）
    srand(time(NULL));
    for (int day = 0; day < 30; day++) {
        for (int hour = 0; hour < 24; hour++) {
            double x = day * 10.0;  // X坐标（天）
            double y = hour * 1.0;   // Y坐标（小时）
            double z = 0.0;         // Z坐标（固定为0）
            
            // 模拟温度变化：基础温度 + 随机波动 + 周期性变化
            double base_temp = 20.0 + 10.0 * sin(day * 0.1);  // 周期性变化
            double noise = (double)(rand() % 100 - 50) / 10.0;  // 随机噪声
            double temp = base_temp + noise + (hour - 12) * 0.2;  // 日变化
            
            long timestamp = time(NULL) + (day * 24 + hour) * 3600;
            
            spatiotemporal_add_point(dataset_id, x, y, z, timestamp, temp);
        }
    }
    
    printf("已生成 %d 个数据点\n", 30 * 24);
}

// 可视化菜单
void visualization_menu(unsigned int dataset_id) {
    int choice;
    char filename[256];
    int width = 800, height = 600;
    
    while (1) {
        printf("\n=== 可视化选项 ===\n");
        printf("1. 生成2D散点图\n");
        printf("2. 生成热力图\n");
        printf("3. 生成时间序列图\n");
        printf("4. 导出数据为JSON\n");
        printf("5. 返回主菜单\n");
        printf("请选择: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请重新输入\n");
            while (getchar() != '\n'); // 清除输入缓冲区
            continue;
        }
        
        switch (choice) {
            case 1:
                printf("输入输出文件名 (如: scatter.bmp): ");
                scanf("%255s", filename);
                if (spatiotemporal_plot_2d_scatter(dataset_id, filename, width, height) == 0) {
                    printf("2D散点图已生成: %s\n", filename);
                } else {
                    printf("生成散点图失败\n");
                }
                break;
                
            case 2:
                printf("输入输出文件名 (如: heatmap.bmp): ");
                scanf("%255s", filename);
                if (spatiotemporal_plot_heatmap(dataset_id, filename, width, height) == 0) {
                    printf("热力图已生成: %s\n", filename);
                } else {
                    printf("生成热力图失败\n");
                }
                break;
                
            case 3:
                printf("输入输出文件名 (如: timeseries.bmp): ");
                scanf("%255s", filename);
                if (spatiotemporal_plot_time_series(dataset_id, filename, width, height) == 0) {
                    printf("时间序列图已生成: %s\n", filename);
                } else {
                    printf("生成时间序列图失败\n");
                }
                break;
                
            case 4:
                printf("输入输出文件名 (如: data.json): ");
                scanf("%255s", filename);
                if (spatiotemporal_export_data(dataset_id, filename) == 0) {
                    printf("数据已导出: %s\n", filename);
                } else {
                    printf("导出数据失败\n");
                }
                break;
                
            case 5:
                return;
                
            default:
                printf("无效选择，请重新输入\n");
        }
    }
}

// 主菜单
void spatiotemporal_main_menu() {
    unsigned int dataset_id = 0;
    int choice;
    
    // 初始化API
    if (spatiotemporal_api_init() != 0) {
        printf("初始化Spaitotemporal API失败\n");
        return;
    }
    
    // 创建数据集
    if (spatiotemporal_create_dataset(&dataset_id) != 0) {
        printf("创建数据集失败\n");
        spatiotemporal_api_cleanup();
        return;
    }
    
    printf("Spaitotemporal Composability 工具\n");
    printf("数据集ID: %u\n", dataset_id);
    
    while (1) {
        printf("\n=== 主菜单 ===\n");
        printf("1. 添加数据点\n");
        printf("2. 生成示例数据\n");
        printf("3. 可视化选项\n");
        printf("4. 查看数据集信息\n");
        printf("5. 退出\n");
        printf("请选择: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请重新输入\n");
            while (getchar() != '\n'); // 清除输入缓冲区
            continue;
        }
        
        switch (choice) {
            case 1: {
                double x, y, z, value;
                long timestamp;
                
                printf("输入X坐标: ");
                scanf("%lf", &x);
                printf("输入Y坐标: ");
                scanf("%lf", &y);
                printf("输入Z坐标: ");
                scanf("%lf", &z);
                printf("输入时间戳 (Unix时间): ");
                scanf("%ld", &timestamp);
                printf("输入值: ");
                scanf("%lf", &value);
                
                if (spatiotemporal_add_point(dataset_id, x, y, z, timestamp, value) == 0) {
                    printf("数据点添加成功\n");
                } else {
                    printf("添加数据点失败\n");
                }
                break;
            }
                
            case 2:
                generate_sample_data(dataset_id);
                break;
                
            case 3:
                visualization_menu(dataset_id);
                break;
                
            case 4: {
                unsigned int point_count;
                double min_x, max_x, min_y, max_y;
                long min_time, max_time;
                
                if (spatiotemporal_get_dataset_info(dataset_id, &point_count, &min_x, &max_x, &min_y, &max_y, &min_time, &max_time) == 0) {
                    printf("数据集信息:\n");
                    printf("  数据点数量: %u\n", point_count);
                    printf("  X坐标范围: %.2f - %.2f\n", min_x, max_x);
                    printf("  Y坐标范围: %.2f - %.2f\n", min_y, max_y);
                    printf("  时间范围: %ld - %ld\n", min_time, max_time);
                } else {
                    printf("获取数据集信息失败\n");
                }
                break;
            }
                
            case 5:
                spatiotemporal_api_cleanup();
                printf("Spaitotemporal Composability 工具已退出\n");
                return;
                
            default:
                printf("无效选择，请重新输入\n");
        }
    }
}

#endif // SPATIOTEMPORAL_API_AVAILABLE

// 应用程序入口点
void spatiotemporal_tool_main() {
    #ifdef SPATIOTEMPORAL_API_AVAILABLE
    printf("正在启动 Spaitotemporal Composability 工具...\n");
    spatiotemporal_main_menu();
    #else
    printf("Spaitotemporal Composability 功能未编译，无法使用\n");
    #endif
}