#include "spatiotemporal_visualization.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

// 预定义颜色方案
static const Color RAINBOW_COLORS[] = {
    {255, 0, 0, 255},    // 红
    {255, 127, 0, 255},  // 橙
    {255, 255, 0, 255},  // 黄
    {0, 255, 0, 255},    // 绿
    {0, 0, 255, 255},    // 蓝
    {75, 0, 130, 255},   // 靛
    {148, 0, 211, 255}   // 紫
};

static const Color HEAT_COLORS[] = {
    {0, 0, 255, 255},    // 深蓝
    {0, 255, 255, 255},  // 青
    {0, 255, 0, 255},    // 绿
    {255, 255, 0, 255},  // 黄
    {255, 127, 0, 255},  // 橙
    {255, 0, 0, 255}     // 红
};

static const Color COOL_COLORS[] = {
    {0, 255, 255, 255},  // 青
    {0, 0, 255, 255},    // 蓝
    {128, 0, 128, 255}   // 紫罗兰
};

static const Color GRAYSCALE_COLORS[] = {
    {0, 0, 0, 255},      // 黑
    {128, 128, 128, 255}, // 灰
    {255, 255, 255, 255}  // 白
};

// 内部函数：创建BMP图像
static int create_bmp_image(
    const char* path,
    int width,
    int height,
    const uint8_t* pixel_data
) {
    // BMP文件头结构
    typedef struct {
        uint16_t signature;    // 文件标识 "BM"
        uint32_t file_size;    // 文件大小
        uint16_t reserved1;    // 保留
        uint16_t reserved2;    // 保留
        uint32_t data_offset;  // 数据偏移
    } BMPHeader;

    // BMP信息头结构
    typedef struct {
        uint32_t header_size;  // 信息头大小
        int32_t width;        // 图像宽度
        int32_t height;       // 图像高度
        uint16_t planes;      // 颜色平面数
        uint16_t bits_per_pixel; // 每像素位数
        uint32_t compression; // 压缩方式
        uint32_t image_size;  // 图像数据大小
        int32_t x_resolution; // 水平分辨率
        int32_t y_resolution; // 垂直分辨率
        uint32_t colors_used; // 使用的颜色数
        uint32_t colors_important; // 重要颜色数
    } BMPInfoHeader;

    // 计算每行字节数（4字节对齐）
    int row_size = (width * 3 + 3) & ~3;
    int image_size = row_size * height;
    int file_size = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + image_size;

    // 分配文件数据
    uint8_t* file_data = (uint8_t*)malloc(file_size);
    if (!file_data) {
        return -1;
    }

    // 填充BMP头
    BMPHeader* header = (BMPHeader*)file_data;
    header->signature = 0x4D42; // "BM"
    header->file_size = file_size;
    header->reserved1 = 0;
    header->reserved2 = 0;
    header->data_offset = sizeof(BMPHeader) + sizeof(BMPInfoHeader);

    // 填充BMP信息头
    BMPInfoHeader* info_header = (BMPInfoHeader*)(file_data + sizeof(BMPHeader));
    info_header->header_size = sizeof(BMPInfoHeader);
    info_header->width = width;
    info_header->height = height;
    info_header->planes = 1;
    info_header->bits_per_pixel = 24;
    info_header->compression = 0;
    info_header->image_size = image_size;
    info_header->x_resolution = 0;
    info_header->y_resolution = 0;
    info_header->colors_used = 0;
    info_header->colors_important = 0;

    // 填充图像数据（BMP是底到顶存储）
    uint8_t* bmp_data = file_data + sizeof(BMPHeader) + sizeof(BMPInfoHeader);
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int src_index = (y * width + x) * 3;
            int dst_index = ((height - 1 - y) * row_size + x * 3);
            
            // BMP是BGR格式，转换成RGB
            bmp_data[dst_index] = pixel_data[src_index + 2];     // B
            bmp_data[dst_index + 1] = pixel_data[src_index + 1]; // G
            bmp_data[dst_index + 2] = pixel_data[src_index];     // R
        }
        
        // 填充行填充
        for (int x = width * 3; x < row_size; x++) {
            bmp_data[(height - 1 - y) * row_size + x] = 0;
        }
    }

    // 写入文件
    FILE* file = fopen(path, "wb");
    if (!file) {
        free(file_data);
        return -1;
    }

    size_t written = fwrite(file_data, 1, file_size, file);
    fclose(file);
    free(file_data);

    return (written == file_size) ? 0 : -1;
}

