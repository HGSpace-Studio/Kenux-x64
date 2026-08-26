"""
KenuxK AI 框架 - 智能系统核心组件

这是一个轻量级、模块化的AI框架，专为KenuxK构建系统设计。
支持多种机器学习和深度学习任务，包括：
- 模型管理（保存、加载、版本控制）
- 模型训练（本地训练和分布式训练）
- 模型推理（批处理和实时推理）
- 模型评估（指标计算和分析）
- 数据预处理（特征工程和标准化）
"""

__version__ = "0.1.0"
__author__ = "KenuxK AI Team"

from .core import BaseModel, ModelManager
from .trainer import ModelTrainer, TrainingConfig, TrainerRegistry
from .inference import ModelInference, InferenceConfig, BatchProcessor
from .evaluation import ModelEvaluator, evaluate_model, compare_models
from .utils import Config, Logger, Timer, ensure_dir
from .models import LinearRegression, NeuralNetwork

# 注册默认的训练器
from .trainer.trainer_registry import trainer_registry

def setup_ai_framework():
    """设置AI框架，注册默认组件"""
    global model_manager, trainer_registry
    
    # 创建模型管理器
    model_manager = ModelManager()
    
    # 注册模型类型
    model_manager.register_model_type("linear_regression", LinearRegression)
    model_manager.register_model_type("neural_network", NeuralNetwork)
    
    print("AI框架初始化完成")
    return model_manager


# 初始化框架
model_manager = setup_ai_framework()

__all__ = [
    "BaseModel",
    "ModelManager", 
    "ModelTrainer",
    "TrainingConfig",
    "TrainerRegistry",
    "ModelInference",
    "InferenceConfig",
    "BatchProcessor",
    "ModelEvaluator",
    "evaluate_model",
    "compare_models",
    "Config",
    "Logger",
    "Timer",
    "ensure_dir",
    "LinearRegression",
    "NeuralNetwork",
    "model_manager",
    "setup_ai_framework"
]