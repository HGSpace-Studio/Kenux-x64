#ifndef SPATIOTEMPORAL_CORE_H
#define SPATIOTEMPORAL_CORE_H

#include "spatiotemporal_types.h"
#include <stddef.h>

// 时空数据集管理器
typedef struct {
    SpatiotemporalDataset* datasets;     // 数据集数组
    size_t dataset_count;                // 数据集数量
    size_t capacity;                     // 总容量
} SpatiotemporalManager;

// 函数返回状态码
typedef enum {
    SPATIOTEMPORAL_SUCCESS = 0,
    SPATIOTEMPORAL_ERROR_MEMORY,
    SPATIOTEMPORAL_ERROR_INVALID_PARAMETER,
    SPATIOTEMPORAL_ERROR_NOT_FOUND,
    SPATIOTEMPORAL_ERROR_BOUNDS
} SpatiotemporalStatus;

// 初始化管理器
SpatiotemporalStatus spatiotemporal_manager_init(SpatiotemporalManager* manager, size_t initial_capacity);

// 清理管理器
void spatiotemporal_manager_cleanup(SpatiotemporalManager* manager);

// 创建新数据集
SpatiotemporalStatus spatiotemporal_manager_create_dataset(
    SpatiotemporalManager* manager, 
    size_t* dataset_id
);

// 删除数据集
SpatiotemporalStatus spatiotemporal_manager_delete_dataset(
    SpatiotemporalManager* manager, 
    size_t dataset_id
);

// 向数据集添加点
SpatiotemporalStatus spatiotemporal_dataset_add_point(
    SpatiotemporalManager* manager, 
    size_t dataset_id, 
    const SpatiotemporalPoint* point
);

// 批量添加点
SpatiotemporalStatus spatiotemporal_dataset_add_points(
    SpatiotemporalManager* manager, 
    size_t dataset_id, 
    const SpatiotemporalPoint* points, 
    size_t point_count
);

// 查询时空数据
SpatiotemporalStatus spatiotemporal_dataset_query(
    const SpatiotemporalManager* manager, 
    size_t dataset_id, 
    const SpatiotemporalQuery* query, 
    SpatiotemporalPoint** results, 
    size_t* result_count
);

// 获取数据集边界
SpatiotemporalStatus spatiotemporal_dataset_get_bounds(
    const SpatiotemporalManager* manager, 
    size_t dataset_id, 
    SpatiotemporalBounds* bounds
);

// 更新数据集边界（内部使用）
void spatiotemporal_update_bounds(
    SpatiotemporalBounds* bounds, 
    const SpatiotemporalPoint* point
);

// 计算两点之间的欧几里得距离
spatial_coord_t spatiotemporal_distance(
    const spatial_coord_t* p1, 
    const spatial_coord_t* p2
);

// 比较两个时间戳
int spatiotemporal_compare_timestamps(timestamp_t t1, timestamp_t t2);

// 判断点是否在查询范围内
bool spatiotemporal_point_in_bounds(
    const SpatiotemporalPoint* point, 
    const SpatiotemporalQuery* query
);

#endif // SPATIOTEMPORAL_CORE_H