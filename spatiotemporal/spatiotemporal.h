#ifndef SPATIOTEMPORAL_H
#define SPATIOTEMPORAL_H

// 统一的Spaitotemporal Composability API头文件
// 包含所有功能的声明

// 包含核心头文件
#include "spatiotemporal_types.h"
#include "spatiotemporal_core.h"
#include "spatiotemporal_analysis.h"
#include "spatiotemporal_visualization.h"

// 创建API状态
typedef enum {
    STAPI_OK = 0,                    // 成功
    STAPI_ERROR_INIT,                // 初始化错误
    STAPI_ERROR_MEMORY,              // 内存错误
    STAPI_ERROR_INVALID_PARAM,       // 无效参数
    STAPI_ERROR_NOT_FOUND,           // 未找到
    STAPI_ERROR_BOUNDS,              // 边界错误
    STAPI_ERROR_ANALYSIS,            // 分析错误
    STAPI_ERROR_VISUALIZATION        // 可视化错误
} SpatiotemporalAPIStatus;

// 简化的初始化函数
SpatiotemporalAPIStatus spatiotemporal_api_init();

// 简化的清理函数
void spatiotemporal_api_cleanup();

// 创建数据集并返回ID
SpatiotemporalAPIStatus spatiotemporal_create_dataset(size_t* dataset_id);

// 添加数据点到数据集
SpatiotemporalAPIStatus spatiotemporal_add_point(size_t dataset_id, 
                                                double x, double y, double z,
                                                int64_t timestamp, 
                                                double value);

// 执行空间聚类分析
SpatiotemporalAPIStatus spatiotemporal_cluster_dataset(size_t dataset_id,
                                                       size_t max_clusters,
                                                       AnalysisResult** result);

// 执行时间趋势分析
SpatiotemporalAPIStatus spatiotemporal_analyze_trends(size_t dataset_id,
                                                      size_t window_size,
                                                      AnalysisResult** result);

// 执行时空相关性分析
SpatiotemporalAPIStatus spatiotemporal_analyze_correlation(size_t dataset_id1,
                                                           size_t dataset_id2,
                                                           AnalysisResult** result);

// 执行异常检测
SpatiotemporalAPIStatus spatiotemporal_detect_anomalies(size_t dataset_id,
                                                        double threshold,
                                                        AnalysisResult** result);

// 生成2D散点图
SpatiotemporalAPIStatus spatiotemporal_plot_2d_scatter(size_t dataset_id,
                                                        const char* output_path,
                                                        int width,
                                                        int height);

// 生成热力图
SpatiotemporalAPIStatus spatiotemporal_plot_heatmap(size_t dataset_id,
                                                     const char* output_path,
                                                     int width,
                                                     int height);

// 生成时间序列图
SpatiotemporalAPIStatus spatiotemporal_plot_time_series(size_t dataset_id,
                                                        const char* output_path,
                                                        int width,
                                                        int height);

// 导出数据为JSON
SpatiotemporalAPIStatus spatiotemporal_export_data(size_t dataset_id,
                                                   const char* output_path);

// 获取数据集信息
SpatiotemporalAPIStatus spatiotemporal_get_dataset_info(size_t dataset_id,
                                                        size_t* point_count,
                                                        double* min_coord_x,
                                                        double* max_coord_x,
                                                        double* min_coord_y,
                                                        double* max_coord_y,
                                                        int64_t* min_time,
                                                        int64_t* max_time);

// 释放分析结果
void spatiotemporal_free_result(AnalysisResult* result);

#endif // SPATIOTEMPORAL_H