// 初始化可视化系统
int spatiotemporal_visualization_init() {
    // 在实际实现中，这里可以初始化图形库等
    return 0;
}

// 清理可视化系统
void spatiotemporal_visualization_cleanup() {
    // 在实际实现中，这里可以清理图形库等
}

// 创建可视化配置
VisualizationConfig* spatiotemporal_visualization_create_config(
    VisualizationType type,
    int width,
    int height
) {
    VisualizationConfig* config = (VisualizationConfig*)malloc(sizeof(VisualizationConfig));
    if (!config) return NULL;

    config->type = type;
    config->width = width;
    config->height = height;
    config->show_axes = true;
    config->show_grid = true;
    config->show_legend = true;
    config->title = strdup("Spatiotemporal Visualization");
    config->xlabel = strdup("X");
    config->ylabel = strdup("Y");
    config->zlabel = strdup("Z");
    config->output_format = strdup("bmp");

    return config;
}

// 销毁可视化配置
void spatiotemporal_visualization_destroy_config(VisualizationConfig* config) {
    if (!config) return;

    if (config->title) free(config->title);
    if (config->xlabel) free(config->xlabel);
    if (config->ylabel) free(config->ylabel);
    if (config->zlabel) free(config->zlabel);
    if (config->output_format) free(config->output_format);
    free(config);
}

// 设置颜色方案
void spatiotemporal_visualization_set_color_scheme(
    VisualizationConfig* config,
    ColorScheme scheme
) {
    if (!config) return;
    // 在实际实现中，这里会保存颜色方案设置
}

// 设置自定义颜色
void spatiotemporal_visualization_set_custom_color(
    VisualizationConfig* config,
    const Color* colors,
    size_t count
) {
    if (!config || !colors || count == 0) return;
    // 在实际实现中，这里会保存自定义颜色设置
}

// 生成2D散点图
int spatiotemporal_visualization_2d_scatter(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
) {
    if (!manager || dataset_id >= manager->dataset_count || !output_path || !config) {
        return -1;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    int width = config->width;
    int height = config->height;

    // 获取数据范围
    SpatiotemporalBounds bounds;
    if (spatiotemporal_dataset_get_bounds(manager, dataset_id, &bounds) != SPATIOTEMPORAL_SUCCESS) {
        return -1;
    }

    // 分配像素数据
    uint8_t* pixel_data = (uint8_t*)malloc(width * height * 3);
    if (!pixel_data) {
        return -1;
    }

    // 初始化为白色背景
    for (int i = 0; i < width * height * 3; i++) {
        pixel_data[i] = 255;
    }

    // 绘制网格
    if (config->show_grid) {
        for (int x = 0; x < width; x += width / 10) {
            for (int y = 0; y < height; y++) {
                pixel_data[(y * width + x) * 3] = 230;     // R
                pixel_data[(y * width + x) * 3 + 1] = 230; // G
                pixel_data[(y * width + x) * 3 + 2] = 230; // B
            }
        }
        for (int y = 0; y < height; y += height / 10) {
            for (int x = 0; x < width; x++) {
                pixel_data[(y * width + x) * 3] = 230;     // R
                pixel_data[(y * width + x) * 3 + 1] = 230; // G
                pixel_data[(y * width + x) * 3 + 2] = 230; // B
            }
        }
    }

    // 绘制坐标轴
    if (config->show_axes) {
        // X轴
        for (int x = 0; x < width; x++) {
            pixel_data[(height / 2 * width + x) * 3] = 0;     // R
            pixel_data[(height / 2 * width + x) * 3 + 1] = 0; // G
            pixel_data[(height / 2 * width + x) * 3 + 2] = 0; // B
        }
        // Y轴
        for (int y = 0; y < height; y++) {
            pixel_data[(y * width + width / 2) * 3] = 0;     // R
            pixel_data[(y * width + width / 2) * 3 + 1] = 0; // G
            pixel_data[(y * width + width / 2) * 3 + 2] = 0; // B
        }
    }

    // 绘制散点
    for (size_t i = 0; i < dataset->count; i++) {
        // 映射坐标到图像空间
        int x = (int)((dataset->points[i].coordinates[0] - bounds.min_coords[0]) / 
                     (bounds.max_coords[0] - bounds.min_coords[0]) * (width - 1));
        int y = (int)((1.0 - (dataset->points[i].coordinates[1] - bounds.min_coords[1]) / 
                     (bounds.max_coords[1] - bounds.min_coords[1])) * (height - 1));

        // 根据值确定颜色
        double value_normalized = (dataset->points[i].value - 0.0) / 100.0; // 假设值范围0-100
        value_normalized = value_normalized > 1.0 ? 1.0 : (value_normalized < 0.0 ? 0.0 : value_normalized);
        
        Color color = spatiotemporal_color_map(value_normalized, 0.0, 1.0, COLOR_SCHEME_RAINBOW);
        
        // 绘制点（5x5像素）
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = -2; dy <= 2; dy++) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    pixel_data[(py * width + px) * 3] = color.r;
                    pixel_data[(py * width + px) * 3 + 1] = color.g;
                    pixel_data[(py * width + px) * 3 + 2] = color.b;
                }
            }
        }
    }

    // 保存图像
    int result = create_bmp_image(output_path, width, height, pixel_data);
    free(pixel_data);
    return result;
}

