#include "spatiotemporal_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// 初始化管理器
SpatiotemporalStatus spatiotemporal_manager_init(SpatiotemporalManager* manager, size_t initial_capacity) {
    if (!manager || initial_capacity == 0) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    manager->datasets = (SpatiotemporalDataset*)calloc(initial_capacity, sizeof(SpatiotemporalDataset));
    if (!manager->datasets) {
        return SPATIOTEMPORAL_ERROR_MEMORY;
    }

    manager->dataset_count = 0;
    manager->capacity = initial_capacity;

    // 初始化每个数据集
    for (size_t i = 0; i < initial_capacity; i++) {
        manager->datasets[i].points = NULL;
        manager->datasets[i].count = 0;
        
        // 初始化边界
        for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
            manager->datasets[i].bounds.min_coords[j] = INFINITY;
            manager->datasets[i].bounds.max_coords[j] = -INFINITY;
        }
        manager->datasets[i].bounds.min_timestamp = INT64_MAX;
        manager->datasets[i].bounds.max_timestamp = INT64_MIN;
    }

    return SPATIOTEMPORAL_SUCCESS;
}

// 清理管理器
void spatiotemporal_manager_cleanup(SpatiotemporalManager* manager) {
    if (!manager) return;

    for (size_t i = 0; i < manager->dataset_count; i++) {
        free(manager->datasets[i].points);
    }
    
    free(manager->datasets);
    manager->datasets = NULL;
    manager->dataset_count = 0;
    manager->capacity = 0;
}

// 创建新数据集
SpatiotemporalStatus spatiotemporal_manager_create_dataset(SpatiotemporalManager* manager, size_t* dataset_id) {
    if (!manager || !dataset_id) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    // 检查是否需要扩容
    if (manager->dataset_count >= manager->capacity) {
        size_t new_capacity = manager->capacity * 2;
        SpatiotemporalDataset* new_datasets = (SpatiotemporalDataset*)realloc(
            manager->datasets, 
            new_capacity * sizeof(SpatiotemporalDataset)
        );
        
        if (!new_datasets) {
            return SPATIOTEMPORAL_ERROR_MEMORY;
        }

        // 初始化新增的数据集
        for (size_t i = manager->capacity; i < new_capacity; i++) {
            new_datasets[i].points = NULL;
            new_datasets[i].count = 0;
            
            for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
                new_datasets[i].bounds.min_coords[j] = INFINITY;
                new_datasets[i].bounds.max_coords[j] = -INFINITY;
            }
            new_datasets[i].bounds.min_timestamp = INT64_MAX;
            new_datasets[i].bounds.max_timestamp = INT64_MIN;
        }

        manager->datasets = new_datasets;
        manager->capacity = new_capacity;
    }

    *dataset_id = manager->dataset_count++;
    return SPATIOTEMPORAL_SUCCESS;
}

// 删除数据集
SpatiotemporalStatus spatiotemporal_manager_delete_dataset(SpatiotemporalManager* manager, size_t dataset_id) {
    if (!manager || dataset_id >= manager->dataset_count) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    // 释放数据点内存
    free(manager->datasets[dataset_id].points);
    manager->datasets[dataset_id].points = NULL;
    manager->datasets[dataset_id].count = 0;

    // 重置边界
    for (int j = 0; j < SPATIAL_DIMENSIONS; j++) {
        manager->datasets[dataset_id].bounds.min_coords[j] = INFINITY;
        manager->datasets[dataset_id].bounds.max_coords[j] = -INFINITY;
    }
    manager->datasets[dataset_id].bounds.min_timestamp = INT64_MAX;
    manager->datasets[dataset_id].bounds.max_timestamp = INT64_MIN;

    return SPATIOTEMPORAL_SUCCESS;
}

// 向数据集添加点
SpatiotemporalStatus spatiotemporal_dataset_add_point(SpatiotemporalManager* manager, size_t dataset_id, const SpatiotemporalPoint* point) {
    if (!manager || dataset_id >= manager->dataset_count || !point) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    
    // 扩展数组
    SpatiotemporalPoint* new_points = (SpatiotemporalPoint*)realloc(
        dataset->points, 
        (dataset->count + 1) * sizeof(SpatiotemporalPoint)
    );
    
    if (!new_points) {
        return SPATIOTEMPORAL_ERROR_MEMORY;
    }

    dataset->points = new_points;
    dataset->points[dataset->count] = *point;
    dataset->count++;

    // 更新边界
    spatiotemporal_update_bounds(&dataset->bounds, point);

    return SPATIOTEMPORAL_SUCCESS;
}

