"""
训练配置类

定义了模型训练的各种参数和配置选项。
"""

from dataclasses import dataclass, field
from typing import Optional, Dict, Any, List
import numpy as np


@dataclass
class TrainingConfig:
    """
    训练配置类
    
    包含模型训练所需的各种参数，学习率调度器优化器选项等。
    """
    
    # 基本训练参数
    max_epochs: int = 100
    batch_size: int = 32
    learning_rate: float = 0.001
    weight_decay: float = 0.0
    
    # 优化器配置
    optimizer: str = "adam"
    optimizer_params: Dict[str, Any] = field(default_factory=lambda: {
        "beta1": 0.9,
        "beta2": 0.999,
        "eps": 1e-8
    })
    
    # 学习率调度
    lr_scheduler: Optional[str] = None
    lr_scheduler_params: Dict[str, Any] = field(default_factory=lambda: {
        "step_size": 10,
        "gamma": 0.1
    })
    
    # 正则化
    dropout_rate: float = 0.0
    l1_lambda: float = 0.0
    l2_lambda: float = 0.0
    
    # 早停配置
    early_stopping: bool = False
    patience: int = 10
    min_delta: float = 0.0
    
    # 数据增强
    data_augmentation: bool = False
    augmentation_params: Dict[str, Any] = field(default_factory=dict)
    
    # 保存和恢复
    save_best_only: bool = True
    save_checkpoint_every: int = 10  # 每隔多少个epoch保存检查点
    checkpoint_dir: str = "checkpoints"
    
    # 验证
    validation_split: float = 0.2
    validation_frequency: int = 1  # 每隔多少个epoch进行一次验证
    
    # 评估指标
    metrics: List[str] = field(default_factory=lambda: ["loss", "accuracy"])
    
    # 并行训练
    use_multiprocessing: bool = False
    num_workers: int = 4
    
    # 混合精度训练
    mixed_precision: bool = False
    
    # 分布式训练
    distributed_training: bool = False
    backend: str = "nccl"
    
    # 设备配置
    device: str = "auto"  # "auto", "cpu", "cuda", "mps"
    
    # 日志记录
    log_frequency: int = 10  # 每隔多少个batch记录一次日志
    tensorboard_dir: str = "tensorboard"
    
    # 实验配置
    experiment_name: str = "default"
    run_name: str = None  # 如果为None，则使用时间戳
    
    def __post_init__(self):
        """初始化后处理"""
        # 自动生成run_name
        if self.run_name is None:
            from datetime import datetime
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            self.run_name = f"{self.experiment_name}_{timestamp}"
        
        # 设置默认设备
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
        if self.max_epochs <= 0:
            raise ValueError("max_epochs 必须大于0")
        
        if self.batch_size <= 0:
            raise ValueError("batch_size 必须大于0")
        
        if self.learning_rate <= 0:
            raise ValueError("learning_rate 必须大于0")
        
        if not (0 <= self.validation_split < 1):
            raise ValueError("validation_split 必须在0到1之间")
        
        if self.num_workers < 0:
            raise ValueError("num_workers 不能为负数")
        
        if self.patience < 0:
            raise ValueError("patience 不能为负数")
        
        if self.log_frequency <= 0:
            raise ValueError("log_frequency 必须大于0")
        
        if self.validation_frequency <= 0:
            raise ValueError("validation_frequency 必须大于0")
        
        if self.save_checkpoint_every <= 0:
            raise ValueError("save_checkpoint_every 必须大于0")
    
    def to_dict(self) -> Dict[str, Any]:
        """将配置转换为字典"""
        return {
            "max_epochs": self.max_epochs,
            "batch_size": self.batch_size,
            "learning_rate": self.learning_rate,
            "weight_decay": self.weight_decay,
            "optimizer": self.optimizer,
            "optimizer_params": self.optimizer_params,
            "lr_scheduler": self.lr_scheduler,
            "lr_scheduler_params": self.lr_scheduler_params,
            "dropout_rate": self.dropout_rate,
            "l1_lambda": self.l1_lambda,
            "l2_lambda": self.l2_lambda,
            "early_stopping": self.early_stopping,
            "patience": self.patience,
            "min_delta": self.min_delta,
            "data_augmentation": self.data_augmentation,
            "augmentation_params": self.augmentation_params,
            "save_best_only": self.save_best_only,
            "save_checkpoint_every": self.save_checkpoint_every,
            "checkpoint_dir": self.checkpoint_dir,
            "validation_split": self.validation_split,
            "validation_frequency": self.validation_frequency,
            "metrics": self.metrics,
            "use_multiprocessing": self.use_multiprocessing,
            "num_workers": self.num_workers,
            "mixed_precision": self.mixed_precision,
            "distributed_training": self.distributed_training,
            "backend": self.backend,
            "device": self.device,
            "log_frequency": self.log_frequency,
            "tensorboard_dir": self.tensorboard_dir,
            "experiment_name": self.experiment_name,
            "run_name": self.run_name
        }
    
    @classmethod
    def from_dict(cls, config_dict: Dict[str, Any]) -> "TrainingConfig":
        """从字典创建配置"""
        return cls(**config_dict)
    
    def save(self, save_path: str) -> None:
        """保存配置到文件"""
        import json
        with open(save_path, "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
    
    @classmethod
    def load(cls, load_path: str) -> "TrainingConfig":
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
    
    def get_optimizer_params(self) -> Dict[str, Any]:
        """获取优化器参数"""
        params = {
            "lr": self.learning_rate,
            "weight_decay": self.weight_decay
        }
        
        # 添加优化器特定参数
        if self.optimizer.lower() == "adam":
            params.update({
                "betas": (
                    self.optimizer_params.get("beta1", 0.9),
                    self.optimizer_params.get("beta2", 0.999)
                ),
                "eps": self.optimizer_params.get("eps", 1e-8)
            })
        elif self.optimizer.lower() == "sgd":
            params.update({
                "momentum": self.optimizer_params.get("momentum", 0.9),
                "dampening": self.optimizer_params.get("dampening", 0.0),
                "nesterov": self.optimizer_params.get("nesterov", False)
            })
        elif self.optimizer.lower() == "rmsprop":
            params.update({
                "rho": self.optimizer_params.get("rho", 0.9),
                "eps": self.optimizer_params.get("eps", 1e-8)
            })
        
        return params
    
    def get_lr_scheduler_params(self) -> Dict[str, Any]:
        """获取学习率调度器参数"""
        if self.lr_scheduler is None:
            return {}
        
        params = self.lr_scheduler_params.copy()
        
        if self.lr_scheduler.lower() == "step":
            params["step_size"] = params.get("step_size", 10)
            params["gamma"] = params.get("gamma", 0.1)
        elif self.lr_scheduler.lower() == "multistep":
            params["milestones"] = params.get("milestones", [30, 60, 90])
            params["gamma"] = params.get("gamma", 0.1)
        elif self.lr_scheduler.lower() == "exponential":
            params["gamma"] = params.get("gamma", 0.95)
        elif self.lr_scheduler.lower() == "cosine":
            params["T_max"] = params.get("T_max", 100)
            params["eta_min"] = params.get("eta_min", 0)
        
        return params