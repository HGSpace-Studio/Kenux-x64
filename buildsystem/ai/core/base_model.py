"""
AI模型基类定义

提供所有AI模型的通用接口和基础功能。
"""

from abc import ABC, abstractmethod
from typing import Any, Dict, List, Optional, Tuple, Union
import json
import pickle
from pathlib import Path
import numpy as np


class BaseModel(ABC):
    """
    所有AI模型的基类，定义了模型的基本接口。
    
    子类需要实现以下方法：
    - train: 训练模型
    - predict: 进行预测
    - evaluate: 评估模型性能
    - save: 保存模型
    - load: 加载模型
    """
    
    def __init__(self, model_name: str, config: Optional[Dict[str, Any]] = None):
        """
        初始化模型
        
        Args:
            model_name: 模型名称
            config: 模型配置字典
        """
        self.model_name = model_name
        self.config = config or {}
        self.model_version = "1.0.0"
        self.training_history = []
        self.is_trained = False
        self.metadata = {
            "created_at": self._get_timestamp(),
            "last_updated": self._get_timestamp(),
            "training_samples": 0,
            "validation_samples": 0
        }
    
    def _get_timestamp(self) -> str:
        """获取当前时间戳"""
        from datetime import datetime
        return datetime.now().isoformat()
    
    def update_metadata(self, **kwargs) -> None:
        """更新模型元数据"""
        self.metadata.update(kwargs)
        self.metadata["last_updated"] = self._get_timestamp()
    
    def get_info(self) -> Dict[str, Any]:
        """获取模型信息"""
        return {
            "name": self.model_name,
            "version": self.model_version,
            "config": self.config,
            "is_trained": self.is_trained,
            "metadata": self.metadata,
            "training_history": self.training_history
        }
    
    @abstractmethod
    def train(self, train_data: Any, validation_data: Optional[Any] = None, 
              **kwargs) -> Dict[str, Any]:
        """
        训练模型
        
        Args:
            train_data: 训练数据
            validation_data: 验证数据（可选）
            **kwargs: 其他训练参数
            
        Returns:
            训练结果字典
        """
        pass
    
    @abstractmethod
    def predict(self, data: Any, **kwargs) -> Any:
        """
        进行预测
        
        Args:
            data: 输入数据
            **kwargs: 其他预测参数
            
        Returns:
            预测结果
        """
        pass
    
    @abstractmethod
    def evaluate(self, test_data: Any, **kwargs) -> Dict[str, float]:
        """
        评估模型性能
        
        Args:
            test_data: 测试数据
            **kwargs: 其他评估参数
            
        Returns:
            评估指标字典
        """
        pass
    
    @abstractmethod
    def save(self, path: Union[str, Path]) -> None:
        """
        保存模型到文件
        
        Args:
            path: 保存路径
        """
        pass
    
    @abstractmethod
    def load(self, path: Union[str, Path]) -> None:
        """
        从文件加载模型
        
        Args:
            path: 模型文件路径
        """
        pass
    
    def save_info(self, path: Union[str, Path]) -> None:
        """保存模型信息到JSON文件"""
        info = self.get_info()
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path.with_suffix(".info.json"), "w", encoding="utf-8") as f:
            json.dump(info, f, indent=2, ensure_ascii=False)
    
    def __str__(self) -> str:
        return f"BaseModel(name='{self.model_name}', version='{self.model_version}')"
    
    def __repr__(self) -> str:
        return self.__str__()