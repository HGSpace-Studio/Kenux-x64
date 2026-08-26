# Spaitotemporal Composability

一个强大的时空数据管理和分析C库，专门用于处理时空数据的存储、分析和可视化。

## 功能特性

### 🗂️ 数据管理
- 多数据集管理
- 动态数据点添加
- 高效的时空数据索引
- 边界自动计算

### 📊 分析算法
- **空间聚类** (K-means算法)
- **时间趋势分析** (线性回归)
- **时空相关性分析** (空间自相关、时间自相关)
- **异常检测** (Z-score方法)

### 🎨 可视化功能
- 2D/3D散点图
- 热力图（空间、时间、时空）
- 时间序列图
- 数据导出（JSON格式）

### 🔧 系统集成
- 轻量级API设计
- 模块化架构
- 错误处理机制
- 内存管理

## 快速开始

### 1. 编译安装

```bash
# 进入spatiotemporal目录
cd spatiotemporal

# 编译静态库
make

# 编译动态库
make shared

# 编译示例程序
make examples

# 运行测试
make test
```

### 2. 基本使用

```c
#include "spatiotemporal.h"
#include <stdio.h>

int main() {
    // 初始化API
    if (spatiotemporal_api_init() != STAPI_OK) {
        printf("初始化失败\n");
        return 1;
    }
    
    // 创建数据集
    size_t dataset_id;
    if (spatiotemporal_create_dataset(&dataset_id) != STAPI_OK) {
        printf("创建数据集失败\n");
        return 1;
    }
    
    // 添加数据点
    spatiotemporal_add_point(dataset_id, 10.5, 20.3, 0.0, 1640995200, 25.5);
    
    // 生成可视化
    spatiotemporal_plot_2d_scatter(dataset_id, "output.bmp", 800, 600);
    
    // 清理资源
    spatiotemporal_api_cleanup();
    
    return 0;
}
```

## API 参考

### 核心函数

| 函数 | 描述 |
|------|------|
| `spatiotemporal_api_init()` | 初始化API |
| `spatiotemporal_api_cleanup()` | 清理资源 |
| `spatiotemporal_create_dataset()` | 创建数据集 |
| `spatiotemporal_add_point()` | 添加数据点 |
| `spatiotemporal_get_dataset_info()` | 获取数据集信息 |

### 分析功能

| 函数 | 描述 |
|------|------|
| `spatiotemporal_cluster_dataset()` | 空间聚类分析 |
| `spatiotemporal_analyze_trends()` | 时间趋势分析 |
| `spatiotemporal_analyze_correlation()` | 时空相关性分析 |
| `spatiotemporal_detect_anomalies()` | 异常检测 |

### 可视化功能

| 函数 | 描述 |
|------|------|
| `spatiotemporal_plot_2d_scatter()` | 2D散点图 |
| `spatiotemporal_plot_heatmap()` | 热力图 |
| `spatiotemporal_plot_time_series()` | 时间序列图 |
| `spatiotemporal_export_data()` | 数据导出 |

## 数据结构

### SpatiotemporalPoint
```c
typedef struct {
    spatial_coord_t coordinates[SPATIAL_DIMENSIONS];  // x, y, z 坐标
    timestamp_t timestamp;                           // 时间戳
    value_t value;                                   // 关联值
} SpatiotemporalPoint;
```

### SpatiotemporalBounds
```c
typedef struct {
    spatial_coord_t min_coords[SPATIAL_DIMENSIONS];  // 最小坐标
    spatial_coord_t max_coords[SPATIAL_DIMENSIONS];  // 最大坐标
    timestamp_t min_timestamp;                       // 最小时间
    timestamp_t max_timestamp;                       // 最大时间
} SpatiotemporalBounds;
```

## 项目结构

```
spatiotemporal/
├── README.md                 # 项目说明
├── Makefile                  # 编译配置
├── spatiotemporal.h          # 统一API头文件
├── spatiotemporal_types.h    # 基本数据类型定义
├── spatiotemporal_core.h     # 核心数据管理接口
├── spatiotemporal_core.c     # 核心数据管理实现
├── spatiotemporal_analysis.h # 分析算法接口
├── spatiotemporal_analysis.c # 分析算法实现
├── spatiotemporal_visualization.h # 可视化接口
├── spatiotemporal_visualization.c # 可视化实现
├── spatiotemporal_api.c     # 高级API实现
├── examples/                # 示例程序
│   ├── basic_usage.c        # 基本使用示例
│   ├── visualization_demo.c  # 可视化演示
│   └── analysis_demo.c      # 分析算法演示
└── tests/                   # 测试程序
    ├── test_core.c          # 核心功能测试
    └── test_analysis.c      # 分析算法测试
```

## 集成到现有系统

要将Spaitotemporal Composability集成到现有的操作系统中：

1. **包含头文件**
```c
#include "spatiotemporal.h"
```

2. **编译时链接**
```bash
gcc your_app.c -L/path/to/spatiotemporal -lspatiotemporal -o your_app
```

3. **运行时设置库路径**
```bash
export LD_LIBRARY_PATH=/path/to/spatiotemporal:$LD_LIBRARY_PATH
```

## 系统要求

- 编译器：GCC 4.8+ 或 Clang 3.8+
- 操作系统：Linux/Windows/macOS
- 依赖库：无（纯C实现）

## 性能特点

- **内存效率**：动态内存分配，支持大数据集
- **计算效率**：优化的算法实现
- **可扩展性**：模块化设计，易于扩展
- **跨平台**：纯C实现，支持多平台

## 示例应用场景

1. **智能交通监控**
   - 车辆位置和时间轨迹分析
   - 交通流量预测
   - 异常交通事件检测

2. **环境监测**
   - 温度、湿度时空分布分析
   - 环境污染源追踪
   - 气候趋势预测

3. **物联网数据处理**
   - 传感器数据时空分析
   - 设备状态监控
   - 异常行为检测

## 贡献指南

欢迎提交Issue和Pull Request来改进这个项目。

## 许可证

MIT License

## 联系方式

如有问题或建议，请提交Issue或联系项目维护者。