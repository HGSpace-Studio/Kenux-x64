#include "../spatiotemporal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

void test_basic_operations() {
    printf("=== 测试基本操作 ===\n");
    
    // 初始化API
    SpatiotemporalAPIStatus status = spatiotemporal_api_init();
    assert(status == STAPI_OK);
    printf("✓ API初始化成功\n");
    
    // 创建数据集
    size_t dataset_id;
    status = spatiotemporal_create_dataset(&dataset_id);
    assert(status == STAPI_OK);
    printf("✓ 创建数据集成功，ID: %zu\n", dataset_id);
    
    // 添加单个数据点
    status = spatiotemporal_add_point(dataset_id, 10.5, 20.3, 0.0, 1640995200, 25.5);
    assert(status == STAPI_OK);
    printf("✓ 添加数据点成功\n");
    
    // 获取数据集信息
    size_t point_count;
    double min_x, max_x, min_y, max_y;
    int64_t min_time, max_time;
    status = spatiotemporal_get_dataset_info(dataset_id, &point_count, &min_x, &max_x, &min_y, &max_y, &min_time, &max_time);
    assert(status == STAPI_OK);
    printf("✓ 获取数据集信息成功\n");
    printf("  数据点数量: %zu\n", point_count);
    printf("  X坐标范围: %.2f - %.2f\n", min_x, max_x);
    printf("  Y坐标范围: %.2f - %.2f\n", min_y, max_y);
    printf("  时间范围: %ld - %ld\n", min_time, max_time);
    
    // 添加更多数据点
    for (int i = 1; i <= 10; i++) {
        double x = 10.0 + i * 2.0;
        double y = 20.0 + i * 1.5;
        long timestamp = 1640995200 + i * 3600;
        double value = 25.0 + sin(i * 0.5) * 5.0;
        
        status = spatiotemporal_add_point(dataset_id, x, y, 0.0, timestamp, value);
        assert(status == STAPI_OK);
    }
    printf("✓ 批量添加数据点成功\n");
    
    // 再次获取数据集信息
    status = spatiotemporal_get_dataset_info(dataset_id, &point_count, &min_x, &max_x, &min_y, &max_y, &min_time, &max_time);
    assert(status == STAPI_OK);
    printf("✓ 更新后的数据集信息:\n");
    printf("  数据点数量: %zu\n", point_count);
    assert(point_count == 11);
    printf("  X坐标范围: %.2f - %.2f\n", min_x, max_x);
    assert(fabs(min_x - 10.5) < 0.001);
    assert(fabs(max_x - 30.0) < 0.001);
    printf("  Y坐标范围: %.2f - %.2f\n", min_y, max_y);
    assert(fabs(min_y - 20.3) < 0.001);
    assert(fabs(max_y - 35.0) < 0.001);
    
    printf("✓ 所有基本操作测试通过\n");
}

void test_dataset_management() {
    printf("\n=== 测试数据集管理 ===\n");
    
    // 创建多个数据集
    size_t dataset1, dataset2;
    spatiotemporal_create_dataset(&dataset1);
    spatiotemporal_create_dataset(&dataset2);
    printf("✓ 创建多个数据集成功，ID: %zu, %zu\n", dataset1, dataset2);
    
    // 向不同数据集添加不同类型的数据
    for (int i = 0; i < 5; i++) {
        spatiotemporal_add_point(dataset1, i * 10.0, i * 5.0, 0.0, 1640995200 + i * 100, i * 2.0);
        spatiotemporal_add_point(dataset2, i * 8.0, i * 3.0, i * 2.0, 1640995200 + i * 200, i * 3.0);
    }
    printf("✓ 向不同数据集添加数据成功\n");
    
    // 验证数据集独立性
    size_t count1, count2;
    double min_x1, min_x2;
    
    spatiotemporal_get_dataset_info(dataset1, &count1, &min_x1, NULL, NULL, NULL, NULL, NULL);
    spatiotemporal_get_dataset_info(dataset2, &count2, &min_x2, NULL, NULL, NULL, NULL, NULL);
    
    assert(count1 == 5 && count2 == 5);
    assert(fabs(min_x1 - 0.0) < 0.001);
    assert(fabs(min_x2 - 0.0) < 0.001);
    printf("✓ 数据集独立性验证通过\n");
    
    printf("✓ 数据集管理测试通过\n");
}

void test_data_bounds() {
    printf("\n=== 测试数据边界 ===\n");
    
    size_t dataset_id;
    spatiotemporal_create_dataset(&dataset_id);
    
    // 添加边界数据点
    spatiotemporal_add_point(dataset_id, -100.0, -50.0, 0.0, 1000000, 0.0);
    spatiotemporal_add_point(dataset_id, 100.0, 50.0, 0.0, 2000000, 100.0);
    spatiotemporal_add_point(dataset_id, 0.0, 0.0, 50.0, 1500000, 50.0);
    
    // 验证边界
    size_t point_count;
    double min_x, max_x, min_y, max_y, min_z, max_z;
    int64_t min_time, max_time;
    
    spatiotemporal_get_dataset_info(dataset_id, &point_count, &min_x, &max_x, &min_y, &max_y, &min_time, &max_time);
    
    assert(fabs(min_x - (-100.0)) < 0.001);
    assert(fabs(max_x - 100.0) < 0.001);
    assert(fabs(min_y - (-50.0)) < 0.001);
    assert(fabs(max_y - 50.0) < 0.001);
    assert(min_time == 1000000);
    assert(max_time == 2000000);
    printf("✓ 边界计算正确\n");
    
    printf("✓ 数据边界测试通过\n");
}

void test_api_cleanup() {
    printf("\n=== 测试API清理 ===\n");
    
    spatiotemporal_api_cleanup();
    printf("✓ API清理成功\n");
    
    // 重新初始化以测试清理效果
    SpatiotemporalAPIStatus status = spatiotemporal_api_init();
    assert(status == STAPI_OK);
    printf("✓ 重新初始化成功\n");
    
    spatiotemporal_api_cleanup();
    printf("✓ 最终清理成功\n");
}

int main() {
    printf("开始Spaitotemporal核心功能测试...\n\n");
    
    test_basic_operations();
    test_dataset_management();
    test_data_bounds();
    test_api_cleanup();
    
    printf("\n🎉 所有核心功能测试通过！\n");
    return 0;
}