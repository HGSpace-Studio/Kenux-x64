#include "spatiotemporal_analysis.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

// 辅助函数：计算两点间欧几里得距离
static double euclidean_distance(const double* p1, const double* p2) {
    double sum = 0.0;
    for (int i = 0; i < SPATIAL_DIMENSIONS; i++) {
        double diff = p1[i] - p2[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

// 辅助函数：计算平均值
static double calculate_mean(const value_t* values, size_t count) {
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}

// 辅助函数：计算标准差
static double calculate_stddev(const value_t* values, size_t count, double mean) {
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum += diff * diff;
    }
    return sqrt(sum / count);
}

// 空间聚类分析
AnalysisResult* spatiotemporal_spatial_clustering(
    const SpatiotemporalManager* manager, 
    size_t dataset_id, 
    size_t max_clusters,
    size_t max_iterations,
    double convergence_threshold
) {
    if (!manager || dataset_id >= manager->dataset_count || max_clusters == 0) {
        AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
        result->type = SPATIAL_CLUSTERING;
        result->success = false;
        result->message = strdup("Invalid parameters");
        result->results = NULL;
        return result;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    size_t point_count = dataset->count;

    // 分配聚类结果结构
    ClusteringResult* cluster_result = (ClusteringResult*)malloc(sizeof(ClusteringResult));
    if (!cluster_result) {
        AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
        result->type = SPATIAL_CLUSTERING;
        result->success = false;
        result->message = strdup("Memory allocation failed");
        result->results = NULL;
        return result;
    }

    cluster_result->cluster_count = max_clusters;
    cluster_result->cluster_sizes = (size_t*)calloc(max_clusters, sizeof(size_t));
    cluster_result->cluster_point_indices = (size_t**)malloc(max_clusters * sizeof(size_t*));
    cluster_result->cluster_centers = (double**)malloc(max_clusters * sizeof(double*));

    // 初始化聚类中心（随机选择点）
    for (size_t k = 0; k < max_clusters; k++) {
        cluster_result->cluster_centers[k] = (double*)malloc(SPATIAL_DIMENSIONS * sizeof(double));
        size_t random_point = rand() % point_count;
        for (int i = 0; i < SPATIAL_DIMENSIONS; i++) {
            cluster_result->cluster_centers[k][i] = dataset->points[random_point].coordinates[i];
        }
    }

    // K-means迭代
    size_t* assignments = (size_t*)malloc(point_count * sizeof(size_t));
    double** old_centers = (double**)malloc(max_clusters * sizeof(double*));
    for (size_t k = 0; k < max_clusters; k++) {
        old_centers[k] = (double*)malloc(SPATIAL_DIMENSIONS * sizeof(double));
    }

    bool converged = false;
    for (size_t iter = 0; iter < max_iterations && !converged; iter++) {
        // 分配点到最近的聚类
        for (size_t i = 0; i < point_count; i++) {
            assignments[i] = spatiotemporal_kmeans_assign_cluster(&dataset->points[i], 
                                                                  (const double**)cluster_result->cluster_centers, 
                                                                  max_clusters);
        }

        // 保存旧的聚类中心
        for (size_t k = 0; k < max_clusters; k++) {
            memcpy(old_centers[k], cluster_result->cluster_centers[k], SPATIAL_DIMENSIONS * sizeof(double));
        }

        // 更新聚类中心
        converged = spatiotemporal_kmeans_update_centers(dataset->points, point_count, assignments, 
                                                        max_clusters, cluster_result->cluster_centers);

        // 检查收敛
        if (converged) break;

        // 检查收敛条件
        double max_change = 0.0;
        for (size_t k = 0; k < max_clusters; k++) {
            double change = euclidean_distance(old_centers[k], cluster_result->cluster_centers[k]);
            if (change > max_change) {
                max_change = change;
            }
        }

        if (max_change < convergence_threshold) {
            converged = true;
        }
    }

    // 构建最终聚类结果
    for (size_t k = 0; k < max_clusters; k++) {
        // 计算聚类大小
        size_t cluster_size = 0;
        for (size_t i = 0; i < point_count; i++) {
            if (assignments[i] == k) {
                cluster_size++;
            }
        }
        cluster_result->cluster_sizes[k] = cluster_size;

        // 分配聚类点索引数组
        if (cluster_size > 0) {
            cluster_result->cluster_point_indices[k] = (size_t*)malloc(cluster_size * sizeof(size_t));
            size_t index = 0;
            for (size_t i = 0; i < point_count; i++) {
                if (assignments[i] == k) {
                    cluster_result->cluster_point_indices[k][index++] = i;
                }
            }
        } else {
            cluster_result->cluster_point_indices[k] = NULL;
        }
    }

    // 创建分析结果
    AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
    result->type = SPATIAL_CLUSTERING;
    result->success = true;
    result->message = strdup("Spatial clustering completed successfully");
    result->results = cluster_result;

    // 清理临时内存
    free(assignments);
    for (size_t k = 0; k < max_clusters; k++) {
        free(old_centers[k]);
    }
    free(old_centers);

    return result;
}

// 时间趋势分析
AnalysisResult* spatiotemporal_temporal_trend_analysis(
    const SpatiotemporalManager* manager, 
    size_t dataset_id,
    size_t time_window_size
) {
    if (!manager || dataset_id >= manager->dataset_count || time_window_size == 0) {
        AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
        result->type = TEMPORAL_TREND_ANALYSIS;
        result->success = false;
        result->message = strdup("Invalid parameters");
        result->results = NULL;
        return result;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    size_t point_count = dataset->count;

    // 按时间戳排序点
    SpatiotemporalPoint* sorted_points = (SpatiotemporalPoint*)malloc(point_count * sizeof(SpatiotemporalPoint));
    memcpy(sorted_points, dataset->points, point_count * sizeof(SpatiotemporalPoint));

    // 简单的时间戳排序
    for (size_t i = 0; i < point_count - 1; i++) {
        for (size_t j = i + 1; j < point_count; j++) {
            if (sorted_points[i].timestamp > sorted_points[j].timestamp) {
                SpatiotemporalPoint temp = sorted_points[i];
                sorted_points[i] = sorted_points[j];
                sorted_points[j] = temp;
            }
        }
    }

    // 提取值和时间戳
    value_t* values = (value_t*)malloc(point_count * sizeof(value_t));
    timestamp_t* timestamps = (timestamp_t*)malloc(point_count * sizeof(timestamp_t));
    for (size_t i = 0; i < point_count; i++) {
        values[i] = sorted_points[i].value;
        timestamps[i] = sorted_points[i].timestamp;
    }

    // 计算移动平均
    value_t* smoothed_values = (value_t*)malloc(point_count * sizeof(value_t));
    spatiotemporal_moving_average(values, point_count, time_window_size, smoothed_values);

    // 计算线性回归
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    for (size_t i = 0; i < point_count; i++) {
        // 归一化时间
        double x = (double)(timestamps[i] - timestamps[0]) / 1000.0; // 转换为秒
        double y = smoothed_values[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    double n = (double)point_count;
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    double mean_y = sum_y / n;
    
    // 计算相关系数
    double ss_tot = 0.0, ss_res = 0.0;
    for (size_t i = 0; i < point_count; i++) {
        double x = (double)(timestamps[i] - timestamps[0]) / 1000.0;
        double y = smoothed_values[i];
        double y_pred = slope * x + (sum_y - slope * sum_x) / n;
        
        ss_tot += (y - mean_y) * (y - mean_y);
        ss_res += (y - y_pred) * (y - y_pred);
    }

    double correlation = sqrt(1.0 - ss_res / ss_tot);

    // 创建趋势分析结果
    TrendAnalysisResult* trend_result = (TrendAnalysisResult*)malloc(sizeof(TrendAnalysisResult));
    trend_result->slope = slope;
    trend_result->correlation = correlation;
    trend_result->r_squared = correlation * correlation;
    trend_result->trend_start = timestamps[0];
    trend_result->trend_end = timestamps[point_count - 1];

    AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
    result->type = TEMPORAL_TREND_ANALYSIS;
    result->success = true;
    result->message = strdup("Temporal trend analysis completed successfully");
    result->results = trend_result;

    // 清理临时内存
    free(sorted_points);
    free(values);
    free(timestamps);
    free(smoothed_values);

    return result;
}

// 时空相关性分析
AnalysisResult* spatiotemporal_spatiotemporal_correlation(
    const SpatiotemporalManager* manager, 
    size_t dataset_id1,
    size_t dataset_id2,
    size_t spatial_lag,
    size_t temporal_lag
) {
    if (!manager || dataset_id1 >= manager->dataset_count || dataset_id2 >= manager->dataset_count) {
        AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
        result->type = SPATIOTEMPORAL_CORRELATION;
        result->success = false;
        result->message = strdup("Invalid parameters");
        result->results = NULL;
        return result;
    }

    const SpatiotemporalDataset* dataset1 = &manager->datasets[dataset_id1];
    const SpatiotemporalDataset* dataset2 = &manager->datasets[dataset_id2];

    // 创建相关性分析结果
    CorrelationResult* corr_result = (CorrelationResult*)malloc(sizeof(CorrelationResult));
    corr_result->spatial_correlation = spatiotemporal_spatial_autocorrelation(dataset1->points, dataset1->count, spatial_lag);
    corr_result->temporal_correlation = spatiotemporal_autocorrelation(dataset1->points, dataset1->count, temporal_lag);
    corr_result->spatiotemporal_correlation = 0.5 * (corr_result->spatial_correlation + corr_result->temporal_correlation);
    corr_result->p_value = 0.05; // 简化的p值

    AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
    result->type = SPATIOTEMPORAL_CORRELATION;
    result->success = true;
    result->message = strdup("Spatiotemporal correlation analysis completed successfully");
    result->results = corr_result;

    return result;
}

// 异常检测（Z-score方法）
AnalysisResult* spatiotemporal_anomaly_detection(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    double threshold,
    const char* method
) {
    if (!manager || dataset_id >= manager->dataset_count || threshold <= 0) {
        AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
        result->type = ANOMALY_DETECTION;
        result->success = false;
        result->message = strdup("Invalid parameters");
        result->results = NULL;
        return result;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    size_t point_count = dataset->count;

    // 计算均值和标准差
    double mean = calculate_mean(dataset->points, point_count);
    double stddev = calculate_stddev(dataset->points, point_count, mean);

    // 检测异常
    size_t anomaly_count = 0;
    for (size_t i = 0; i < point_count; i++) {
        double z_score = fabs((dataset->points[i].value - mean) / stddev);
        if (z_score > threshold) {
            anomaly_count++;
        }
    }

    // 分配异常检测结果
    AnomalyDetectionResult* anomaly_result = (AnomalyDetectionResult*)malloc(sizeof(AnomalyDetectionResult));
    anomaly_result->anomaly_count = anomaly_count;
    anomaly_result->anomaly_indices = (size_t*)malloc(anomaly_count * sizeof(size_t));
    anomaly_result->anomaly_scores = (double*)malloc(anomaly_count * sizeof(double));
    anomaly_result->threshold = threshold;

    // 收集异常点
    size_t index = 0;
    for (size_t i = 0; i < point_count; i++) {
        double z_score = fabs((dataset->points[i].value - mean) / stddev);
        if (z_score > threshold) {
            anomaly_result->anomaly_indices[index] = i;
            anomaly_result->anomaly_scores[index] = z_score;
            index++;
        }
    }

    AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
    result->type = ANOMALY_DETECTION;
    result->success = true;
    result->message = strdup("Anomaly detection completed successfully");
    result->results = anomaly_result;

    return result;
}

// 模式识别
AnalysisResult* spatiotemporal_pattern_recognition(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    size_t pattern_length,
    size_t min_support
) {
    if (!manager || dataset_id >= manager->dataset_count || pattern_length == 0) {
        AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
        result->type = PATTERN_RECOGNITION;
        result->success = false;
        result->message = strdup("Invalid parameters");
        result->results = NULL;
        return result;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    size_t point_count = dataset->count;

    // 简化的模式识别：寻找频繁的值序列
    size_t max_patterns = 10;
    PatternRecognitionResult* pattern_result = (PatternRecognitionResult*)malloc(sizeof(PatternRecognitionResult));
    pattern_result->pattern_count = max_patterns;
    pattern_result->pattern_frequencies = (double*)calloc(max_patterns, sizeof(double));
    pattern_result->pattern_point_indices = (size_t**)malloc(max_patterns * sizeof(size_t*));
    pattern_result->pattern_length = pattern_length;

    // 创建值序列
    value_t* sequence = (value_t*)malloc(point_count * sizeof(value_t));
    for (size_t i = 0; i < point_count; i++) {
        sequence[i] = dataset->points[i].value;
    }

    // 简化的频繁模式挖掘
    size_t* pattern_counts = (size_t*)calloc(1000, sizeof(size_t)); // 假设有1000种可能的模式
    for (size_t i = 0; i <= point_count - pattern_length; i++) {
        // 将序列转换为模式索引（简化处理）
        size_t pattern_index = 0;
        for (size_t j = 0; j < pattern_length; j++) {
            pattern_index = pattern_index * 10 + (size_t)(sequence[i + j] * 10); // 简化编码
        }
        pattern_counts[pattern_index]++;
    }

    // 找到频繁模式
    size_t frequent_pattern_count = 0;
    for (size_t i = 0; i < 1000 && frequent_pattern_count < max_patterns; i++) {
        if (pattern_counts[i] >= min_support) {
            pattern_result->pattern_frequencies[frequent_pattern_count] = 
                (double)pattern_counts[i] / (point_count - pattern_length + 1);
            
            // 为每个频繁模式分配点索引数组
            size_t* pattern_indices = (size_t*)malloc(pattern_counts[i] * sizeof(size_t));
            size_t index = 0;
            for (size_t j = 0; j <= point_count - pattern_length; j++) {
                size_t current_pattern = 0;
                for (size_t k = 0; k < pattern_length; k++) {
                    current_pattern = current_pattern * 10 + (size_t)(sequence[j + k] * 10);
                }
                if (current_pattern == i) {
                    pattern_indices[index++] = j;
                }
            }
            pattern_result->pattern_point_indices[frequent_pattern_count] = pattern_indices;
            frequent_pattern_count++;
        }
    }

    pattern_result->pattern_count = frequent_pattern_count;

    AnalysisResult* result = (AnalysisResult*)malloc(sizeof(AnalysisResult));
    result->type = PATTERN_RECOGNITION;
    result->success = true;
    result->message = strdup("Pattern recognition completed successfully");
    result->results = pattern_result;

    // 清理临时内存
    free(sequence);
    free(pattern_counts);

    return result;
}

// 释放分析结果
void spatiotemporal_free_analysis_result(AnalysisResult* result) {
    if (!result) return;

    if (result->message) {
        free(result->message);
    }

    if (result->results) {
        switch (result->type) {
            case SPATIAL_CLUSTERING: {
                ClusteringResult* cluster = (ClusteringResult*)result->results;
                for (size_t k = 0; k < cluster->cluster_count; k++) {
                    if (cluster->cluster_point_indices[k]) {
                        free(cluster->cluster_point_indices[k]);
                    }
                    if (cluster->cluster_centers[k]) {
                        free(cluster->cluster_centers[k]);
                    }
                }
                free(cluster->cluster_sizes);
                free(cluster->cluster_point_indices);
                free(cluster->cluster_centers);
                free(cluster);
                break;
            }
            case ANOMALY_DETECTION: {
                AnomalyDetectionResult* anomaly = (AnomalyDetectionResult*)result->results;
                if (anomaly->anomaly_indices) {
                    free(anomaly->anomaly_indices);
                }
                if (anomaly->anomaly_scores) {
                    free(anomaly->anomaly_scores);
                }
                free(anomaly);
                break;
            }
            case PATTERN_RECOGNITION: {
                PatternRecognitionResult* pattern = (PatternRecognitionResult*)result->results;
                for (size_t i = 0; i < pattern->pattern_count; i++) {
                    if (pattern->pattern_point_indices[i]) {
                        free(pattern->pattern_point_indices[i]);
                    }
                }
                free(pattern->pattern_frequencies);
                free(pattern->pattern_point_indices);
                free(pattern);
                break;
            }
            case TEMPORAL_TREND_ANALYSIS:
            case SPATIOTEMPORAL_CORRELATION:
                free(result->results);
                break;
            default:
                free(result->results);
        }
    }

    free(result);
}

// K-means聚类辅助函数
size_t spatiotemporal_kmeans_assign_cluster(
    const SpatiotemporalPoint* point,
    const double** centers,
    size_t k
) {
    size_t best_cluster = 0;
    double min_distance = INFINITY;

    for (size_t i = 0; i < k; i++) {
        double distance = 0.0;
        for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
            double diff = point->coordinates[j] - centers[i][j];
            distance += diff * diff;
        }
        distance = sqrt(distance);

        if (distance < min_distance) {
            min_distance = distance;
            best_cluster = i;
        }
    }

    return best_cluster;
}

bool spatiotemporal_kmeans_update_centers(
    const SpatiotemporalPoint* points,
    size_t point_count,
    const size_t* assignments,
    size_t k,
    double** new_centers
) {
    bool converged = true;
    size_t* cluster_counts = (size_t*)calloc(k, sizeof(size_t));
    double** new_sums = (double**)malloc(k * sizeof(double*));

    // 初始化
    for (size_t i = 0; i < k; i++) {
        new_sums[i] = (double*)calloc(SPATIAL_DIMENSIONS, sizeof(double));
        for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
            new_centers[i][j] = 0.0;
        }
    }

    // 计算聚类和
    for (size_t i = 0; i < point_count; i++) {
        size_t cluster = assignments[i];
        cluster_counts[cluster]++;
        for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
            new_sums[cluster][j] += points[i].coordinates[j];
        }
    }

    // 更新中心点
    for (size_t i = 0; i < k; i++) {
        if (cluster_counts[i] > 0) {
            for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
                double old_center = new_centers[i][j];
                new_centers[i][j] = new_sums[i][j] / cluster_counts[i];
                
                // 检查收敛
                if (fabs(new_centers[i][j] - old_center) > 1e-6) {
                    converged = false;
                }
            }
        }
    }

    // 清理
    for (size_t i = 0; i < k; i++) {
        free(new_sums[i]);
    }
    free(new_sums);
    free(cluster_counts);

    return converged;
}

// 计算移动平均
void spatiotemporal_moving_average(
    const value_t* values,
    size_t count,
    size_t window_size,
    value_t* result
) {
    for (size_t i = 0; i < count; i++) {
        double sum = 0.0;
        size_t window_count = 0;
        
        for (size_t j = (i >= window_size) ? i - window_size + 1 : 0; j <= i; j++) {
            sum += values[j];
            window_count++;
        }
        
        result[i] = (value_t)(sum / window_count);
    }
}

// 计算时间序列的自相关
double spatiotemporal_autocorrelation(
    const value_t* series,
    size_t length,
    size_t lag
) {
    if (lag >= length) return 0.0;

    double mean = calculate_mean(series, length);
    double numerator = 0.0;
    double denominator = 0.0;

    for (size_t i = 0; i < length - lag; i++) {
        numerator += (series[i] - mean) * (series[i + lag] - mean);
    }

    for (size_t i = 0; i < length - lag; i++) {
        denominator += (series[i] - mean) * (series[i] - mean);
    }

    return denominator > 0.0 ? numerator / denominator : 0.0;
}

// 计算空间自相关
double spatiotemporal_spatial_autocorrelation(
    const SpatiotemporalPoint* points,
    size_t point_count,
    size_t spatial_lag
) {
    if (point_count < 2) return 0.0;

    double mean = calculate_mean(points, point_count);
    double numerator = 0.0;
    double denominator = 0.0;
    size_t pairs = 0;

    for (size_t i = 0; i < point_count; i++) {
        for (size_t j = i + 1; j < point_count; j++) {
            double distance = spatiotemporal_distance(points[i].coordinates, points[j].coordinates);
            
            if (distance <= spatial_lag) {
                numerator += (points[i].value - mean) * (points[j].value - mean);
                pairs++;
            }
        }
    }

    for (size_t i = 0; i < point_count; i++) {
        denominator += (points[i].value - mean) * (points[i].value - mean);
    }

    return pairs > 0 && denominator > 0.0 ? numerator / denominator : 0.0;
}