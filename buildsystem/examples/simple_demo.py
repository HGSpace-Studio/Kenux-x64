"""
简化演示脚本

展示AI框架的基本功能。
"""

import sys
from pathlib import Path

# 添加项目路径
sys.path.insert(0, str(Path(__file__).parent.parent))

from core.model import BuildGraph, Target
from ai import Config
from ai.models.linear_regression import LinearRegression
from ai.models.neural_network import NeuralNetwork


def demonstrate_config():
    """演示配置功能"""
    print("演示配置功能...")
    
    # 加载配置
    config = Config()
    
    # 获取配置值
    lr = config.get("training.default_learning_rate", 0.001)
    bs = config.get("training.default_batch_size", 32)
    
    print(f"默认学习率: {lr}")
    print(f"默认批大小: {bs}")
    
    # 修改配置
    config.set("training.learning_rate", 0.01)
    new_lr = config.get("training.learning_rate")
    print(f"修改后的学习率: {new_lr}")
    
    return {"config_loaded": True}


def demonstrate_models():
    """演示模型创建"""
    print("\n演示模型创建...")
    
    # 创建线性回归模型
    lr_model = LinearRegression(
        model_name="linear_demo",
        config={"learning_rate": 0.01, "n_iterations": 100}
    )
    
    print(f"创建线性回归模型: {lr_model.model_name}")
    print(f"模型配置: {lr_model.config}")
    
    # 创建神经网络模型
    nn_model = NeuralNetwork(
        model_name="nn_demo",
        config={
            "layer_sizes": [64, 32, 1],
            "activation": "relu",
            "learning_rate": 0.001,
            "n_iterations": 200
        }
    )
    
    print(f"创建神经网络模型: {nn_model.model_name}")
    print(f"模型配置: {nn_model.config}")
    
    # 获取模型信息
    lr_info = lr_model.get_info()
    nn_info = nn_model.get_info()
    
    print("\n线性回归模型信息:")
    print(f"  {lr_info}")
    
    print("\n神经网络模型信息:")
    print(f"  {nn_info}")
    
    return {
        "linear_regression": {
            "name": lr_model.model_name,
            "config": lr_model.config
        },
        "neural_network": {
            "name": nn_model.model_name,
            "config": nn_model.config
        }
    }


def demonstrate_target_creation():
    """演示目标创建"""
    print("\n演示目标创建...")
    
    # 创建构建图
    root = Path(__file__).parent.parent
    graph = BuildGraph(root)
    
    # 创建一个简单的目标
    target = Target(
        name="demo_target",
        outputs=(root / "output" / "demo.txt",)
    )
    
    def demo_action(context):
        """演示动作"""
        output_file = context.paths.out / "demo.txt"
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, "w", encoding="utf-8") as f:
            f.write("Hello from AI framework demo!\n")
            f.write("This is a demonstration target.\n")
        
        print(f"目标执行完成: {output_file}")
        return True
    
    target.action = demo_action
    graph.add(target)
    
    print(f"创建目标: {target.name}")
    print(f"目标类型: {target.kind}")
    print(f"输出文件: {target.outputs}")
    
    return {"target_created": target.name}


def main():
    """主函数"""
    print("KenuxK AI框架简化演示")
    print("=" * 40)
    
    results = {}
    
    # 演示配置
    results["config"] = demonstrate_config()
    
    # 演示模型
    results["models"] = demonstrate_models()
    
    # 演示目标创建
    results["target"] = demonstrate_target_creation()
    
    # 总结
    print("\n" + "=" * 40)
    print("演示完成")
    
    print("\n结果总结:")
    for key, result in results.items():
        if isinstance(result, dict) and "error" in result:
            print(f"  {key}: 失败 - {result['error']}")
        else:
            print(f"  {key}: 成功")
    
    return results


if __name__ == "__main__":
    main()