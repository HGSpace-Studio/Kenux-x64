#ifndef SPATIOTEMPORAL_VISUALIZATION_H
#define SPATIOTEMPORAL_VISUALIZATION_H

#include "spatiotemporal_types.h"
#include "spatiotemporal_core.h"
#include <stddef.h>

// 可视化类型枚举
typedef enum {
    VIS_2D_SCATTER,                   // 2D散点图
    VIS_3D_SCATTER,                   // 3D散点图
    VIS_HEATMAP,                      // 热力图
    VIS_TIME_SERIES,                  // 时间序列图
    VIS_SPATIAL_CLUSTERING,           // 空间聚类可视化
    VIS_TEMPORAL_HEATMAP,             // 时间热力图
    VIS_SPATIAL_HEATMAP               // 空间热力图
} VisualizationType;

// 可视化配置结构
typedef struct {
    VisualizationType type;           // 可视化类型
    int width;                        // 图像宽度
    int height;                       // 图像高度
    bool show_axes;                   // 是否显示坐标轴
    bool show_grid;                   // 是否显示网格
    bool show_legend;                 // 是否显示图例
    char* title;                      // 图像标题
    char* xlabel;                     // X轴标签
    char* ylabel;                     // Y轴标签
    char* zlabel;                     // Z轴标签
    char* output_format;              // 输出格式（"png", "jpg", "bmp"等）
} VisualizationConfig;

// 颜色结构
typedef struct {
    uint8_t r;                        // 红色分量 (0-255)
    uint8_t g;                        // 绿色分量 (0-255)
    uint8_t b;                        // 蓝色分量 (0-255)
    uint8_t a;                        // 透明度分量 (0-255)
} Color;

// 颜色方案枚举
typedef enum {
    COLOR_SCHEME_RAINBOW,             // 彩虹色方案
    COLOR_SCHEME_HEAT,               // 热力色方案
    COLOR_SCHEME_COOL,               // 冷色方案
    COLOR_SCHEME_GRAYSCALE,          // 灰度方案
    COLOR_SCHEME_CUSTOM              // 自定义方案
} ColorScheme;

// 初始化可视化系统
int spatiotemporal_visualization_init();

// 清理可视化系统
void spatiotemporal_visualization_cleanup();

// 创建可视化配置
VisualizationConfig* spatiotemporal_visualization_create_config(
    VisualizationType type,
    int width,
    int height
);

// 销毁可视化配置
void spatiotemporal_visualization_destroy_config(VisualizationConfig* config);

// 设置颜色方案
void spatiotemporal_visualization_set_color_scheme(
    VisualizationConfig* config,
    ColorScheme scheme
);

// 设置自定义颜色
void spatiotemporal_visualization_set_custom_color(
    VisualizationConfig* config,
    const Color* colors,
    size_t count
);

// 生成2D散点图
int spatiotemporal_visualization_2d_scatter(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成3D散点图
int spatiotemporal_visualization_3d_scatter(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成热力图
int spatiotemporal_visualization_heatmap(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成时间序列图
int spatiotemporal_visualization_time_series(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成空间聚类可视化
int spatiotemporal_visualization_spatial_clustering(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const AnalysisResult* clustering_result,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成时间热力图
int spatiotemporal_visualization_temporal_heatmap(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成空间热力图
int spatiotemporal_visualization_spatial_heatmap(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
);

// 生成动画（时间序列变化）
int spatiotemporal_visualization_animation(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_dir,
    const VisualizationConfig* config,
    size_t frame_count,
    bool animate_points
);

// 获取颜色映射值
Color spatiotemporal_color_map(double value, double min, double max, ColorScheme scheme);

// 辅助函数：将RGB转换为十六进制颜色值
uint32_t spatiotemporal_rgb_to_hex(const Color* color);

// 辅助函数：将十六进制转换为RGB颜色
Color spatiotemporal_hex_to_rgb(uint32_t hex);

// 导出可视化数据为JSON格式
int spatiotemporal_export_visualization_data(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    VisualizationType type
);

#endif // SPATIOTEMPORAL_VISUALIZATION_H