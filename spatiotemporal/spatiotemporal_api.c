#include "spatiotemporal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 全局管理器实例
static SpatiotemporalManager g_st_manager = {0};
static bool g_initialized = false;

// 初始化API
SpatiotemporalAPIStatus spatiotemporal_api_init() {
    if (g_initialized) {
        return STAPI_OK;
    }

    // 初始化核心管理器
    SpatiotemporalStatus core_status = spatiotemporal_manager_init(&g_st_manager, 10);
    if (core_status != SPATIOTEMPORAL_SUCCESS) {
        return STAPI_ERROR_INIT;
    }

    // 初始化可视化系统
    int viz_status = spatiotemporal_visualization_init();
    if (viz_status != 0) {
        spatiotemporal_manager_cleanup(&g_st_manager);
        return STAPI_ERROR_INIT;
    }

    g_initialized = true;
    return STAPI_OK;
}

// 清理API
void spatiotemporal_api_cleanup() {
    if (!g_initialized) {
        return;
    }

    spatiotemporal_visualization_cleanup();
    spatiotemporal_manager_cleanup(&g_st_manager);
    g_initialized = false;
}

// 创建数据集
SpatiotemporalAPIStatus spatiotemporal_create_dataset(size_t* dataset_id) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    if (!dataset_id) {
        return STAPI_ERROR_INVALID_PARAM;
    }

    SpatiotemporalStatus status = spatiotemporal_manager_create_dataset(&g_st_manager, dataset_id);
    if (status != SPATIOTEMPORAL_SUCCESS) {
        return STAPI_ERROR_MEMORY;
    }

    return STAPI_OK;
}

// 添加数据点到数据集
SpatiotemporalAPIStatus spatiotemporal_add_point(size_t dataset_id, 
                                                double x, double y, double z,
                                                int64_t timestamp, 
                                                double value) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    SpatiotemporalPoint point;
    point.coordinates[0] = x;
    point.coordinates[1] = y;
    point.coordinates[2] = z;
    point.timestamp = timestamp;
    point.value = value;

    SpatiotemporalStatus status = spatiotemporal_dataset_add_point(&g_st_manager, dataset_id, &point);
    if (status != SPATIOTEMPORAL_SUCCESS) {
        return STAPI_ERROR_INVALID_PARAM;
    }

    return STAPI_OK;
}

// 执行空间聚类分析
SpatiotemporalAPIStatus spatiotemporal_cluster_dataset(size_t dataset_id,
                                                       size_t max_clusters,
                                                       AnalysisResult** result) {
    if (!g_initialized || !result) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count) {
        return STAPI_ERROR_NOT_FOUND;
    }

    AnalysisResult* cluster_result = spatiotemporal_spatial_clustering(
        &g_st_manager, dataset_id, max_clusters, 100, 1e-6);
    
    if (!cluster_result || !cluster_result->success) {
        if (cluster_result) {
            spatiotemporal_free_analysis_result(cluster_result);
        }
        return STAPI_ERROR_ANALYSIS;
    }

    *result = cluster_result;
    return STAPI_OK;
}

// 执行时间趋势分析
SpatiotemporalAPIStatus spatiotemporal_analyze_trends(size_t dataset_id,
                                                      size_t window_size,
                                                      AnalysisResult** result) {
    if (!g_initialized || !result) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count) {
        return STAPI_ERROR_NOT_FOUND;
    }

    AnalysisResult* trend_result = spatiotemporal_temporal_trend_analysis(
        &g_st_manager, dataset_id, window_size);
    
    if (!trend_result || !trend_result->success) {
        if (trend_result) {
            spatiotemporal_free_analysis_result(trend_result);
        }
        return STAPI_ERROR_ANALYSIS;
    }

    *result = trend_result;
    return STAPI_OK;
}

// 执行时空相关性分析
SpatiotemporalAPIStatus spatiotemporal_analyze_correlation(size_t dataset_id1,
                                                           size_t dataset_id2,
                                                           AnalysisResult** result) {
    if (!g_initialized || !result) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id1 >= g_st_manager.dataset_count || dataset_id2 >= g_st_manager.dataset_count) {
        return STAPI_ERROR_NOT_FOUND;
    }

    AnalysisResult* corr_result = spatiotemporal_spatiotemporal_correlation(
        &g_st_manager, dataset_id1, dataset_id2, 1, 1);
    
    if (!corr_result || !corr_result->success) {
        if (corr_result) {
            spatiotemporal_free_analysis_result(corr_result);
        }
        return STAPI_ERROR_ANALYSIS;
    }

    *result = corr_result;
    return STAPI_OK;
}

