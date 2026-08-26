# KenuxK AI框架集成指南

本指南介绍如何将AI模型集成到KenuxK构建系统中。

## 快速开始

### 1. 基本用法

```python
from buildsystem import BuildPaths, BuildRunner, BuildSettings
from buildsystem.ai_integration import create_ai_build_graph, run_ai_training_task
from buildsystem.ai import Dataset, TrainingConfig
import pandas as pd
import numpy as np

# 设置构建路径
build_paths = BuildPaths(
    root=Path("your_project"),
    build="build",
    out="output",
    temp="temp"
)

# 创建示例数据
X = np.random.randn(100, 5)
y = X @ np.array([1.5, -2.0, 1.0, 0.5, -1.0]) + np.random.randn(100) * 0.1
dataset_df = pd.DataFrame(X, columns=[f"feature_{i}" for i in range(5)])
dataset_df["target"] = y
dataset_path = build_paths.root / "dataset.csv"
dataset_df.to_csv(dataset_path, index=False)

# 运行AI训练任务
result = run_ai_training_task(
    paths=build_paths,
    task_name="my_model",
    model_type="linear_regression",
    model_name="demo_model",
    dataset_path=dataset_path,
    training_config={
        "learning_rate": 0.01,
        "n_iterations": 100,
        "batch_size": 32
    }
)
```

### 2. 创建AI流水线

```python
from buildsystem.ai_integration import setup_ai_build

# 定义模型配置
model_configs = [
    {
        "model_type": "linear_regression",
        "training_config": {
            "learning_rate": 0.01,
            "n_iterations": 100
        }
    },
    {
        "model_type": "neural_network",
        "training_config": {
            "learning_rate": 0.001,
            "n_iterations": 200,
            "layer_sizes": [64, 32, 1]
        }
    }
]

# 设置AI构建
runner, graph = setup_ai_build(
    paths=build_paths,
    graph_name="my_pipeline",
    dataset_path=dataset_path,
    model_configs=model_configs
)

# 运行流水线
targets_to_run = [t for t in graph.targets.values() if t.kind == "ai_train"]
metrics = runner.run(targets_to_run, "my_pipeline")
```

### 3. 自定义AI目标

```python
from buildsystem.ai_integration import AIBuildGraph, AIModelTarget

# 创建AI构建图
graph = AIBuildGraph(build_paths.root)

# 添加自定义AI模型目标
target = graph.add_ai_model_target(
    name="my_custom_model",
    model_type="neural_network",
    model_name="custom_nn",
    dataset_path=dataset_path,
    training_config={
        "learning_rate": 0.001,
        "n_iterations": 200,
        "layer_sizes": [128, 64, 32, 1]
    },
    action="train"
)

# 运行自定义目标
runner = BuildRunner(graph, build_paths, BuildSettings.automatic(), "custom_demo")
metrics = runner.run((target,), "custom_demo")
```

## 核心概念

### 1. AI模型目标 (AIModelTarget)

`AIModelTarget` 是构建系统中的特殊目标类型，用于AI模型训练、评估和推理。

```python
target = AIModelTarget(
    name="model_train",
    model_type="linear_regression",
    model_name="my_model",
    dataset_path="data.csv",
    training_config={"learning_rate": 0.01},
    action="train"
)
```

### 2. AI构建图 (AIBuildGraph)

`AIBuildGraph` 扩展了标准的构建图，提供了AI模型的专门支持。

```python
graph = AIBuildGraph(root_path)
targets = graph.create_ai_pipeline(
    dataset_path="data.csv",
    model_configs=[config1, config2]
)
```

### 3. 模型管理器 (ModelManager)

`ModelManager` 负责管理所有AI模型的创建、注册和检索。

```python
from buildsystem.ai import ModelManager, LinearRegression, NeuralNetwork

model_manager = ModelManager()
model_manager.register_model_type("linear_regression", LinearRegression)
model_manager.register_model_type("neural_network", NeuralNetwork)
```

## 配置文件

AI框架使用JSON配置文件进行配置。示例配置文件位于 `buildsystem/examples/ai_config.json`。

要使用配置文件：

```python
from buildsystem.ai import Config

config = Config("path/to/your/config.json")
learning_rate = config.get("training.default_learning_rate", 0.001)
```

## 数据集处理

框架提供了 `Dataset` 类来处理数据：

```python
from buildsystem.ai import Dataset

dataset = Dataset("my_dataset", data_path="data.csv", target_column="target")
dataset.load_data()
dataset.split_data(test_size=0.2, val_size=0.1)
dataset.normalize_features()

train_data = dataset.get_train_data()
test_data = dataset.get_test_data()
```

## 模型评估

框架提供了多种评估工具：

```python
from buildsystem.ai import ModelEvaluator, evaluate_model

# 评估单个模型
metrics = evaluate_model(model, X_test, y_test, task_type="regression")

# 评估多个模型
comparison = compare_models(
    {"model1": model1, "model2": model2},
    X_test, y_test,
    task_type="classification"
)
```

## 支持的模型类型

### 1. 线性回归 (LinearRegression)
- 适合回归任务
- 支持L2正则化
- 提供置信区间预测

### 2. 神经网络 (NeuralNetwork)
- 支持多层感知机
- 可配置网络结构
- 支持多种激活函数
- 使用Adam优化器

## 输出结构

AI训练任务的输出结构：

```
output/
├── [model_name]_model/
│   ├── model.pkl          # 模型文件
│   ├── model_info.json    # 模型信息
│   └── training_result.json  # 训练结果
├── [model_name]_evaluation/
│   └── evaluation_result.json  # 评估结果
└── [model_name]_inference/
    └── predictions.json   # 推理结果
```

## 故障排除

### 常见问题

1. **导入错误**
   - 确保所有依赖已安装：`pip install numpy pandas scikit-learn`
   - 检查Python路径

2. **数据加载错误**
   - 确保CSV文件存在且格式正确
   - 检查目标列名称

3. **模型训练失败**
   - 检查数据维度是否匹配
   - 验证超参数配置

### 调试模式

启用详细日志：

```python
import logging
logging.basicConfig(level=logging.DEBUG)

# 或使用配置文件
config = Config("config.json")
config.set("logging.level", "DEBUG")
```

## 高级用法

### 自定义模型

要添加自定义模型类型：

```python
from buildsystem.ai.core.base_model import BaseModel

class CustomModel(BaseModel):
    def __init__(self, model_name: str, config: Dict = None):
        super().__init__(model_name, config)
        # 初始化代码
    
    def train(self, train_data, **kwargs):
        # 训练代码
        pass
    
    def predict(self, data, **kwargs):
        # 预测代码
        pass

# 注册模型
model_manager.register_model_type("custom_model", CustomModel)
```

### 批处理推理

```python
from buildsystem.ai import ModelInference, InferenceConfig

# 创建推理配置
inference_config = InferenceConfig(
    batch_size=64,
    threshold=0.5
)

# 创建推理器
inference = ModelInference(model, inference_config)

# 批量预测
batch_predictions = inference.predict_batch(test_data)
```

## 性能优化

1. **批处理训练**
   - 使用较大的batch_size以提高内存效率
   - 对于大数据集，考虑使用批处理生成器

2. **早停机制**
   - 启用早停以避免过拟合
   - 调整patience和min_delta参数

3. **模型并行**
   - 对于多个模型，使用流水线并行训练

## 贡献

欢迎贡献代码和改进建议！请遵循以下步骤：

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 创建Pull Request

## 许可证

MIT License