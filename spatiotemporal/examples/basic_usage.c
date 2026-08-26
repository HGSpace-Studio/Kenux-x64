#include "../spatiotemporal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

int main() {
    printf("Spaitotemporal Composability 基本使用示例\n");
    printf("=============================================\n\n");
    
    // 1. 初始化API
    printf("1. 初始化Spaitotemporal API...\n");
    SpatiotemporalAPIStatus status = spatiotemporal_api_init();
    if (status != STAPI_OK) {
        printf("❌ API初始化失败，错误码: %d\n", status);
        return 1;
    }
    printf("✅ API初始化成功\n\n");
    
    // 2. 创建数据集
    printf("2. 创建时空数据集...\n");
    size_t temperature_dataset_id;
    status = spatiotemporal_create_dataset(&temperature_dataset_id);
    if (status != STAPI_OK) {
        printf("❌ 创建数据集失败，错误码: %d\n", status);
        return 1;
    }
    printf("✅ 数据集创建成功，ID: %zu\n\n", temperature_dataset_id);
    
    // 3. 添加示例数据（模拟城市温度监测）
    printf("3. 添加示例数据（城市温度监测）...\n");
    srand(time(NULL));
    
    // 模拟30天的温度数据
    for (int day = 0; day < 30; day++) {
        for (int hour = 0; hour < 24; hour++) {
            // 位置坐标（模拟城市的不同区域）
            double x = day * 10.0;  // X: 天数
            double y = hour * 1.0;   // Y: 小时
            double z = 0.0;         // Z: 固定高度
            
            // 时间戳
            long timestamp = time(NULL) + (day * 24 + hour) * 3600;
            
            // 温度计算：基础温度 + 时间变化 + 随机波动
            double base_temp = 20.0;               // 基础温度
            double daily_variation = 5.0 * sin(day * 0.2);  // 每日变化
            double hourly_variation = (hour - 12) * 0.4;    // 小时变化
            double random_noise = (rand() % 100 - 50) / 10.0; // 随机噪声
            double temperature = base_temp + daily_variation + hourly_variation + random_noise;
            
            // 添加数据点
            status = spatiotemporal_add_point(temperature_dataset_id, x, y, z, timestamp, temperature);
            if (status != STAPI_OK) {
                printf("❌ 添加数据点失败，错误码: %d\n", status);
                return 1;
            }
        }
    }
    printf("✅ 已添加 %d 个温度数据点\n\n", 30 * 24);
    
    // 4. 获取数据集信息
    printf("4. 获取数据集信息...\n");
    size_t point_count;
    double min_x, max_x, min_y, max_y;
    int64_t min_time, max_time;
    
    status = spatiotemporal_get_dataset_info(temperature_dataset_id, &point_count, 
                                            &min_x, &max_x, &min_y, &max_y, 
                                            &min_time, &max_time);
    if (status != STAPI_OK) {
        printf("❌ 获取数据集信息失败，错误码: %d\n", status);
        return 1;
    }
    
    printf("✅ 数据集信息:\n");
    printf("   数据点数量: %zu\n", point_count);
    printf("   X坐标范围: %.2f - %.2f\n", min_x, max_x);
    printf("   Y坐标范围: %.2f - %.2f\n", min_y, max_y);
    printf("   时间范围: %ld - %ld\n", min_time, max_time);
    printf("   时间跨度: %.2f 小时\n\n", (max_time - min_time) / 3600.0);
    
    // 5. 导出数据
    printf("5. 导出数据为JSON格式...\n");
    status = spatiotemporal_export_data(temperature_dataset_id, "temperature_data.json");
    if (status != STAPI_OK) {
        printf("❌ 数据导出失败，错误码: %d\n", status);
        return 1;
    }
    printf("✅ 数据已导出到 temperature_data.json\n\n");
    
    // 6. 生成可视化图表
    printf("6. 生成可视化图表...\n");
    
    // 2D散点图
    status = spatiotemporal_plot_2d_scatter(temperature_dataset_id, "temperature_scatter.bmp", 800, 600);
    if (status == STAPI_OK) {
        printf("✅ 2D散点图已生成: temperature_scatter.bmp\n");
    } else {
        printf("⚠️  2D散点图生成失败，错误码: %d\n", status);
    }
    
    // 热力图
    status = spatiotemporal_plot_heatmap(temperature_dataset_id, "temperature_heatmap.bmp", 600, 400);
    if (status == STAPI_OK) {
        printf("✅ 热力图已生成: temperature_heatmap.bmp\n");
    } else {
        printf("⚠️  热力图生成失败，错误码: %d\n", status);
    }
    
    // 时间序列图
    status = spatiotemporal_plot_time_series(temperature_dataset_id, "temperature_timeseries.bmp", 800, 400);
    if (status == STAPI_OK) {
        printf("✅ 时间序列图已生成: temperature_timeseries.bmp\n");
    } else {
        printf("⚠️  时间序列图生成失败，错误码: %d\n", status);
    }
    
    printf("\n");
    
    // 7. 清理资源
    printf("7. 清理资源...\n");
    spatiotemporal_api_cleanup();
    printf("✅ 资源清理完成\n\n");
    
    printf("🎉 Spaitotemporal Composability 基本使用示例完成！\n");
    printf("生成的文件:\n");
    printf("  - temperature_data.json: 导出的数据\n");
    printf("  - temperature_scatter.bmp: 2D散点图\n");
    printf("  - temperature_heatmap.bmp: 热力图\n");
    printf("  - temperature_timeseries.bmp: 时间序列图\n");
    
    return 0;
}