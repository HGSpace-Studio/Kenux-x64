"""
AI框架集成示例

展示如何将AI模型集成到KenuxK构建系统中，包括：
1. 创建AI构建图
2. 训练模型
3. 评估模型
4. 运行推理
"""

import sys
import os
from pathlib import Path

# 添加项目路径
sys.path.append(str(Path(__file__).parent.parent.parent))

from buildsystem import BuildPaths, BuildRunner, BuildSettings
from buildsystem.ai_integration import (
    create_ai_build_graph, 
    run_ai_training_task,
    setup_ai_build
)
from buildsystem.ai import (
    Dataset, 
    ModelManager,
    TrainingConfig,
    InferenceConfig
)


def example_1_basic_ai_training():
    """示例1：基本的AI训练任务"""
    print("=" * 60)
    print("示例1：基本的AI训练任务")
    print("=" * 60)
    
    # 设置构建路径
    build_paths = BuildPaths(
        root=Path(__file__).parent.parent,
        build="build",
        out="output",
        temp="temp"
    )
    
    # 创建训练配置
    training_config = {
        "learning_rate": 0.01,
        "n_iterations": 100,
        "batch_size": 16,
        "regularization": 0.01
    }
    
    # 创建数据集（示例数据）
    import numpy as np
    n_samples = 1000
    X = np.random.randn(n_samples, 5)
    y = X @ np.array([1.5, -2.0, 1.0, 0.5, -1.0]) + np.random.randn(n_samples) * 0.1
    
    # 保存示例数据
    import pandas as pd
    dataset_df = pd.DataFrame(X, columns=[f"feature_{i}" for i in range(5)])
    dataset_df["target"] = y
    dataset_path = build_paths.root / "sample_dataset.csv"
    dataset_df.to_csv(dataset_path, index=False)
    
    print(f"创建示例数据集: {dataset_path}")
    
    # 运行AI训练任务
    result = run_ai_training_task(
        paths=build_paths,
        task_name="linear_regression_demo",
        model_type="linear_regression",
        model_name="demo_model",
        dataset_path=dataset_path,
        training_config=training_config
    )
    
    print(f"训练结果: {result}")
    return result


def example_2_ai_pipeline():
    """示例2：AI流水线训练多个模型"""
    print("\n" + "=" * 60)
    print("示例2：AI流水线训练多个模型")
    print("=" * 60)
    
    # 设置构建路径
    build_paths = BuildPaths(
        root=Path(__file__).parent.parent,
        build="build",
        out="output",
        temp="temp"
    )
    
    # 创建模型配置列表
    model_configs = [
        {
            "model_type": "linear_regression",
            "training_config": {
                "learning_rate": 0.01,
                "n_iterations": 100,
                "batch_size": 16,
                "regularization": 0.01
            },
            "inference_config": {
                "batch_size": 32,
                "threshold": 0.5
            }
        },
        {
            "model_type": "neural_network",
            "training_config": {
                "learning_rate": 0.001,
                "n_iterations": 200,
                "batch_size": 32,
                "layer_sizes": [64, 32, 1],
                "activation": "relu"
            },
            "inference_config": {
                "batch_size": 32,
                "threshold": 0.5
            }
        }
    ]
    
    # 创建示例数据
    import numpy as np
    import pandas as pd
    n_samples = 1000
    X = np.random.randn(n_samples, 5)
    y = X @ np.array([1.5, -2.0, 1.0, 0.5, -1.0]) + np.random.randn(n_samples) * 0.1
    dataset_df = pd.DataFrame(X, columns=[f"feature_{i}" for i in range(5)])
    dataset_df["target"] = y
    
    # 保存数据集
    dataset_path = build_paths.root / "sample_dataset_pipeline.csv"
    dataset_df.to_csv(dataset_path, index=False)
    
    # 设置AI构建
    runner, graph = setup_ai_build(
        paths=build_paths,
        graph_name="ai_pipeline_demo",
        dataset_path=dataset_path,
        model_configs=model_configs
    )
    
    print(f"创建AI构建图，包含 {len(graph.targets)} 个目标")
    print("目标列表:")
    for target in graph.targets.values():
        print(f"  - {target.name} ({target.kind})")
    
    # 运行流水线
    print("\n运行AI流水线...")
    targets_to_run = [t for t in graph.targets.values() if t.kind in ["ai_train"]]
    metrics = runner.run(targets_to_run, "ai_pipeline_demo")
    
    print(f"流水线执行完成，结果: {metrics}")
    return metrics


