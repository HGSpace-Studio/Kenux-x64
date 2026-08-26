"""
模型训练器

负责模型的训练过程，包括训练、验证、早停、模型保存等功能。
"""

import time
import numpy as np
from typing import Dict, List, Any, Optional, Callable, Union, Tuple
from pathlib import Path
import json
import threading
import queue

from ..core.base_model import BaseModel
from .training_config import TrainingConfig
from .trainer_registry import TrainerRegistry


class ModelTrainer:
    """模型训练器类"""
    
    def __init__(self, 
                 model: BaseModel,
                 training_config: Optional[TrainingConfig] = None):
        """
        初始化训练器
        
        Args:
            model: 要训练的模型
            training_config: 训练配置
        """
        self.model = model
        self.config = training_config or TrainingConfig()
        
        # 训练历史
        self.training_history = {
            "train_loss": [],
            "train_metrics": [],
            "val_loss": [],
            "val_metrics": [],
            "epochs": [],
            "learning_rates": [],
            "timestamps": []
        }
        
        # 早停相关
        self.best_val_loss = float('inf')
        self.patience_counter = 0
        self.best_model_state = None
        
        # 回调函数
        self.callbacks: Dict[str, List[Callable]] = {
            "on_train_begin": [],
            "on_epoch_begin": [],
            "on_batch_begin": [],
            "on_batch_end": [],
            "on_epoch_end": [],
            "on_train_end": []
        }
        
        # 信号
        self.should_stop = False
        self._stop_event = threading.Event()
    
    def add_callback(self, event: str, callback: Callable) -> None:
        """
        添加回调函数
        
        Args:
            event: 回调事件
            callback: 回调函数
        """
        if event in self.callbacks:
            self.callbacks[event].append(callback)
        else:
            raise ValueError(f"未知的回调事件: {event}")
    
    def train(self, 
              train_data: Any,
              validation_data: Optional[Any] = None,
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
        # 调用训练开始回调
        self._trigger_callbacks("on_train_begin", {
            "model": self.model,
            "config": self.config,
            "train_data": train_data,
            "validation_data": validation_data
        })
        
        start_time = time.time()
        epoch = 0
        
        while epoch < self.config.max_epochs and not self.should_stop:
            self._stop_event.clear()
            
            # 训练一个epoch
            epoch_start = time.time()
            epoch += 1
            
            # 调用epoch开始回调
            self._trigger_callbacks("on_epoch_begin", {
                "epoch": epoch,
                "max_epochs": self.config.max_epochs,
                "model": self.model
            })
            
            # 训练
            train_result = self._train_epoch(train_data, epoch)
            
            # 验证
            val_result = None
            if validation_data is not None:
                val_result = self._validate(validation_data)
            
            # 记录训练历史
            current_time = time.time()
            self.training_history["epochs"].append(epoch)
            self.training_history["train_loss"].append(train_result.get("loss", 0))
            self.training_history["train_metrics"].append(train_result.get("metrics", {}))
            self.training_history["val_loss"].append(val_result.get("loss", 0) if val_result else 0)
            self.training_history["val_metrics"].append(val_result.get("metrics", {}) if val_result else {})
            self.training_history["learning_rates"].append(self.config.learning_rate)
            self.training_history["timestamps"].append(current_time)
            
            # 检查早停
            should_stop_early = False
            if val_result and self.config.early_stopping:
                should_stop_early = self._check_early_stopping(val_result["loss"])
            
            # 调用epoch结束回调
            epoch_info = {
                "epoch": epoch,
                "epoch_time": time.time() - epoch_start,
                "total_time": time.time() - start_time,
                "train_result": train_result,
                "val_result": val_result,
                "training_history": self.training_history,
                "should_stop": should_stop_early
            }
            
            self._trigger_callbacks("on_epoch_end", epoch_info)
            
            # 如果需要早停，停止训练
            if should_stop_early:
                print(f"早停触发，在epoch {epoch}停止训练")
                break
        
        # 调用训练结束回调
        train_info = {
            "model": self.model,
            "config": self.config,
            "training_history": self.training_history,
            "total_time": time.time() - start_time,
            "total_epochs": epoch,
            "stopped_early": self.should_stop or epoch >= self.config.max_epochs
        }
        
        self._trigger_callbacks("on_train_end", train_info)
        
        return {
            "training_history": self.training_history,
            "total_epochs": epoch,
            "total_time": time.time() - start_time,
            "best_val_loss": self.best_val_loss,
            "stopped_early": self.should_stop or epoch >= self.config.max_epochs
        }
    
    def _train_epoch(self, train_data: Any, epoch: int) -> Dict[str, Any]:
        """训练一个epoch"""
        epoch_loss = 0.0
        epoch_metrics = {}
        num_batches = 0
        
        # 这里假设train_data是一个可迭代的数据集
        # 实际实现需要根据具体的数据类型进行调整
        for batch_idx, batch in enumerate(train_data):
            if self.should_stop:
                break
            
            # 调用batch开始回调
            self._trigger_callbacks("on_batch_begin", {
                "batch_idx": batch_idx,
                "epoch": epoch,
                "batch_data": batch
            })
            
            # 训练一个batch
            batch_result = self._train_batch(batch)
            
            # 累加损失和指标
            epoch_loss += batch_result.get("loss", 0)
            for key, value in batch_result.get("metrics", {}).items():
                if key not in epoch_metrics:
                    epoch_metrics[key] = []
                epoch_metrics[key].append(value)
            
            num_batches += 1
            
            # 调用batch结束回调
            self._trigger_callbacks("on_batch_end", {
                "batch_idx": batch_idx,
                "epoch": epoch,
                "batch_result": batch_result,
                "epoch_result": {
                    "loss": epoch_loss / num_batches,
                    "metrics": self._calculate_average_metrics(epoch_metrics)
                }
            })
        
        # 计算平均损失和指标
        avg_loss = epoch_loss / num_batches if num_batches > 0 else 0
        avg_metrics = self._calculate_average_metrics(epoch_metrics)
        
        return {
            "loss": avg_loss,
            "metrics": avg_metrics,
            "num_batches": num_batches
        }
    
    def _train_batch(self, batch_data: Any) -> Dict[str, Any]:
        """训练一个batch"""
        # 这里需要根据具体的模型类型实现训练逻辑
        # 这是一个抽象方法，子类需要实现具体逻辑
        raise NotImplementedError("子类需要实现_train_batch方法")
    
    def _validate(self, validation_data: Any) -> Dict[str, Any]:
        """验证模型"""
        val_loss = 0.0
        val_metrics = {}
        num_batches = 0
        
        for batch in validation_data:
            if self.should_stop:
                break
            
            # 验证一个batch
            batch_result = self._validate_batch(batch)
            
            # 累加损失和指标
            val_loss += batch_result.get("loss", 0)
            for key, value in batch_result.get("metrics", {}).items():
                if key not in val_metrics:
                    val_metrics[key] = []
                val_metrics[key].append(value)
            
            num_batches += 1
        
        # 计算平均损失和指标
        avg_loss = val_loss / num_batches if num_batches > 0 else 0
        avg_metrics = self._calculate_average_metrics(val_metrics)
        
        return {
            "loss": avg_loss,
            "metrics": avg_metrics,
            "num_batches": num_batches
        }
    
    def _validate_batch(self, batch_data: Any) -> Dict[str, Any]:
        """验证一个batch"""
        # 这里需要根据具体的模型类型实现验证逻辑
        # 这是一个抽象方法，子类需要实现具体逻辑
        raise NotImplementedError("子类需要实现_validate_batch方法")
    
    def _calculate_average_metrics(self, metrics: Dict[str, List[float]]) -> Dict[str, float]:
        """计算指标的平均值"""
        avg_metrics = {}
        for key, values in metrics.items():
            avg_metrics[key] = sum(values) / len(values) if values else 0
        return avg_metrics
    
    def _check_early_stopping(self, val_loss: float) -> bool:
        """检查是否需要早停"""
        if val_loss < self.best_val_loss:
            self.best_val_loss = val_loss
            self.patience_counter = 0
            
            # 保存最佳模型状态
            self.best_model_state = self.model.get_state() if hasattr(self.model, 'get_state') else None
        else:
            self.patience_counter += 1
            if self.patience_counter >= self.config.patience:
                return True
        
        return False
    
    def _trigger_callbacks(self, event: str, info: Dict[str, Any]) -> None:
        """触发回调函数"""
        for callback in self.callbacks.get(event, []):
            try:
                callback(info)
            except Exception as e:
                print(f"回调函数执行失败 {event}: {e}")
    
    def stop_training(self) -> None:
        """停止训练"""
        self.should_stop = True
        self._stop_event.set()
    
    def save_training_history(self, save_path: Union[str, Path]) -> None:
        """保存训练历史到文件"""
        save_path = Path(save_path)
        save_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(save_path, "w", encoding="utf-8") as f:
            json.dump(self.training_history, f, indent=2, ensure_ascii=False)
    
    def load_training_history(self, load_path: Union[str, Path]) -> None:
        """从文件加载训练历史"""
        load_path = Path(load_path)
        
        if load_path.exists():
            with open(load_path, "r", encoding="utf-8") as f:
                self.training_history = json.load(f)
    
    def get_training_history(self) -> Dict[str, Any]:
        """获取训练历史"""
        return self.training_history
    
    def plot_training_history(self, save_path: Optional[Union[str, Path]] = None) -> None:
        """
        绘制训练历史图表
        
        Args:
            save_path: 保存路径（可选）
        """
        try:
            import matplotlib.pyplot as plt
            
            plt.figure(figsize=(12, 4))
            
            # 绘制损失曲线
            plt.subplot(1, 2, 1)
            plt.plot(self.training_history["epochs"], self.training_history["train_loss"], 
                    label="Training Loss", color="blue")
            plt.plot(self.training_history["epochs"], self.training_history["val_loss"], 
                    label="Validation Loss", color="red")
            plt.xlabel("Epoch")
            plt.ylabel("Loss")
            plt.title("Training and Validation Loss")
            plt.legend()
            
            # 绘制指标曲线（如果有）
            if self.training_history["train_metrics"]:
                plt.subplot(1, 2, 2)
                # 这里可以根据具体的指标进行绘制
                # 例如准确率、精确率等
                plt.xlabel("Epoch")
                plt.ylabel("Metric")
                plt.title("Training Metrics")
                
                # 添加示例指标（如果有多个指标，可以选择主要的）
                for key, values in self.training_history["train_metrics"][0].items():
                    if isinstance(values, (int, float)):
                        metric_values = [m.get(key, 0) for m in self.training_history["train_metrics"]]
                        plt.plot(self.training_history["epochs"], metric_values, label=key)
                plt.legend()
            
            plt.tight_layout()
            
            if save_path:
                plt.savefig(save_path)
                print(f"训练历史图表已保存: {save_path}")
            else:
                plt.show()
                
        except ImportError:
            print("未安装matplotlib，无法绘制训练历史图表")
    
    def resume_training(self, checkpoint_path: Union[str, Path]) -> None:
        """
        从检查点恢复训练
        
        Args:
            checkpoint_path: 检查点路径
        """
        checkpoint_path = Path(checkpoint_path)
        
        if checkpoint_path.exists():
            # 加载模型状态
            self.model.load(checkpoint_path)
            
            # 加载训练历史
            history_path = checkpoint_path.parent / f"{checkpoint_path.name}_history.json"
            if history_path.exists():
                self.load_training_history(history_path)
            
            print(f"已从检查点恢复训练: {checkpoint_path}")
        else:
            raise FileNotFoundError(f"检查点文件不存在: {checkpoint_path}")