// 执行异常检测
SpatiotemporalAPIStatus spatiotemporal_detect_anomalies(size_t dataset_id,
                                                        double threshold,
                                                        AnalysisResult** result) {
    if (!g_initialized || !result) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count) {
        return STAPI_ERROR_NOT_FOUND;
    }

    AnalysisResult* anomaly_result = spatiotemporal_anomaly_detection(
        &g_st_manager, dataset_id, threshold, "zscore");
    
    if (!anomaly_result || !anomaly_result->success) {
        if (anomaly_result) {
            spatiotemporal_free_analysis_result(anomaly_result);
        }
        return STAPI_ERROR_ANALYSIS;
    }

    *result = anomaly_result;
    return STAPI_OK;
}

// 生成2D散点图
SpatiotemporalAPIStatus spatiotemporal_plot_2d_scatter(size_t dataset_id,
                                                        const char* output_path,
                                                        int width,
                                                        int height) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count || !output_path) {
        return STAPI_ERROR_INVALID_PARAM;
    }

    VisualizationConfig* config = spatiotemporal_visualization_create_config(VIS_2D_SCATTER, width, height);
    if (!config) {
        return STAPI_ERROR_VISUALIZATION;
    }

    int result = spatiotemporal_visualization_2d_scatter(
        &g_st_manager, dataset_id, output_path, config);
    
    spatiotemporal_visualization_destroy_config(config);
    
    return (result == 0) ? STAPI_OK : STAPI_ERROR_VISUALIZATION;
}

// 生成热力图
SpatiotemporalAPIStatus spatiotemporal_plot_heatmap(size_t dataset_id,
                                                     const char* output_path,
                                                     int width,
                                                     int height) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count || !output_path) {
        return STAPI_ERROR_INVALID_PARAM;
    }

    VisualizationConfig* config = spatiotemporal_visualization_create_config(VIS_HEATMAP, width, height);
    if (!config) {
        return STAPI_ERROR_VISUALIZATION;
    }

    int result = spatiotemporal_visualization_heatmap(
        &g_st_manager, dataset_id, output_path, config);
    
    spatiotemporal_visualization_destroy_config(config);
    
    return (result == 0) ? STAPI_OK : STAPI_ERROR_VISUALIZATION;
}

// 生成时间序列图
SpatiotemporalAPIStatus spatiotemporal_plot_time_series(size_t dataset_id,
                                                        const char* output_path,
                                                        int width,
                                                        int height) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count || !output_path) {
        return STAPI_ERROR_INVALID_PARAM;
    }

    VisualizationConfig* config = spatiotemporal_visualization_create_config(VIS_TIME_SERIES, width, height);
    if (!config) {
        return STAPI_ERROR_VISUALIZATION;
    }

    int result = spatiotemporal_visualization_time_series(
        &g_st_manager, dataset_id, output_path, config);
    
    spatiotemporal_visualization_destroy_config(config);
    
    return (result == 0) ? STAPI_OK : STAPI_ERROR_VISUALIZATION;
}

// 导出数据为JSON
SpatiotemporalAPIStatus spatiotemporal_export_data(size_t dataset_id,
                                                   const char* output_path) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count || !output_path) {
        return STAPI_ERROR_INVALID_PARAM;
    }

    int result = spatiotemporal_export_visualization_data(
        &g_st_manager, dataset_id, output_path, VIS_2D_SCATTER);
    
    return (result == 0) ? STAPI_OK : STAPI_ERROR_VISUALIZATION;
}

// 获取数据集信息
SpatiotemporalAPIStatus spatiotemporal_get_dataset_info(size_t dataset_id,
                                                        size_t* point_count,
                                                        double* min_coord_x,
                                                        double* max_coord_x,
                                                        double* min_coord_y,
                                                        double* max_coord_y,
                                                        int64_t* min_time,
                                                        int64_t* max_time) {
    if (!g_initialized) {
        return STAPI_ERROR_INIT;
    }

    if (dataset_id >= g_st_manager.dataset_count) {
        return STAPI_ERROR_NOT_FOUND;
    }

    SpatiotemporalBounds bounds;
    SpatiotemporalStatus status = spatiotemporal_dataset_get_bounds(&g_st_manager, dataset_id, &bounds);
    if (status != SPATIOTEMPORAL_SUCCESS) {
        return STAPI_ERROR_BOUNDS;
    }

    if (point_count) *point_count = g_st_manager.datasets[dataset_id].count;
    if (min_coord_x) *min_coord_x = bounds.min_coords[0];
    if (max_coord_x) *max_coord_x = bounds.max_coords[0];
    if (min_coord_y) *min_coord_y = bounds.min_coords[1];
    if (max_coord_y) *max_coord_y = bounds.max_coords[1];
    if (min_time) *min_time = bounds.min_timestamp;
    if (max_time) *max_time = bounds.max_timestamp;

    return STAPI_OK;
}

// 释放分析结果
void spatiotemporal_free_result(AnalysisResult* result) {
    if (result) {
        spatiotemporal_free_analysis_result(result);
    }
}