def example_3_custom_ai_target():
    """示例3：自定义AI目标"""
    print("\n" + "=" * 60)
    print("示例3：自定义AI目标")
    print("=" * 60)
    
    # 导入需要的模块
    from buildsystem.ai_integration import AIBuildGraph, AIModelTarget
    from buildsystem.ai import Dataset, ModelTrainer
    from buildsystem import BuildPaths
    
    # 设置构建路径
    build_paths = BuildPaths(
        root=Path(__file__).parent.parent,
        build="build",
        out="output",
        temp="temp"
    )
    
    # 创建AI构建图
    graph = AIBuildGraph(build_paths.root)
    
    # 创建数据集
    import numpy as np
    import pandas as pd
    n_samples = 500
    X = np.random.randn(n_samples, 3)
    y = (X @ np.array([1.0, -0.5, 0.8]) > 0).astype(int)  # 二分类
    dataset_df = pd.DataFrame(X, columns=["feature_1", "feature_2", "feature_3"])
    dataset_df["target"] = y
    
    dataset_path = build_paths.root / "classification_dataset.csv"
    dataset_df.to_csv(dataset_path, index=False)
    
    # 添加自定义AI模型目标
    target = graph.add_ai_model_target(
        name="custom_classification_model",
        model_type="linear_regression",
        model_name="binary_classifier",
        dataset_path=dataset_path,
        training_config={
            "learning_rate": 0.01,
            "n_iterations": 100,
            "batch_size": 16
        },
        action="train"
    )
    
    print(f"创建自定义AI目标: {target.name}")
    
    # 运行自定义目标
    from buildsystem import BuildRunner, BuildSettings
    
    settings = BuildSettings.automatic()
    runner = BuildRunner(graph, build_paths, settings, "custom_target_demo")
    
    print("运行自定义AI目标...")
    metrics = runner.run((target,), "custom_target_demo")
    
    print(f"自定义目标执行完成，结果: {metrics}")
    return metrics


def example_4_model_management():
    """示例4：模型管理"""
    print("\n" + "=" * 60)
    print("示例4：模型管理")
    print("=" * 60)
    
    # 创建模型管理器
    model_manager = ModelManager()
    
    # 创建训练配置
    from buildsystem.ai import TrainingConfig
    training_config = TrainingConfig(
        learning_rate=0.01,
        n_iterations=100,
        batch_size=32,
        regularization=0.01
    )
    
    # 创建两个模型
    from buildsystem.ai import LinearRegression, NeuralNetwork
    
    lr_model = LinearRegression(
        model_name="regression_model_1",
        config={"learning_rate": 0.01}
    )
    
    nn_model = NeuralNetwork(
        model_name="nn_model_1",
        config={
            "layer_sizes": [64, 32, 1],
            "activation": "relu",
            "learning_rate": 0.001
        }
    )
    
    # 注册模型
    model_manager.register_model("regression_model_1", lr_model)
    model_manager.register_model("nn_model_1", nn_model)
    
    print(f"已注册 {len(model_manager.models)} 个模型")
    print(f"模型列表: {list(model_manager.models.keys())}")
    
    # 创建示例数据
    import numpy as np
    n_samples = 100
    X = np.random.randn(n_samples, 3)
    y = X @ np.array([1.5, -2.0, 1.0]) + np.random.randn(n_samples) * 0.1
    
    # 训练模型
    print("\n训练模型...")
    train_data = (X, y)
    
    lr_result = lr_model.train(train_data, validation_data=None, **training_config.to_dict())
    print(f"线性回归训练完成，最终损失: {lr_result['loss']:.6f}")
    
    nn_result = nn_model.train(train_data, validation_data=None, **training_config.to_dict())
    print(f"神经网络训练完成，最终损失: {nn_result['loss']:.6f}")
    
    # 保存模型
    model_dir = Path("output/models")
    model_dir.mkdir(parents=True, exist_ok=True)
    
    lr_model.save(model_dir / "linear_regression")
    nn_model.save(model_dir / "neural_network")
    
    print(f"模型已保存到: {model_dir}")
    
    # 加载模型
    loaded_lr = LinearRegression("loaded_model")
    loaded_lr.load(model_dir / "linear_regression")
    
    print(f"模型加载成功，名称: {loaded_lr.model_name}")
    
    return {
        "linear_regression": lr_result,
        "neural_network": nn_result
    }


def main():
    """主函数：运行所有示例"""
    print("KenuxK AI框架集成示例")
    print("=" * 60)
    
    # 确保输出目录存在
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # 运行示例
    results = {}
    
    try:
        results["basic_training"] = example_1_basic_ai_training()
    except Exception as e:
        print(f"示例1执行失败: {e}")
        results["basic_training"] = {"error": str(e)}
    
    try:
        results["pipeline"] = example_2_ai_pipeline()
    except Exception as e:
        print(f"示例2执行失败: {e}")
        results["pipeline"] = {"error": str(e)}
    
    try:
        results["custom_target"] = example_3_custom_ai_target()
    except Exception as e:
        print(f"示例3执行失败: {e}")
        results["custom_target"] = {"error": str(e)}
    
    try:
        results["model_management"] = example_4_model_management()
    except Exception as e:
        print(f"示例4执行失败: {e}")
        results["model_management"] = {"error": str(e)}
    
    # 保存结果
    from buildsystem.ai.utils import save_json
    save_json(results, "output/ai_examples_results.json")
    
    print("\n" + "=" * 60)
    print("所有示例执行完成")
    print("=" * 60)
    
    return results


if __name__ == "__main__":
    main()