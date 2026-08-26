#ifndef SPATIOTEMPORAL_TYPES_H
#define SPATIOTEMPORAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// 基本数据类型
typedef double spatial_coord_t;  // 空间坐标
typedef int64_t timestamp_t;     // 时间戳
typedef float value_t;          // 数据值

// 空间维度
#define SPATIAL_DIMENSIONS 3     // 支持三维空间

// 时空数据点结构
typedef struct {
    spatial_coord_t coordinates[SPATIAL_DIMENSIONS];  // x, y, z 坐标
    timestamp_t timestamp;                           // 时间戳
    value_t value;                                   // 关联值
} SpatiotemporalPoint;

// 时空数据范围结构
typedef struct {
    spatial_coord_t min_coords[SPATIAL_DIMENSIONS];  // 最小坐标
    spatial_coord_t max_coords[SPATIAL_DIMENSIONS];  // 最大坐标
    timestamp_t min_timestamp;                       // 最小时间
    timestamp_t max_timestamp;                       // 最大时间
} SpatiotemporalBounds;

// 时空查询范围
typedef struct {
    spatial_coord_t query_min[SPATIAL_DIMENSIONS];    // 查询最小坐标
    spatial_coord_t query_max[SPATIAL_DIMENSIONS];    // 查询最大坐标
    timestamp_t start_time;                          // 开始时间
    timestamp_t end_time;                            // 结束时间
} SpatiotemporalQuery;

// 时空数据集结构
typedef struct {
    SpatiotemporalPoint* points;    // 数据点数组
    size_t count;                    // 数据点数量
    SpatiotemporalBounds bounds;     // 数据范围
} SpatiotemporalDataset;

// 时空分析方法枚举
typedef enum {
    SPATIAL_CLUSTERING,              // 空间聚类
    TEMPORAL_TREND_ANALYSIS,        // 时间趋势分析
    SPATIOTEMPORAL_CORRELATION,     // 时空相关性分析
    ANOMALY_DETECTION,              // 异常检测
    PATTERN_RECOGNITION             // 模式识别
} AnalysisType;

// 分析结果结构
typedef struct {
    AnalysisType type;               // 分析类型
    bool success;                    // 是否成功
    char* message;                   // 结果消息
    void* results;                  // 具体结果指针（类型取决于分析类型）
} AnalysisResult;

// 内存管理函数
void spatiotemporal_init();
void spatiotemporal_cleanup();

#endif // SPATIOTEMPORAL_TYPES_H