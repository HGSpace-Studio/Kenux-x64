"""
基本AI框架使用示例

展示如何在构建系统中使用AI框架，不依赖外部库。
"""

import sys
from pathlib import Path

# 添加项目路径
sys.path.insert(0, str(Path(__file__).parent.parent))

from core.model import BuildGraph, Target
from core.runner import BuildRunner, BuildSettings
from core.state import BuildPaths
from ai import Config
from ai.models.linear_regression import LinearRegression
from core.ai_integration import AIBuildGraph, AIModelTarget


def create_basic_ai_project():
    """创建基本的AI项目"""
    print("创建基本的AI项目...")
    
    # 设置构建路径
    build_paths = BuildPaths(root=Path(__file__).parent.parent)
    
    # 创建构建图
    graph = BuildGraph(build_paths.root)
    
    # 添加一个简单的目标
    target = Target("build_project", outputs=("output/project.txt",))
    
    def build_action(context):
        """构建动作"""
        output_file = context.paths.out / "project.txt"
        with open(output_file, "w") as f:
            f.write("Project built successfully!\n")
        print(f"Project built: {output_file}")
    
    target.action = build_action
    graph.add(target)
    
    # 运行构建
    runner = BuildRunner(graph, build_paths, BuildSettings.automatic(), "basic_project")
    metrics = runner.run((target,), "basic_project")
    
    print("基本项目创建完成")
    return metrics


def create_ai_model_example():
    """创建AI模型示例"""
    print("\n创建AI模型示例...")
    
    # 设置构建路径
    build_paths = BuildPaths(root=Path(__file__).parent.parent)
    
    # 创建AI构建图
    graph = AIBuildGraph(build_paths.root)
    
    # 创建模型配置
    model_config = {
        "model_type": "linear_regression",
        "model_name": "example_model",
        "training_config": {
            "learning_rate": 0.01,
            "n_iterations": 100,
            "batch_size": 32
        }
    }
    
    # 添加AI模型目标
    ai_target = AIModelTarget(
        name="train_example_model",
        model_type=model_config["model_type"],
        model_name=model_config["model_name"],
        training_config=model_config["training_config"],
        action="train"
    )
    
    graph.add(ai_target)
    
    print(f"AI模型目标创建: {ai_target.name}")
    
    # 运行构建
    runner = BuildRunner(graph, build_paths, BuildSettings.automatic(), "ai_model_example")
    metrics = runner.run((ai_target,), "ai_model_example")
    
    print("AI模型示例创建完成")
    return metrics


def demonstrate_model_creation():
    """演示模型创建"""
    print("\n演示模型创建...")
    
    # 加载配置
    config = Config()
    learning_rate = config.get("training.default_learning_rate", 0.001)
    
    # 创建线性回归模型
    model = LinearRegression(
        model_name="demo_model",
        config={"learning_rate": learning_rate}
    )
    
    print(f"创建模型: {model.model_name}")
    print(f"配置: {model.config}")
    
    # 模拟训练数据（在实际应用中应该从文件加载）
    print("注意：这里没有实际训练数据，所以不会执行训练")
    
    # 模拟预测
    test_data = [[1.0, 2.0, 3.0]]
    if model.is_trained:
        predictions = model.predict(test_data)
        print(f"预测结果: {predictions}")
    else:
        print("模型尚未训练，无法进行预测")
    
    return {
        "model_name": model.model_name,
        "is_trained": model.is_trained,
        "config": model.config
    }


def main():
    """主函数"""
    print("KenuxK AI框架基本使用示例")
    print("=" * 50)
    
    results = {}
    
    try:
        # 创建基本项目
        results["basic_project"] = create_basic_ai_project()
    except Exception as e:
        print(f"基本项目创建失败: {e}")
        results["basic_project"] = {"error": str(e)}
    
    try:
        # 创建AI模型示例
        results["ai_model"] = create_ai_model_example()
    except Exception as e:
        print(f"AI模型创建失败: {e}")
        results["ai_model"] = {"error": str(e)}
    
    try:
        # 演示模型创建
        results["model_creation"] = demonstrate_model_creation()
    except Exception as e:
        print(f"模型演示失败: {e}")
        results["model_creation"] = {"error": str(e)}
    
    # 总结
    print("\n" + "=" * 50)
    print("示例执行完成")
    
    for key, result in results.items():
        if isinstance(result, dict) and "error" in result:
            print(f"{key}: 失败 - {result['error']}")
        else:
            print(f"{key}: 成功")
    
    return results


if __name__ == "__main__":
    main()