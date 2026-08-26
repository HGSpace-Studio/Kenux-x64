"""
AI工具模块

提供配置管理、日志记录等工具函数。
"""

import json
import logging
import os
import pickle
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Union


class Config:
    """配置管理类"""
    
    def __init__(self, config_file: Optional[Union[str, Path]] = None):
        """
        初始化配置
        
        Args:
            config_file: 配置文件路径
        """
        self.config_data: Dict[str, Any] = {}
        self.config_file = Path(config_file) if config_file else None
        
        # 加载默认配置
        self._load_default_config()
        
        # 如果指定了配置文件，加载配置
        if self.config_file and self.config_file.exists():
            self.load_config()
    
    def _load_default_config(self) -> None:
        """加载默认配置"""
        self.config_data = {
            # AI框架配置
            "ai_framework": {
                "model_dir": "models",
                "dataset_dir": "datasets",
                "log_dir": "logs",
                "max_model_versions": 5,
                "auto_cleanup": True
            },
            
            # 训练配置
            "training": {
                "default_batch_size": 32,
                "default_learning_rate": 0.001,
                "default_epochs": 100,
                "patience": 10,
                "min_delta": 0.001,
                "early_stopping": True,
                "save_best_only": True,
                "validation_split": 0.2
            },
            
            # 数据处理配置
            "data_processing": {
                "test_size": 0.2,
                "val_size": 0.1,
                "random_state": 42,
                "stratify": True,
                "normalize_features": True,
                "normalize_targets": False,
                "feature_range": (0, 1),
                "standard_scaler": "standard"
            },
            
            # 模型评估配置
            "evaluation": {
                "cv_folds": 5,
                "scoring": "accuracy",
                "plot_curves": True,
                "save_predictions": True,
                "output_dir": "evaluations"
            },
            
            # 日志配置
            "logging": {
                "level": "INFO",
                "format": "%(asctime)s - %(name)s - %(levelname)s - %(message)s",
                "file": "ai_framework.log",
                "max_size": "10MB",
                "backup_count": 5
            },
            
            # 推理配置
            "inference": {
                "batch_size": 32,
                "threshold": 0.5,
                "confidence_interval": 0.95,
                "save_results": True,
                "output_format": "json"
            }
        }
    
    def load_config(self, config_file: Optional[Union[str, Path]] = None) -> None:
        """
        从文件加载配置
        
        Args:
            config_file: 配置文件路径，如果为None则使用默认路径
        """
        if config_file:
            self.config_file = Path(config_file)
        
        if not self.config_file or not self.config_file.exists():
            raise FileNotFoundError(f"配置文件不存在: {self.config_file}")
        
        with open(self.config_file, 'r', encoding='utf-8') as f:
            file_config = json.load(f)
        
        # 合并配置
        self._merge_config(file_config)
        
        print(f"配置已加载: {self.config_file}")
    
    def save_config(self, config_file: Optional[Union[str, Path]] = None) -> None:
        """
        保存配置到文件
        
        Args:
            config_file: 配置文件路径，如果为None则使用默认路径
        """
        if config_file:
            self.config_file = Path(config_file)
        
        if not self.config_file:
            raise ValueError("未指定配置文件路径")
        
        # 确保目录存在
        self.config_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(self.config_file, 'w', encoding='utf-8') as f:
            json.dump(self.config_data, f, indent=2, ensure_ascii=False)
        
        print(f"配置已保存: {self.config_file}")
    
    def _merge_config(self, new_config: Dict[str, Any]) -> None:
        """合并配置"""
        def deep_update(base_dict, update_dict):
            for key, value in update_dict.items():
                if isinstance(value, dict) and key in base_dict and isinstance(base_dict[key], dict):
                    deep_update(base_dict[key], value)
                else:
                    base_dict[key] = value
        
        deep_update(self.config_data, new_config)
    
    def get(self, key: str, default: Any = None) -> Any:
        """
        获取配置值
        
        Args:
            key: 配置键，支持点号分隔的嵌套键
            default: 默认值
            
        Returns:
            配置值
        """
        keys = key.split('.')
        value = self.config_data
        
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default
        
        return value
    
    def set(self, key: str, value: Any) -> None:
        """
        设置配置值
        
        Args:
            key: 配置键，支持点号分隔的嵌套键
            value: 配置值
        """
        keys = key.split('.')
        config = self.config_data
        
        for k in keys[:-1]:
            if k not in config:
                config[k] = {}
            config = config[k]
        
        config[keys[-1]] = value
    
    def update(self, updates: Dict[str, Any]) -> None:
        """
        批量更新配置
        
        Args:
            updates: 配置更新字典
        """
        self._merge_config(updates)
    
    def to_dict(self) -> Dict[str, Any]:
        """返回配置字典的副本"""
        return self.config_data.copy()
    
    def __getitem__(self, key: str) -> Any:
        """支持字典式访问"""
        return self.get(key)
    
    def __setitem__(self, key: str, value: Any) -> None:
        """支持字典式设置"""
        self.set(key, value)


