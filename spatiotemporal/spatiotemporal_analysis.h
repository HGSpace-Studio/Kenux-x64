#ifndef SPATIOTEMPORAL_ANALYSIS_H
#define SPATIOTEMPORAL_ANALYSIS_H

#include "spatiotemporal_types.h"
#include "spatiotemporal_core.h"
#include <stddef.h>

// 聚类结果结构
typedef struct {
    size_t cluster_count;              // 聚类数量
    size_t* cluster_sizes;            // 每个聚类的大小
    size_t** cluster_point_indices;   // 每个聚类的点索引数组
    double** cluster_centers;         // 每个聚类的中心坐标
} ClusteringResult;

// 趋势分析结果结构
typedef struct {
    double slope;                      // 趋势斜率
    double correlation;                // 相关系数
    double r_squared;                  // R平方值
    timestamp_t trend_start;          // 趋势开始时间
    timestamp_t trend_end;            // 趋势结束时间
} TrendAnalysisResult;

// 相关性分析结果结构
typedef struct {
    double spatial_correlation;       // 空间相关性
    double temporal_correlation;      // 时间相关性
    double spatiotemporal_correlation; // 时空相关性
    double p_value;                   // p值
} CorrelationResult;

// 异常检测结果结构
typedef struct {
    size_t anomaly_count;             // 异常点数量
    size_t* anomaly_indices;          // 异常点索引数组
    double* anomaly_scores;           // 异常分数
    double threshold;                 // 检测阈值
} AnomalyDetectionResult;

// 模式识别结果结构
typedef struct {
    size_t pattern_count;             // 检测到的模式数量
    double* pattern_frequencies;     // 模式频率
    size_t** pattern_point_indices;  // 每个模式的点索引
    size_t pattern_length;           // 模式长度
} PatternRecognitionResult;

// 空间聚类分析
AnalysisResult* spatiotemporal_spatial_clustering(
    const SpatiotemporalManager* manager, 
    size_t dataset_id, 
    size_t max_clusters,
    size_t max_iterations,
    double convergence_threshold
);

// 时间趋势分析
AnalysisResult* spatiotemporal_temporal_trend_analysis(
    const SpatiotemporalManager* manager, 
    size_t dataset_id,
    size_t time_window_size
);

// 时空相关性分析
AnalysisResult* spatiotemporal_spatiotemporal_correlation(
    const SpatiotemporalManager* manager, 
    size_t dataset_id1,
    size_t dataset_id2,
    size_t spatial_lag,
    size_t temporal_lag
);

// 异常检测
AnalysisResult* spatiotemporal_anomaly_detection(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    double threshold,
    const char* method  // "zscore", "isolation", "lof"
);

// 模式识别
AnalysisResult* spatiotemporal_pattern_recognition(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    size_t pattern_length,
    size_t min_support
);

// 释放分析结果
void spatiotemporal_free_analysis_result(AnalysisResult* result);

// K-means聚类辅助函数
size_t spatiotemporal_kmeans_assign_cluster(
    const SpatiotemporalPoint* point,
    const double** centers,
    size_t k
);

bool spatiotemporal_kmeans_update_centers(
    const SpatiotemporalPoint* points,
    size_t point_count,
    const size_t* assignments,
    size_t k,
    double** new_centers
);

// 计算移动平均
void spatiotemporal_moving_average(
    const value_t* values,
    size_t count,
    size_t window_size,
    value_t* result
);

// 计算时间序列的自相关
double spatiotemporal_autocorrelation(
    const value_t* series,
    size_t length,
    size_t lag
);

// 计算空间自相关
double spatiotemporal_spatial_autocorrelation(
    const SpatiotemporalPoint* points,
    size_t point_count,
    size_t spatial_lag
);

#endif // SPATIOTEMPORAL_ANALYSIS_H