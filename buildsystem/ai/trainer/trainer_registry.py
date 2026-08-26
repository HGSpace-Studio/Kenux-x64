"""
训练器注册表

用于注册和管理不同类型的训练器。
"""

from typing import Dict, Type, Any, Optional
from ..core.base_model import BaseModel


class TrainerRegistry:
    """训练器注册表类"""
    
    def __init__(self):
        """初始化注册表"""
        self._trainers: Dict[str, Type[Any]] = {}
        self._default_trainer = None
    
    def register_trainer(self, name: str, trainer_class: Type[Any], 
                        is_default: bool = False) -> None:
        """
        注册训练器
        
        Args:
            name: 训练器名称
            trainer_class: 训练器类
            is_default: 是否为默认训练器
        """
        self._trainers[name] = trainer_class
        
        if is_default:
            self._default_trainer = trainer_class
        
        print(f"已注册训练器: {name}")
    
    def get_trainer(self, name: str) -> Optional[Type[Any]]:
        """
        获取训练器类
        
        Args:
            name: 训练器名称
            
        Returns:
            训练器类，如果不存在则返回None
        """
        return self._trainers.get(name)
    
    def list_trainers(self) -> Dict[str, Type[Any]]:
        """列出所有注册的训练器"""
        return self._trainers.copy()
    
    def get_default_trainer(self) -> Optional[Type[Any]]:
        """获取默认训练器"""
        return self._default_trainer
    
    def create_trainer(self, 
                       name: Optional[str] = None,
                       model: Optional[BaseModel] = None,
                       config: Optional[Dict[str, Any]] = None) -> Any:
        """
        创建训练器实例
        
        Args:
            name: 训练器名称（如果为None则使用默认训练器）
            model: 模型实例
            config: 配置字典
            
        Returns:
            训练器实例
        """
        if name is None:
            if self._default_trainer is None:
                raise ValueError("没有设置默认训练器")
            trainer_class = self._default_trainer
        else:
            trainer_class = self.get_trainer(name)
            if trainer_class is None:
                raise ValueError(f"未知的训练器类型: {name}")
        
        # 创建训练器实例
        trainer = trainer_class(model, config)
        
        return trainer


# 全局训练器注册表实例
trainer_registry = TrainerRegistry()


def register_trainer(name: str, is_default: bool = False):
    """
    装饰器，用于注册训练器
    
    Args:
        name: 训练器名称
        is_default: 是否为默认训练器
    """
    def decorator(trainer_class):
        trainer_registry.register_trainer(name, trainer_class, is_default)
        return trainer_class
    return decorator