class Logger:
    """日志记录器"""
    
    def __init__(self, 
                 name: str = "AI_Framework",
                 log_file: Optional[Union[str, Path]] = None,
                 level: str = "INFO",
                 format_str: Optional[str] = None):
        """
        初始化日志记录器
        
        Args:
            name: 日志记录器名称
            log_file: 日志文件路径
            level: 日志级别
            format_str: 日志格式
        """
        self.name = name
        self.logger = logging.getLogger(name)
        self.logger.setLevel(getattr(logging, level.upper()))
        
        # 清除现有的处理器
        self.logger.handlers.clear()
        
        # 设置格式
        if format_str is None:
            format_str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
        
        formatter = logging.Formatter(format_str)
        
        # 控制台处理器
        console_handler = logging.StreamHandler()
        console_handler.setFormatter(formatter)
        self.logger.addHandler(console_handler)
        
        # 文件处理器
        if log_file:
            log_file = Path(log_file)
            log_file.parent.mkdir(parents=True, exist_ok=True)
            
            file_handler = logging.FileHandler(log_file, encoding='utf-8')
            file_handler.setFormatter(formatter)
            self.logger.addHandler(file_handler)
    
    def debug(self, message: str, *args, **kwargs) -> None:
        """记录调试信息"""
        self.logger.debug(message, *args, **kwargs)
    
    def info(self, message: str, *args, **kwargs) -> None:
        """记录信息"""
        self.logger.info(message, *args, **kwargs)
    
    def warning(self, message: str, *args, **kwargs) -> None:
        """记录警告"""
        self.logger.warning(message, *args, **kwargs)
    
    def error(self, message: str, *args, **kwargs) -> None:
        """记录错误"""
        self.logger.error(message, *args, **kwargs)
    
    def critical(self, message: str, *args, **kwargs) -> None:
        """记录严重错误"""
        self.logger.critical(message, *args, **kwargs)
    
    def set_level(self, level: str) -> None:
        """设置日志级别"""
        self.logger.setLevel(getattr(logging, level.upper()))


class Timer:
    """计时器工具类"""
    
    def __init__(self, name: str = "Timer"):
        """
        初始化计时器
        
        Args:
            name: 计时器名称
        """
        self.name = name
        self.start_time = None
        self.end_time = None
    
    def start(self) -> None:
        """开始计时"""
        self.start_time = time.time()
        self.end_time = None
        print(f"计时器 '{self.name}' 开始")
    
    def stop(self) -> float:
        """停止计时并返回耗时（秒）"""
        if self.start_time is None:
            raise RuntimeError("计时器未开始")
        
        self.end_time = time.time()
        elapsed = self.end_time - self.start_time
        print(f"计时器 '{self.name}' 结束，耗时: {elapsed:.4f} 秒")
        return elapsed
    
    def elapsed(self) -> float:
        """返回已用时间（秒），如果计时器未停止则返回当前时间"""
        if self.start_time is None:
            raise RuntimeError("计时器未开始")
        
        end_time = self.end_time if self.end_time else time.time()
        return end_time - self.start_time
    
    def __enter__(self):
        """上下文管理器入口"""
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器出口"""
        self.stop()


def ensure_dir(path: Union[str, Path]) -> Path:
    """
    确保目录存在
    
    Args:
        path: 目录路径
        
    Returns:
        目录路径对象
    """
    path = Path(path)
    path.mkdir(parents=True, exist_ok=True)
    return path


def save_json(data: Any, file_path: Union[str, Path], indent: int = 2, **kwargs) -> None:
    """
    保存数据为JSON文件
    
    Args:
        data: 要保存的数据
        file_path: 文件路径
        indent: 缩进
        **kwargs: 其他传递给json.dump的参数
    """
    file_path = Path(file_path)
    file_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(file_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=indent, ensure_ascii=False, **kwargs)
    
    print(f"数据已保存为JSON: {file_path}")


def load_json(file_path: Union[str, Path]) -> Any:
    """
    从JSON文件加载数据
    
    Args:
        file_path: 文件路径
        
    Returns:
        加载的数据
    """
    file_path = Path(file_path)
    
    if not file_path.exists():
        raise FileNotFoundError(f"文件不存在: {file_path}")
    
    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    return data


def save_pickle(data: Any, file_path: Union[str, Path]) -> None:
    """
    保存数据为pickle文件
    
    Args:
        data: 要保存的数据
        file_path: 文件路径
    """
    file_path = Path(file_path)
    file_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(file_path, 'wb') as f:
        pickle.dump(data, f)
    
    print(f"数据已保存为pickle: {file_path}")


def load_pickle(file_path: Union[str, Path]) -> Any:
    """
    从pickle文件加载数据
    
    Args:
        file_path: 文件路径
        
    Returns:
        加载的数据
    """
    file_path = Path(file_path)
    
    if not file_path.exists():
        raise FileNotFoundError(f"文件不存在: {file_path}")
    
    with open(file_path, 'rb') as f:
        data = pickle.load(f)
    
    return data


def get_timestamp() -> str:
    """获取当前时间戳字符串"""
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def format_size(size_bytes: int) -> str:
    """
    格式化文件大小
    
    Args:
        size_bytes: 字节数
        
    Returns:
        格式化后的大小字符串
    """
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} PB"


def calculate_hash(data: Any, algorithm: str = "md5") -> str:
    """
    计算数据哈希值
    
    Args:
        data: 要计算哈希的数据
        algorithm: 哈希算法
        
    Returns:
        哈希字符串
    """
    import hashlib
    
    if isinstance(data, str):
        data = data.encode('utf-8')
    elif not isinstance(data, bytes):
        data = str(data).encode('utf-8')
    
    hasher = hashlib.new(algorithm)
    hasher.update(data)
    return hasher.hexdigest()