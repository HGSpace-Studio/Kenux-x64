"""
推理配置类

定义了模型推理的各种参数和配置选项。
"""

from dataclasses import dataclass, field
from typing import Optional, Dict, Any, List


@dataclass
class InferenceConfig:
    """
    推理配置类
    
    包含模型推理所需的各种参数，包括批处理、并行、缓存等配置。
    """
    
    # 批处理配置
    batch_size: int = 1
    max_batch_size: int = 32
    dynamic_batching: bool = True
    max_queue_size: int = 1000
    
    # 并行推理配置
    parallel_inference: bool = False
    max_workers: int = 4
    
    # 缓存配置
    cache_enabled: bool = False
    cache_size_limit: int = 1000
    cache_ttl: int = 3600  # 缓存过期时间（秒）
    
    # 推理设备配置
    device: str = "auto"  # "auto", "cpu", "cuda", "mps"
    precision: str = "float32"  # "float32", "float16", "bfloat16"
    quantize: bool = False
    quantization_mode: str = "dynamic"  # "dynamic", "static"
    
    # 性能配置
    enable_profiling: bool = False
    profile_iterations: int = 100
    memory_optimization: bool = True
    
    # 输入输出配置
    input_preprocessing: bool = True
    output_postprocessing: bool = True
    normalization: bool = True
    standardization: bool = False
    
    # 日志记录配置
    log_inference: bool = False
    log_level: str = "INFO"  # "DEBUG", "INFO", "WARNING", "ERROR"
    log_file: Optional[str] = None
    
    # 安全配置
    timeout: float = 30.0  # 推理超时时间（秒）
    max_input_size: Optional[int] = None  # 最大输入大小（字节）
    input_validation: bool = True
    
    # 特殊模型配置
    attention_masking: bool = True
    position_limit: Optional[int] = None  # 对于序列模型，限制序列长度
    
    def __post_init__(self):
        """初始化后处理"""
        # 自动选择设备
        if self.device == "auto":
            self.device = self._get_default_device()
        
        # 验证参数
        self._validate_parameters()
    
    def _get_default_device(self) -> str:
        """获取默认设备"""
        try:
            import torch
            if torch.cuda.is_available():
                return "cuda"
            elif hasattr(torch.backends, 'mps') and torch.backends.mps.is_available():
                return "mps"
        except ImportError:
            pass
        
        return "cpu"
    
    def _validate_parameters(self) -> None:
        """验证配置参数的有效性"""
        if self.batch_size <= 0:
            raise ValueError("batch_size 必须大于0")
        
        if self.max_batch_size < self.batch_size:
            raise ValueError("max_batch_size 必须大于等于 batch_size")
        
        if self.max_workers <= 0:
            raise ValueError("max_workers 必须大于0")
        
        if self.cache_size_limit <= 0:
            raise ValueError("cache_size_limit 必须大于0")
        
        if self.timeout <= 0:
            raise ValueError("timeout 必须大于0")
        
        if self.max_input_size is not None and self.max_input_size <= 0:
            raise ValueError("max_input_size 必须大于0")
        
        if self.profile_iterations <= 0:
            raise ValueError("profile_iterations 必须大于0")
        
        # 验证精度模式
        if self.precision not in ["float32", "float16", "bfloat16"]:
            raise ValueError("precision 必须是 'float32', 'float16' 或 'bfloat16'")
        
        # 验证量化模式
        if self.quantize and self.quantization_mode not in ["dynamic", "static"]:
            raise ValueError("quantization_mode 必须是 'dynamic' 或 'static'")
        
        # 验证日志级别
        if self.log_level not in ["DEBUG", "INFO", "WARNING", "ERROR"]:
            raise ValueError("log_level 必须是 'DEBUG', 'INFO', 'WARNING' 或 'ERROR'")
    
    def to_dict(self) -> Dict[str, Any]:
        """将配置转换为字典"""
        return {
            "batch_size": self.batch_size,
            "max_batch_size": self.max_batch_size,
            "dynamic_batching": self.dynamic_batching,
            "max_queue_size": self.max_queue_size,
            "parallel_inference": self.parallel_inference,
            "max_workers": self.max_workers,
            "cache_enabled": self.cache_enabled,
            "cache_size_limit": self.cache_size_limit,
            "cache_ttl": self.cache_ttl,
            "device": self.device,
            "precision": self.precision,
            "quantize": self.quantize,
            "quantization_mode": self.quantization_mode,
            "enable_profiling": self.enable_profiling,
            "profile_iterations": self.profile_iterations,
            "memory_optimization": self.memory_optimization,
            "input_preprocessing": self.input_preprocessing,
            "output_postprocessing": self.output_postprocessing,
            "normalization": self.normalization,
            "standardization": self.standardization,
            "log_inference": self.log_inference,
            "log_level": self.log_level,
            "log_file": self.log_file,
            "timeout": self.timeout,
            "max_input_size": self.max_input_size,
            "input_validation": self.input_validation,
            "attention_masking": self.attention_masking,
            "position_limit": self.position_limit
        }
    
    @classmethod
    def from_dict(cls, config_dict: Dict[str, Any]) -> "InferenceConfig":
        """从字典创建配置"""
        return cls(**config_dict)
    
    def save(self, save_path: str) -> None:
        """保存配置到文件"""
        import json
        with open(save_path, "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
    
    @classmethod
    def load(cls, load_path: str) -> "InferenceConfig":
        """从文件加载配置"""
        import json
        with open(load_path, "r", encoding="utf-8") as f:
            config_dict = json.load(f)
        return cls.from_dict(config_dict)
    
    def update(self, **kwargs) -> None:
        """更新配置参数"""
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)
        self._validate_parameters()
    
    def get_optimal_batch_size(self, input_size: int, memory_budget: int) -> int:
        """
        根据输入大小和内存预算计算最优批大小
        
        Args:
            input_size: 输入大小
            memory_budget: 内存预算（字节）
            
        Returns:
            最优批大小
        """
        # 估计每个样本的内存需求
        estimated_memory_per_sample = input_size * 8  # 假设每个字节用8位
        
        # 考虑模型参数和中间结果的额外内存
        overhead_factor = 3.0  # 估计有3倍的额外内存需求
        
        # 计算最大可能的批大小
        max_batch_size = int(memory_budget / (estimated_memory_per_sample * overhead_factor))
        
        # 返回配置值和计算值的较小值
        return min(self.batch_size, max_batch_size)
    
    def get_device_settings(self) -> Dict[str, Any]:
        """获取设备相关设置"""
        settings = {
            "device": self.device,
            "precision": self.precision,
            "memory_optimization": self.memory_optimization
        }
        
        if self.quantize:
            settings["quantize"] = True
            settings["quantization_mode"] = self.quantization_mode
        
        return settings
    
    def get_performance_settings(self) -> Dict[str, Any]:
        """获取性能相关设置"""
        return {
            "batch_size": self.batch_size,
            "max_batch_size": self.max_batch_size,
            "parallel_inference": self.parallel_inference,
            "max_workers": self.max_workers,
            "enable_profiling": self.enable_profiling,
            "profile_iterations": self.profile_iterations
        }
    
    def get_cache_settings(self) -> Dict[str, Any]:
        """获取缓存相关设置"""
        return {
            "cache_enabled": self.cache_enabled,
            "cache_size_limit": self.cache_size_limit,
            "cache_ttl": self.cache_ttl
        }