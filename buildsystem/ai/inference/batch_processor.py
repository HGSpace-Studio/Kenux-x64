"""
批处理器

负责将输入数据分割成批次，并进行批处理操作。
"""

import numpy as np
from typing import Any, Dict, List, Optional, Union, Tuple
from collections import deque
import threading
import time
from queue import Queue


class BatchProcessor:
    """批处理器类"""
    
    def __init__(self, 
                 default_batch_size: int = 32,
                 max_batch_size: int = 128,
                 queue_size: int = 1000):
        """
        初始化批处理器
        
        Args:
            default_batch_size: 默认批处理大小
            max_batch_size: 最大批处理大小
            queue_size: 队列大小
        """
        self.default_batch_size = default_batch_size
        self.max_batch_size = max_batch_size
        self.queue_size = queue_size
        
        # 数据队列
        self.data_queue = Queue(maxsize=queue_size)
        
        # 当前批大小
        self.current_batch_size = default_batch_size
        
        # 队列锁
        self._queue_lock = threading.Lock()
        
        # 统计信息
        self.stats = {
            "total_batches": 0,
            "total_items": 0,
            "avg_batch_size": 0.0,
            "queue_sizes": []
        }
    
    def create_batches(self, 
                      data: Any, 
                      batch_size: Optional[int] = None) -> List[Any]:
        """
        创建批次
        
        Args:
            data: 输入数据
            batch_size: 批处理大小（可选）
            
        Returns:
            批次列表
        """
        batch_size = batch_size or self.default_batch_size
        
        if isinstance(data, np.ndarray):
            return self._create_numpy_batches(data, batch_size)
        elif isinstance(data, list):
            return self._create_list_batches(data, batch_size)
        elif isinstance(data, (tuple, dict)):
            return self._create_generic_batches(data, batch_size)
        else:
            # 尝试将数据转换为列表
            try:
                data_list = list(data)
                return self._create_list_batches(data_list, batch_size)
            except:
                # 如果无法转换，作为单个批次处理
                return [data]
    
    def _create_numpy_batches(self, 
                             data: np.ndarray, 
                             batch_size: int) -> List[np.ndarray]:
        """创建NumPy数组的批次"""
        total_samples = len(data)
        num_batches = (total_samples + batch_size - 1) // batch_size
        
        batches = []
        for i in range(num_batches):
            start_idx = i * batch_size
            end_idx = min((i + 1) * batch_size, total_samples)
            batch = data[start_idx:end_idx]
            batches.append(batch)
        
        return batches
    
    def _create_list_batches(self, 
                            data: List[Any], 
                            batch_size: int) -> List[List[Any]]:
        """创建列表的批次"""
        total_samples = len(data)
        num_batches = (total_samples + batch_size - 1) // batch_size
        
        batches = []
        for i in range(num_batches):
            start_idx = i * batch_size
            end_idx = min((i + 1) * batch_size, total_samples)
            batch = data[start_idx:end_idx]
            batches.append(batch)
        
        return batches
    
    def _create_generic_batches(self, 
                               data: Union[Tuple, Dict], 
                               batch_size: int) -> List[Any]:
        """创建通用数据类型的批次"""
        # 这里可以根据具体的数据类型实现不同的批处理逻辑
        # 目前简单地将数据作为单个批次处理
        return [data]
    
    def add_to_queue(self, data: Any) -> None:
        """
        将数据添加到队列
        
        Args:
            data: 输入数据
        """
        self.data_queue.put(data)
    
    def get_from_queue(self, timeout: Optional[float] = None) -> Optional[Any]:
        """
        从队列获取数据
        
        Args:
            timeout: 超时时间（秒）
            
        Returns:
            数据项，如果超时则返回None
        """
        try:
            return self.data_queue.get(timeout=timeout)
        except:
            return None
    
    def get_batch_from_queue(self, 
                            batch_size: Optional[int] = None,
                            timeout: Optional[float] = None) -> List[Any]:
        """
        从队列获取一个批次的数据
        
        Args:
            batch_size: 批处理大小
            timeout: 超时时间（秒）
            
        Returns:
            批次数据列表
        """
        batch_size = batch_size or self.default_batch_size
        batch = []
        
        # 非阻塞方式获取数据
        for _ in range(batch_size):
            data = self.get_from_queue(timeout=timeout)
            if data is None:
                break
            batch.append(data)
        
        return batch
    
    def queue_size(self) -> int:
        """获取队列大小"""
        return self.data_queue.qsize()
    
    def is_empty(self) -> bool:
        """检查队列是否为空"""
        return self.data_queue.empty()
    
    def clear_queue(self) -> None:
        """清空队列"""
        with self._queue_lock:
            while not self.data_queue.empty():
                try:
                    self.data_queue.get_nowait()
                except:
                    break
    
    def update_batch_size(self, batch_size: int) -> None:
        """
        更新批处理大小
        
        Args:
            batch_size: 新的批处理大小
        """
        if 1 <= batch_size <= self.max_batch_size:
            self.current_batch_size = batch_size
            print(f"批处理大小已更新为: {batch_size}")
        else:
            raise ValueError(f"批处理大小必须在1到{self.max_batch_size}之间")
    
    def optimize_batch_size(self, 
                           performance_stats: Dict[str, Any]) -> int:
        """
        根据性能统计优化批处理大小
        
        Args:
            performance_stats: 性能统计信息
            
        Returns:
            优化后的批处理大小
        """
        # 这里可以根据具体的性能指标来优化批处理大小
        # 例如，根据GPU内存使用率、推理时间等
        
        # 简单的启发式优化策略
        avg_inference_time = performance_stats.get("avg_inference_time", 0)
        
        if avg_inference_time > 1.0:  # 如果推理时间过长，减小批处理大小
            new_batch_size = max(1, self.current_batch_size // 2)
        elif avg_inference_time < 0.1 and self.current_batch_size < self.max_batch_size:  # 如果推理时间很短，增大批处理大小
            new_batch_size = min(self.max_batch_size, self.current_batch_size * 2)
        else:
            new_batch_size = self.current_batch_size
        
        self.update_batch_size(new_batch_size)
        return new_batch_size
    
    def process_batches(self, 
                       processor_func: callable,
                       batch_size: Optional[int] = None) -> List[Any]:
        """
        处理队列中的所有批次
        
        Args:
            processor_func: 处理函数
            batch_size: 批处理大小
            
        Returns:
            处理结果列表
        """
        results = []
        
        while not self.is_empty():
            batch = self.get_batch_from_queue(batch_size)
            if batch:
                # 处理批次
                batch_result = processor_func(batch)
                results.append(batch_result)
                
                # 更新统计信息
                self._update_stats(len(batch))
        
        return results
    
    def _update_stats(self, batch_size: int) -> None:
        """更新统计信息"""
        with self._queue_lock:
            self.stats["total_batches"] += 1
            self.stats["total_items"] += batch_size
            
            # 计算平均批大小
            self.stats["avg_batch_size"] = (
                self.stats["total_items"] / self.stats["total_batches"]
            )
            
            # 记录队列大小
            self.stats["queue_sizes"].append(self.queue_size())
            
            # 限制队列大小历史记录
            if len(self.stats["queue_sizes"]) > 1000:
                self.stats["queue_sizes"] = self.stats["queue_sizes"][-1000:]
    
    def get_stats(self) -> Dict[str, Any]:
        """获取统计信息"""
        with self._queue_lock:
            return self.stats.copy()
    
    def reset_stats(self) -> None:
        """重置统计信息"""
        with self._queue_lock:
            self.stats = {
                "total_batches": 0,
                "total_items": 0,
                "avg_batch_size": 0.0,
                "queue_sizes": []
            }
    
    def get_queue_info(self) -> Dict[str, Any]:
        """获取队列信息"""
        return {
            "queue_size": self.queue_size(),
            "max_queue_size": self.queue_size,
            "is_empty": self.is_empty(),
            "current_batch_size": self.current_batch_size,
            "max_batch_size": self.max_batch_size
        }


class DynamicBatchProcessor(BatchProcessor):
    """动态批处理器"""
    
    def __init__(self, 
                 default_batch_size: int = 32,
                 max_batch_size: int = 128,
                 queue_size: int = 1000,
                 adaptive_batching: bool = True):
        """
        初始化动态批处理器
        
        Args:
            default_batch_size: 默认批处理大小
            max_batch_size: 最大批处理大小
            queue_size: 队列大小
            adaptive_batching: 是否启用自适应批处理
        """
        super().__init__(default_batch_size, max_batch_size, queue_size)
        self.adaptive_batching = adaptive_batching
        
        # 自适应批处理参数
        self.min_batch_size = 1
        self.batch_adaptation_rate = 0.1
        self.performance_window = 100  # 性能评估窗口大小
        
        # 性能历史记录
        self.performance_history = deque(maxlen=self.performance_window)
        
        # 自适应批处理锁
        self._adaptation_lock = threading.Lock()
    
    def create_adaptive_batches(self, 
                              data: List[Any],
                              size_func: Optional[callable] = None) -> List[List[Any]]:
        """
        创建自适应批次
        
        Args:
            data: 输入数据列表
            size_func: 计算数据大小的函数
            
        Returns:
            自适应批次列表
        """
        if not self.adaptive_batching:
            return self.create_batches(data)
        
        batches = []
        current_batch = []
        current_size = 0
        target_size = self.default_batch_size
        
        for item in data:
            item_size = size_func(item) if size_func else 1
            
            # 如果添加当前项目会超过目标大小，完成当前批次
            if current_size + item_size > target_size and current_batch:
                batches.append(current_batch)
                current_batch = []
                current_size = 0
            
            current_batch.append(item)
            current_size += item_size
            
            # 如果当前批次已经达到最大批处理大小，立即处理
            if current_size >= self.max_batch_size:
                batches.append(current_batch)
                current_batch = []
                current_size = 0
        
        # 添加最后一个批次
        if current_batch:
            batches.append(current_batch)
        
        return batches
    
    def record_performance(self, 
                          batch_size: int, 
                          inference_time: float,
                          memory_usage: float) -> None:
        """
        记录性能数据
        
        Args:
            batch_size: 批处理大小
            inference_time: 推理时间
            memory_usage: 内存使用率
        """
        performance_metric = {
            "batch_size": batch_size,
            "inference_time": inference_time,
            "memory_usage": memory_usage,
            "efficiency": batch_size / inference_time if inference_time > 0 else 0
        }
        
        self.performance_history.append(performance_metric)
        
        # 如果有足够的数据，调整批处理大小
        if len(self.performance_history) >= self.performance_window // 2:
            self._adapt_batch_size()
    
    def _adapt_batch_size(self) -> None:
        """自适应调整批处理大小"""
        if not self.adaptive_batching or len(self.performance_history) < 10:
            return
        
        with self._adaptation_lock:
            # 计算平均效率
            avg_efficiency = sum(p["efficiency"] for p in self.performance_history) / len(self.performance_history)
            
            # 计算平均内存使用率
            avg_memory_usage = sum(p["memory_usage"] for p in self.performance_history) / len(self.performance_history)
            
            # 根据效率调整批处理大小
            if avg_efficiency > 100 and avg_memory_usage < 0.8:  # 效率高，内存使用低
                # 可以增大批处理大小
                new_size = min(self.max_batch_size, 
                             int(self.current_batch_size * (1 + self.batch_adaptation_rate)))
            elif avg_efficiency < 50 or avg_memory_usage > 0.9:  # 效率低或内存使用高
                # 需要减小批处理大小
                new_size = max(self.min_batch_size, 
                             int(self.current_batch_size * (1 - self.batch_adaptation_rate)))
            else:
                new_size = self.current_batch_size
            
            self.update_batch_size(new_size)