// 生成3D散点图（简化实现，实际应使用3D库）
int spatiotemporal_visualization_3d_scatter(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
) {
    // 简化实现：将3D投影为2D
    return spatiotemporal_visualization_2d_scatter(manager, dataset_id, output_path, config);
}

// 生成热力图
int spatiotemporal_visualization_heatmap(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
) {
    if (!manager || dataset_id >= manager->dataset_count || !output_path || !config) {
        return -1;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    int width = config->width;
    int height = config->height;

    // 分配像素数据
    uint8_t* pixel_data = (uint8_t*)malloc(width * height * 3);
    if (!pixel_data) {
        return -1;
    }

    // 初始化为白色背景
    for (int i = 0; i < width * height * 3; i++) {
        pixel_data[i] = 255;
    }

    // 获取值范围
    double min_val = INFINITY, max_val = -INFINITY;
    for (size_t i = 0; i < dataset->count; i++) {
        if (dataset->points[i].value < min_val) min_val = dataset->points[i].value;
        if (dataset->points[i].value > max_val) max_val = dataset->points[i].value;
    }

    // 创建热力图网格
    int grid_size = 20; // 20x20网格
    double** grid = (double**)malloc(grid_size * sizeof(double*));
    for (int i = 0; i < grid_size; i++) {
        grid[i] = (double*)calloc(grid_size, sizeof(double));
    }

    // 将数据点分配到网格
    for (size_t i = 0; i < dataset->count; i++) {
        int grid_x = (int)((dataset->points[i].coordinates[0] - dataset->bounds.min_coords[0]) / 
                         (dataset->bounds.max_coords[0] - dataset->bounds.min_coords[0]) * (grid_size - 1));
        int grid_y = (int)((dataset->points[i].coordinates[1] - dataset->bounds.min_coords[1]) / 
                         (dataset->bounds.max_coords[1] - dataset->bounds.min_coords[1]) * (grid_size - 1));
        
        if (grid_x >= 0 && grid_x < grid_size && grid_y >= 0 && grid_y < grid_size) {
            grid[grid_y][grid_x] += dataset->points[i].value;
        }
    }

    // 绘制热力图
    for (int grid_y = 0; grid_y < grid_size; grid_y++) {
        for (int grid_x = 0; grid_x < grid_size; grid_x++) {
            if (grid[grid_y][grid_x] > 0) {
                double normalized = (grid[grid_y][grid_x] - min_val) / (max_val - min_val);
                normalized = normalized > 1.0 ? 1.0 : (normalized < 0.0 ? 0.0 : normalized);
                
                Color color = spatiotemporal_color_map(normalized, 0.0, 1.0, COLOR_SCHEME_HEAT);
                
                // 映射到图像空间
                int x1 = (int)((double)grid_x / grid_size * width);
                int x2 = (int)((double)(grid_x + 1) / grid_size * width);
                int y1 = (int)((double)grid_y / grid_size * height);
                int y2 = (int)((double)(grid_y + 1) / grid_size * height);
                
                // 填充矩形区域
                for (int y = y1; y < y2 && y < height; y++) {
                    for (int x = x1; x < x2 && x < width; x++) {
                        pixel_data[(y * width + x) * 3] = color.r;
                        pixel_data[(y * width + x) * 3 + 1] = color.g;
                        pixel_data[(y * width + x) * 3 + 2] = color.b;
                    }
                }
            }
        }
    }

    // 清理网格
    for (int i = 0; i < grid_size; i++) {
        free(grid[i]);
    }
    free(grid);

    // 保存图像
    int result = create_bmp_image(output_path, width, height, pixel_data);
    free(pixel_data);
    return result;
}

// 生成时间序列图
int spatiotemporal_visualization_time_series(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    const VisualizationConfig* config
) {
    if (!manager || dataset_id >= manager->dataset_count || !output_path || !config) {
        return -1;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    int width = config->width;
    int height = config->height;

    // 分配像素数据
    uint8_t* pixel_data = (uint8_t*)malloc(width * height * 3);
    if (!pixel_data) {
        return -1;
    }

    // 初始化为白色背景
    for (int i = 0; i < width * height * 3; i++) {
        pixel_data[i] = 255;
    }

    // 获取值范围和时间范围
    double min_val = INFINITY, max_val = -INFINITY;
    timestamp_t min_time = INT64_MAX, max_time = INT64_MIN;
    
    for (size_t i = 0; i < dataset->count; i++) {
        if (dataset->points[i].value < min_val) min_val = dataset->points[i].value;
        if (dataset->points[i].value > max_val) max_val = dataset->points[i].value;
        if (dataset->points[i].timestamp < min_time) min_time = dataset->points[i].timestamp;
        if (dataset->points[i].timestamp > max_time) max_time = dataset->points[i].timestamp;
    }

    // 按时间排序点
    SpatiotemporalPoint* sorted_points = (SpatiotemporalPoint*)malloc(dataset->count * sizeof(SpatiotemporalPoint));
    memcpy(sorted_points, dataset->points, dataset->count * sizeof(SpatiotemporalPoint));

    // 简单的时间戳排序
    for (size_t i = 0; i < dataset->count - 1; i++) {
        for (size_t j = i + 1; j < dataset->count; j++) {
            if (sorted_points[i].timestamp > sorted_points[j].timestamp) {
                SpatiotemporalPoint temp = sorted_points[i];
                sorted_points[i] = sorted_points[j];
                sorted_points[j] = temp;
            }
        }
    }

    // 绘制时间序列线
    for (size_t i = 0; i < dataset->count - 1; i++) {
        // 映射时间到X坐标
        double t1 = (double)(sorted_points[i].timestamp - min_time) / (max_time - min_time);
        double t2 = (double)(sorted_points[i + 1].timestamp - min_time) / (max_time - min_time);
        int x1 = (int)(t1 * width);
        int x2 = (int)(t2 * width);
        
        // 映射值到Y坐标
        double v1 = (sorted_points[i].value - min_val) / (max_val - min_val);
        double v2 = (sorted_points[i + 1].value - min_val) / (max_val - min_val);
        int y1 = (int)((1.0 - v1) * height);
        int y2 = (int)((1.0 - v2) * height);
        
        // 绘制线段
        if (x1 == x2) {
            // 垂直线
            int y_min = y1 < y2 ? y1 : y2;
            int y_max = y1 > y2 ? y1 : y2;
            for (int y = y_min; y <= y_max && y < height; y++) {
                pixel_data[(y * width + x1) * 3] = 0;
                pixel_data[(y * width + x1) * 3 + 1] = 0;
                pixel_data[(y * width + x1) * 3 + 2] = 255;
            }
        } else {
            // 使用Bresenham算法绘制斜线
            int dx = abs(x2 - x1);
            int dy = abs(y2 - y1);
            int sx = x1 < x2 ? 1 : -1;
            int sy = y1 < y2 ? 1 : -1;
            int err = dx - dy;
            
            int x = x1, y = y1;
            while (1) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    pixel_data[(y * width + x) * 3] = 0;
                    pixel_data[(y * width + x) * 3 + 1] = 0;
                    pixel_data[(y * width + x) * 3 + 2] = 255;
                }
                
                if (x == x2 && y == y2) break;
                
                int e2 = 2 * err;
                if (e2 > -dy) {
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) {
                    err += dx;
                    y += sy;
                }
            }
        }
    }

    // 绘制坐标轴
    if (config->show_axes) {
        // X轴（时间轴）
        for (int x = 0; x < width; x++) {
            pixel_data[(height - 50 * height / 100) * width * 3 + x * 3] = 0;
            pixel_data[(height - 50 * height / 100) * width * 3 + x * 3 + 1] = 0;
            pixel_data[(height - 50 * height / 100) * width * 3 + x * 3 + 2] = 0;
        }
        
        // Y轴（值轴）
        for (int y = 0; y < height; y++) {
            pixel_data[(y * width + 50 * width / 100) * 3] = 0;
            pixel_data[(y * width + 50 * width / 100) * 3 + 1] = 0;
            pixel_data[(y * width + 50 * width / 100) * 3 + 2] = 0;
        }
    }

    free(sorted_points);

    // 保存图像
    int result = create_bmp_image(output_path, width, height, pixel_data);
    free(pixel_data);
    return result;
}

