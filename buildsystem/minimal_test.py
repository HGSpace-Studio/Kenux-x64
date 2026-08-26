"""
最小化测试脚本 - 不依赖外部库
"""

import sys
from pathlib import Path

# 添加项目路径
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

print("Testing minimal imports...")

try:
    # 测试核心模块
    from core.model import BuildGraph, GraphError, Target
    print("Core modules imported successfully")
    
    # 创建简单的目标
    target = Target("test_target", outputs=())
    print(f"Created target: {target.name}")
    
    # 创建简单的构建图
    graph = BuildGraph(project_root)
    added_target = graph.add(target)
    print(f"Added target to graph: {added_target.name}")
    
    # 测试AI基本模块（不依赖pandas）
    from ai import Config
    config = Config()
    lr = config.get("training.default_learning_rate")
    print(f"Default learning rate: {lr}")
    
    # 测试模型类
    from ai.models.linear_regression import LinearRegression
    lr_model = LinearRegression("test_model", {"learning_rate": 0.01})
    print(f"Created model: {lr_model.model_name}")
    
    print("All basic functionality works!")
    
except Exception as e:
    print(f"Error: {e}")
    import traceback
    traceback.print_exc()

print("Minimal test completed.")