// 批量添加点
SpatiotemporalStatus spatiotemporal_dataset_add_points(SpatiotemporalManager* manager, size_t dataset_id, const SpatiotemporalPoint* points, size_t point_count) {
    if (!manager || dataset_id >= manager->dataset_count || !points || point_count == 0) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    
    // 扩展数组
    SpatiotemporalPoint* new_points = (SpatiotemporalPoint*)realloc(
        dataset->points, 
        (dataset->count + point_count) * sizeof(SpatiotemporalPoint)
    );
    
    if (!new_points) {
        return SPATIOTEMPORAL_ERROR_MEMORY;
    }

    dataset->points = new_points;
    memcpy(&dataset->points[dataset->count], points, point_count * sizeof(SpatiotemporalPoint));
    dataset->count += point_count;

    // 批量更新边界
    for (size_t i = 0; i < point_count; i++) {
        spatiotemporal_update_bounds(&dataset->bounds, &points[i]);
    }

    return SPATIOTEMPORAL_SUCCESS;
}

// 查询时空数据
SpatiotemporalStatus spatiotemporal_dataset_query(const SpatiotemporalManager* manager, size_t dataset_id, const SpatiotemporalQuery* query, SpatiotemporalPoint** results, size_t* result_count) {
    if (!manager || dataset_id >= manager->dataset_count || !query || !results || !result_count) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    size_t count = 0;

    // 首先计算匹配数量
    for (size_t i = 0; i < dataset->count; i++) {
        if (spatiotemporal_point_in_bounds(&dataset->points[i], query)) {
            count++;
        }
    }

    // 分配内存
    SpatiotemporalPoint* matched_points = (SpatiotemporalPoint*)malloc(count * sizeof(SpatiotemporalPoint));
    if (!matched_points) {
        return SPATIOTEMPORAL_ERROR_MEMORY;
    }

    // 收集匹配的点
    size_t index = 0;
    for (size_t i = 0; i < dataset->count; i++) {
        if (spatiotemporal_point_in_bounds(&dataset->points[i], query)) {
            matched_points[index++] = dataset->points[i];
        }
    }

    *results = matched_points;
    *result_count = count;
    return SPATIOTEMPORAL_SUCCESS;
}

// 获取数据集边界
SpatiotemporalStatus spatiotemporal_dataset_get_bounds(const SpatiotemporalManager* manager, size_t dataset_id, SpatiotemporalBounds* bounds) {
    if (!manager || dataset_id >= manager->dataset_count || !bounds) {
        return SPATIOTEMPORAL_ERROR_INVALID_PARAMETER;
    }

    *bounds = manager->datasets[dataset_id].bounds;
    return SPATIOTEMPORAL_SUCCESS;
}

// 更新数据集边界
void spatiotemporal_update_bounds(SpatiotemporalBounds* bounds, const SpatiotemporalPoint* point) {
    for (int i = 0; i < SPATIAL_DIMENSIONS; i++) {
        if (point->coordinates[i] < bounds->min_coords[i]) {
            bounds->min_coords[i] = point->coordinates[i];
        }
        if (point->coordinates[i] > bounds->max_coords[i]) {
            bounds->max_coords[i] = point->coordinates[i];
        }
    }

    if (point->timestamp < bounds->min_timestamp) {
        bounds->min_timestamp = point->timestamp;
    }
    if (point->timestamp > bounds->max_timestamp) {
        bounds->max_timestamp = point->timestamp;
    }
}

// 计算两点之间的欧几里得距离
spatial_coord_t spatiotemporal_distance(const spatial_coord_t* p1, const spatial_coord_t* p2) {
    spatial_coord_t sum = 0.0;
    for (int i = 0; i < SPATIAL_DIMENSIONS; i++) {
        spatial_coord_t diff = p1[i] - p2[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

// 比较两个时间戳
int spatiotemporal_compare_timestamps(timestamp_t t1, timestamp_t t2) {
    if (t1 < t2) return -1;
    if (t1 > t2) return 1;
    return 0;
}

// 判断点是否在查询范围内
bool spatiotemporal_point_in_bounds(const SpatiotemporalPoint* point, const SpatiotemporalQuery* query) {
    // 检查空间范围
    for (int i = 0; i < SPATIAL_DIMENSIONS; i++) {
        if (point->coordinates[i] < query->query_min[i] || point->coordinates[i] > query->query_max[i]) {
            return false;
        }
    }

    // 检查时间范围
    if (point->timestamp < query->start_time || point->timestamp > query->end_time) {
        return false;
    }

    return true;
}