// 其他可视化函数的实现可以类似地完成...

// 获取颜色映射值
Color spatiotemporal_color_map(double value, double min, double max, ColorScheme scheme) {
    if (max == min) max = min + 1.0; // 避免除零
    
    double normalized = (value - min) / (max - min);
    normalized = normalized > 1.0 ? 1.0 : (normalized < 0.0 ? 0.0 : normalized);
    
    Color color = {0, 0, 0, 255};
    
    switch (scheme) {
        case COLOR_SCHEME_RAINBOW: {
            int color_count = sizeof(RAINBOW_COLORS) / sizeof(Color);
            int index = (int)(normalized * (color_count - 1));
            double fraction = normalized * (color_count - 1) - index;
            
            if (index < color_count - 1) {
                Color c1 = RAINBOW_COLORS[index];
                Color c2 = RAINBOW_COLORS[index + 1];
                color.r = (uint8_t)(c1.r + (c2.r - c1.r) * fraction);
                color.g = (uint8_t)(c1.g + (c2.g - c1.g) * fraction);
                color.b = (uint8_t)(c1.b + (c2.b - c1.b) * fraction);
            } else {
                color = RAINBOW_COLORS[color_count - 1];
            }
            break;
        }
        case COLOR_SCHEME_HEAT: {
            int color_count = sizeof(HEAT_COLORS) / sizeof(Color);
            int index = (int)(normalized * (color_count - 1));
            double fraction = normalized * (color_count - 1) - index;
            
            if (index < color_count - 1) {
                Color c1 = HEAT_COLORS[index];
                Color c2 = HEAT_COLORS[index + 1];
                color.r = (uint8_t)(c1.r + (c2.r - c1.r) * fraction);
                color.g = (uint8_t)(c1.g + (c2.g - c1.g) * fraction);
                color.b = (uint8_t)(c1.b + (c2.b - c1.b) * fraction);
            } else {
                color = HEAT_COLORS[color_count - 1];
            }
            break;
        }
        case COLOR_SCHEME_COOL: {
            int color_count = sizeof(COOL_COLORS) / sizeof(Color);
            int index = (int)(normalized * (color_count - 1));
            double fraction = normalized * (color_count - 1) - index;
            
            if (index < color_count - 1) {
                Color c1 = COOL_COLORS[index];
                Color c2 = COOL_COLORS[index + 1];
                color.r = (uint8_t)(c1.r + (c2.r - c1.r) * fraction);
                color.g = (uint8_t)(c1.g + (c2.g - c1.g) * fraction);
                color.b = (uint8_t)(c1.b + (c2.b - c1.b) * fraction);
            } else {
                color = COOL_COLORS[color_count - 1];
            }
            break;
        }
        case COLOR_SCHEME_GRAYSCALE: {
            uint8_t gray = (uint8_t)(normalized * 255);
            color.r = color.g = color.b = gray;
            break;
        }
        case COLOR_SCHEME_CUSTOM:
        default:
            color.r = (uint8_t)(normalized * 255);
            color.g = (uint8_t)((1.0 - normalized) * 255);
            color.b = (uint8_t)(normalized * 128);
    }
    
    return color;
}

