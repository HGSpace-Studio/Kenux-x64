"""
简单测试脚本
"""

import sys
from pathlib import Path

# 添加项目路径
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

print("Testing basic imports...")

try:
    # 测试核心模块
    from core.model import BuildGraph, GraphError, Target
    from core.runner import BuildRunner, BuildSettings
    from core.state import BuildPaths
    print("Core modules imported successfully")
    
    # 测试AI模块
    from ai import Config, Dataset, LinearRegression, NeuralNetwork
    print("AI modules imported successfully")
    
    # 测试AI集成模块
    from ai_integration import AIModelTarget, AIBuildGraph
    print("AI integration modules imported successfully")
    
    print("All imports successful!")
    
except Exception as e:
    print(f"Import error: {e}")

if __name__ == "__main__":
    pass