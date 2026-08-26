#include "../spatiotemporal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

void generate_test_data(size_t dataset_id, int data_points) {
    printf("生成测试数据...\n");
    
    for (int i = 0; i < data_points; i++) {
        // 创建带有噪声的正弦波数据
        double x = i * 2.0;
        double y = sin(i * 0.5) + (rand() % 100 - 50) / 100.0; // 添加噪声
        double z = cos(i * 0.3);
        long timestamp = 1640995200 + i * 3600; // 每小时一个点
        double value = 25.0 + y * 10.0; // 基础温度 + 波动
        
        spatiotemporal_add_point(dataset_id, x, y, z, timestamp, value);
    }
    
    printf("已生成 %d 个数据点\n", data_points);
}

void test_clustering() {
    printf("\n=== 测试空间聚类分析 ===\n");
    
    // 创建数据集并生成聚类数据
    size_t dataset_id;
    spatiotemporal_create_dataset(&dataset_id);
    
    // 生成3个聚类的数据
    for (int cluster = 0; cluster < 3; cluster++) {
        double center_x = cluster * 20.0 + 10.0;
        double center_y = cluster * 15.0 + 5.0;
        
        for (int i = 0; i < 20; i++) {
            double x = center_x + (rand() % 100 - 50) / 10.0;
            double y = center_y + (rand() % 100 - 50) / 10.0;
            double z = 0.0;
            long timestamp = 1640995200 + (cluster * 20 + i) * 3600;
            double value = 20.0 + cluster * 5.0 + (rand() % 50 - 25) / 10.0;
            
            spatiotemporal_add_point(dataset_id, x, y, z, timestamp, value);
        }
    }
    
    printf("✓ 生成聚类测试数据完成\n");
    
    // 执行聚类分析
    AnalysisResult* result = NULL;
    SpatiotemporalAPIStatus status = spatiotemporal_cluster_dataset(dataset_id, 3, &result);
    
    assert(status == STAPI_OK);
    assert(result != NULL);
    assert(result->success == true);
    assert(result->type == SPATIAL_CLUSTERING);
    
    printf("✓ 聚类分析执行成功\n");
    printf("  检测到 %zu 个聚类\n", ((struct ClusteringResult*)result->results)->cluster_count);
    
    // 释放结果
    spatiotemporal_free_result(result);
    printf("✓ 聚类结果释放成功\n");
}

void test_trend_analysis() {
    printf("\n=== 测试时间趋势分析 ===\n");
    
    size_t dataset_id;
    spatiotemporal_create_dataset(&dataset_id);
    
    // 生成具有明显趋势的时间序列数据
    for (int i = 0; i < 100; i++) {
        double x = i * 0.1;
        double y = i * 0.05 + sin(i * 0.2); // 上升趋势 + 周期性波动
        double z = 0.0;
        long timestamp = 1640995200 + i * 3600;
        double value = 20.0 + i * 0.2 + sin(i * 0.3) * 2.0; // 上升趋势 + 周期性波动
        
        spatiotemporal_add_point(dataset_id, x, y, z, timestamp, value);
    }
    
    printf("✓ 生成趋势分析测试数据完成\n");
    
    // 执行趋势分析
    AnalysisResult* result = NULL;
    SpatiotemporalAPIStatus status = spatiotemporal_analyze_trends(dataset_id, 5, &result);
    
    assert(status == STAPI_OK);
    assert(result != NULL);
    assert(result->success == true);
    assert(result->type == TEMPORAL_TREND_ANALYSIS);
    
    TrendAnalysisResult* trend = (TrendAnalysisResult*)result->results;
    printf("✓ 趋势分析执行成功\n");
    printf("  趋势斜率: %.4f\n", trend->slope);
    printf("  相关系数: %.4f\n", trend->correlation);
    printf("  R平方值: %.4f\n", trend->r_squared);
    
    // 验证上升趋势（斜率应该为正）
    assert(trend->slope > 0.1); // 应该有明显的上升趋势
    printf("✓ 趋势验证通过（正斜率）\n");
    
    // 释放结果
    spatiotemporal_free_result(result);
    printf("✓ 趋势分析结果释放成功\n");
}

void test_correlation_analysis() {
    printf("\n=== 测试时空相关性分析 ===\n");
    
    // 创建两个相关数据集
    size_t dataset1, dataset2;
    spatiotemporal_create_dataset(&dataset1);
    spatiotemporal_create_dataset(&dataset2);
    
    // 生成相关数据集
    for (int i = 0; i < 50; i++) {
        double x = i * 2.0;
        double base_value = sin(i * 0.1);
        
        // 数据集1：基础值 + 噪声
        double y1 = base_value + (rand() % 100 - 50) / 200.0;
        double z1 = 0.0;
        long timestamp = 1640995200 + i * 3600;
        double value1 = 25.0 + y1 * 10.0;
        
        spatiotemporal_add_point(dataset1, x, y1, z1, timestamp, value1);
        
        // 数据集2：与数据集1相关但有延迟
        double y2 = base_value * 0.8 + (rand() % 100 - 50) / 200.0;
        double z2 = 0.0;
        double value2 = 20.0 + y2 * 8.0;
        
        spatiotemporal_add_point(dataset2, x + 10.0, y2, z2, timestamp + 3600, value2);
    }
    
    printf("✓ 生成相关性分析测试数据完成\n");
    
    // 执行相关性分析
    AnalysisResult* result = NULL;
    SpatiotemporalAPIStatus status = spatiotemporal_analyze_correlation(dataset1, dataset2, &result);
    
    assert(status == STAPI_OK);
    assert(result != NULL);
    assert(result->success == true);
    assert(result->type == SPATIOTEMPORAL_CORRELATION);
    
    CorrelationResult* corr = (CorrelationResult*)result->results;
    printf("✓ 相关性分析执行成功\n");
    printf("  空间相关性: %.4f\n", corr->spatial_correlation);
    printf("  时间相关性: %.4f\n", corr->temporal_correlation);
    printf("  时空相关性: %.4f\n", corr->spatiotemporal_correlation);
    
    // 验证存在正相关性
    assert(corr->spatiotemporal_correlation > 0.1); // 应该有一定的相关性
    printf("✓ 相关性验证通过（正相关性）\n");
    
    // 释放结果
    spatiotemporal_free_result(result);
    printf("✓ 相关性分析结果释放成功\n");
}