// 辅助函数：将RGB转换为十六进制颜色值
uint32_t spatiotemporal_rgb_to_hex(const Color* color) {
    return ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
}

// 辅助函数：将十六进制转换为RGB颜色
Color spatiotemporal_hex_to_rgb(uint32_t hex) {
    Color color;
    color.r = (hex >> 16) & 0xFF;
    color.g = (hex >> 8) & 0xFF;
    color.b = hex & 0xFF;
    color.a = 255;
    return color;
}

// 导出可视化数据为JSON格式
int spatiotemporal_export_visualization_data(
    const SpatiotemporalManager* manager,
    size_t dataset_id,
    const char* output_path,
    VisualizationType type
) {
    if (!manager || dataset_id >= manager->dataset_count || !output_path) {
        return -1;
    }

    const SpatiotemporalDataset* dataset = &manager->datasets[dataset_id];
    
    // 创建JSON文件
    FILE* file = fopen(output_path, "w");
    if (!file) {
        return -1;
    }

    // 写入JSON头部
    fprintf(file, "{\n");
    fprintf(file, "  \"type\": \"%s\",\n", 
            type == VIS_2D_SCATTER ? "2d_scatter" :
            type == VIS_3D_SCATTER ? "3d_scatter" :
            type == VIS_HEATMAP ? "heatmap" :
            type == VIS_TIME_SERIES ? "time_series" : "unknown");
    fprintf(file, "  \"dataset_id\": %zu,\n", dataset_id);
    fprintf(file, "  \"point_count\": %zu,\n", dataset->count);
    fprintf(file, "  \"bounds\": {\n");
    fprintf(file, "    \"min_coords\": [%.6f, %.6f, %.6f],\n", 
            dataset->bounds.min_coords[0], dataset->bounds.min_coords[1], dataset->bounds.min_coords[2]);
    fprintf(file, "    \"max_coords\": [%.6f, %.6f, %.6f],\n", 
            dataset->bounds.max_coords[0], dataset->bounds.max_coords[1], dataset->bounds.max_coords[2]);
    fprintf(file, "    \"min_timestamp\": %ld,\n", dataset->bounds.min_timestamp);
    fprintf(file, "    \"max_timestamp\": %ld\n", dataset->bounds.max_timestamp);
    fprintf(file, "  },\n");
    fprintf(file, "  \"points\": [\n");

    // 写入数据点
    for (size_t i = 0; i < dataset->count; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"coordinates\": [%.6f, %.6f, %.6f],\n", 
                dataset->points[i].coordinates[0], 
                dataset->points[i].coordinates[1], 
                dataset->points[i].coordinates[2]);
        fprintf(file, "      \"timestamp\": %ld,\n", dataset->points[i].timestamp);
        fprintf(file, "      \"value\": %.6f\n", dataset->points[i].value);
        
        if (i < dataset->count - 1) {
            fprintf(file, "    },\n");
        } else {
            fprintf(file, "    }\n");
        }
    }

    // 写入JSON尾部
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");

    fclose(file);
    return 0;
}