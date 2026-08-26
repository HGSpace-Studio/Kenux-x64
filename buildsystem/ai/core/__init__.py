"""
AI 框架核心模块

包含模型的基础定义和模型管理功能。
"""

from .base_model import BaseModel
from .model_manager import ModelManager

__all__ = ["BaseModel", "ModelManager"]