void test_anomaly_detection() {
    printf("\n=== 测试异常检测 ===\n");
    
    size_t dataset_id;
    spatiotemporal_create_dataset(&dataset_id);
    
    // 生成正常数据和异常点
    for (int i = 0; i < 100; i++) {
        double x = i * 0.5;
        double y = sin(i * 0.1) + (rand() % 50 - 25) / 50.0; // 正常波动
        double z = 0.0;
        long timestamp = 1640995200 + i * 3600;
        double value = 25.0 + y * 5.0; // 正常值
        
        // 在几个位置添加异常点
        if (i == 20 || i == 45 || i == 78) {
            value += 15.0; // 显著偏离正常值的异常点
        }
        
        spatiotemporal_add_point(dataset_id, x, y, z, timestamp, value);
    }
    
    printf("✓ 生成异常检测测试数据完成\n");
    
    // 执行异常检测
    AnalysisResult* result = NULL;
    SpatiotemporalAPIStatus status = spatiotemporal_detect_anomalies(dataset_id, 2.0, &result);
    
    assert(status == STAPI_OK);
    assert(result != NULL);
    assert(result->success == true);
    assert(result->type == ANOMALY_DETECTION);
    
    AnomalyDetectionResult* anomaly = (AnomalyDetectionResult*)result->results;
    printf("✓ 异常检测执行成功\n");
    printf("  检测到 %zu 个异常点\n", anomaly->anomaly_count);
    printf("  检测阈值: %.2f\n", anomaly->threshold);
    
    // 验证检测到异常点（应该检测到我们设置的3个异常点）
    assert(anomaly->anomaly_count >= 2); // 可能由于噪声，检测到2-3个异常点
    printf("✓ 异常数量验证通过\n");
    
    // 打印异常点信息
    for (size_t i = 0; i < anomaly->anomaly_count; i++) {
        printf("  异常点 %zu: 索引=%zu, 异常分数=%.2f\n", 
               i + 1, anomaly->anomaly_indices[i], anomaly->anomaly_scores[i]);
    }
    
    // 释放结果
    spatiotemporal_free_result(result);
    printf("✓ 异常检测结果释放成功\n");
}

void test_visualization_export() {
    printf("\n=== 测试可视化导出 ===\n");
    
    size_t dataset_id;
    spatiotemporal_create_dataset(&dataset_id);
    
    // 生成简单的测试数据
    for (int i = 0; i < 20; i++) {
        double x = i * 5.0;
        double y = i * 3.0;
        double z = 0.0;
        long timestamp = 1640995200 + i * 3600;
        double value = 20.0 + sin(i * 0.5) * 5.0;
        
        spatiotemporal_add_point(dataset_id, x, y, z, timestamp, value);
    }
    
    printf("✓ 生成可视化测试数据完成\n");
    
    // 测试导出数据
    SpatiotemporalAPIStatus status = spatiotemporal_export_data(dataset_id, "test_data.json");
    assert(status == STAPI_OK);
    printf("✓ 数据导出成功（test_data.json）\n");
    
    // 测试2D散点图生成
    status = spatiotemporal_plot_2d_scatter(dataset_id, "test_scatter.bmp", 800, 600);
    assert(status == STAPI_OK);
    printf("✓ 2D散点图生成成功（test_scatter.bmp）\n");
    
    // 测试热力图生成
    status = spatiotemporal_plot_heatmap(dataset_id, "test_heatmap.bmp", 400, 300);
    assert(status == STAPI_OK);
    printf("✓ 热力图生成成功（test_heatmap.bmp）\n");
    
    // 测试时间序列图生成
    status = spatiotemporal_plot_time_series(dataset_id, "test_timeseries.bmp", 800, 400);
    assert(status == STAPI_OK);
    printf("✓ 时间序列图生成成功（test_timeseries.bmp）\n");
    
    printf("✓ 所有可视化测试完成\n");
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
    printf("开始Spaitotemporal分析功能测试...\n\n");
    
    test_clustering();
    test_trend_analysis();
    test_correlation_analysis();
    test_anomaly_detection();
    test_visualization_export();
    test_api_cleanup();
    
    printf("\n🎉 所有分析功能测试通过！\n");
    return 0;
}