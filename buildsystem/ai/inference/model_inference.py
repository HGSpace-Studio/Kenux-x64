"""
模型推理器

负责模型的推理过程，包括预处理、推理、后处理等功能。
"""

import time
import numpy as np
from typing import Any, Dict, List, Optional, Union, Tuple
from pathlib import Path
import json
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
from queue import Queue

from ..core.base_model import BaseModel
from .inference_config import InferenceConfig
from .batch_processor import BatchProcessor


class ModelInference:
    """模型推理器类"""
    
    def __init__(self, 
                 model: BaseModel,
                 inference_config: Optional[InferenceConfig] = None):
        """
        初始化推理器
        
        Args:
            model: 要推理的模型
            inference_config: 推理配置
        """
        self.model = model
        self.config = inference_config or InferenceConfig()
        
        # 批处理器
        self.batch_processor = BatchProcessor(self.config.batch_size, 
                                           self.config.max_batch_size)
        
        # 推理统计信息
        self.inference_stats = {
            "total_inferences": 0,
            "total_time": 0.0,
            "avg_inference_time": 0.0,
            "min_inference_time": float('inf'),
            "max_inference_time": 0.0,
            "batch_times": [],
            "input_shapes": [],
            "output_shapes": []
        }
        
        # 线程安全锁
        self._stats_lock = threading.Lock()
        
        # 输入/输出变换器
        self.input_transformer = None
        self.output_transformer = None
        
        # 缓存
        self.cache_enabled = self.config.cache_enabled
        self.cache = {}
        
        # 回调函数
        self.callbacks = {
            "on_inference_begin": [],
            "on_inference_end": [],
            "on_batch_begin": [],
            "on_batch_end": []
        }
    
    def set_input_transformer(self, transformer: Any) -> None:
        """设置输入数据变换器"""
        self.input_transformer = transformer
    
    def set_output_transformer(self, transformer: Any) -> None:
        """设置输出数据变换器"""
        self.output_transformer = transformer
    
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
    
    def predict(self, 
                input_data: Any,
                batch_size: Optional[int] = None,
                use_cache: Optional[bool] = None) -> Any:
        """
        进行预测
        
        Args:
            input_data: 输入数据
            batch_size: 批处理大小（可选）
            use_cache: 是否使用缓存（可选）
            
        Returns:
            预测结果
        """
        # 调用推理开始回调
        self._trigger_callbacks("on_inference_begin", {
            "input_data": input_data,
            "config": self.config,
            "model": self.model
        })
        
        start_time = time.time()
        
        # 确定是否使用缓存
        use_cache = use_cache if use_cache is not None else self.cache_enabled
        
        # 处理单个样本
        if self._is_single_sample(input_data):
            result = self._predict_single(input_data, use_cache)
        else:
            # 批量处理
            batch_size = batch_size or self.config.batch_size
            result = self._predict_batch(input_data, batch_size, use_cache)
        
        # 更新统计信息
        inference_time = time.time() - start_time
        self._update_stats(inference_time, input_data, result)
        
        # 调用推理结束回调
        self._trigger_callbacks("on_inference_end", {
            "result": result,
            "inference_time": inference_time,
            "stats": self.inference_stats
        })
        
        return result
    
    def _predict_single(self, input_data: Any, use_cache: bool) -> Any:
        """预测单个样本"""
        # 生成缓存键
        cache_key = None
        if use_cache:
            cache_key = self._generate_cache_key(input_data)
            
            # 检查缓存
            if cache_key in self.cache:
                return self.cache[cache_key]
        
        # 输入预处理
        processed_input = self._preprocess_input(input_data)
        
        # 模型推理
        start_time = time.time()
        prediction = self.model.predict(processed_input)
        inference_time = time.time() - start_time
        
        # 输出后处理
        result = self._postprocess_output(prediction)
        
        # 缓存结果
        if use_cache and cache_key is not None:
            self.cache[cache_key] = result
        
        return result
    
    def _predict_batch(self, 
                      input_data: Any, 
                      batch_size: int, 
                      use_cache: bool) -> Any:
        """批量预测"""
        # 分批处理数据
        batches = self.batch_processor.create_batches(input_data, batch_size)
        
        results = []
        total_time = 0.0
        
        # 调用批处理开始回调
        self._trigger_callbacks("on_batch_begin", {
            "num_batches": len(batches),
            "batch_size": batch_size,
            "total_samples": len(input_data) if hasattr(input_data, '__len__') else 0
        })
        
        # 并行处理批次（如果启用）
        if self.config.parallel_inference and batch_size > 1:
            results = self._predict_parallel(batches, use_cache)
        else:
            # 串行处理批次
            for batch_idx, batch in enumerate(batches):
                start_time = time.time()
                
                batch_result = self._predict_batch_single(batch, use_cache)
                results.append(batch_result)
                
                batch_time = time.time() - start_time
                total_time += batch_time
                
                self.inference_stats["batch_times"].append(batch_time)
                
                # 调用批次结束回调
                self._trigger_callbacks("on_batch_end", {
                    "batch_idx": batch_idx,
                    "batch_size": len(batch),
                    "batch_time": batch_time,
                    "total_time": total_time,
                    "batch_result": batch_result
                })
        
        # 合并结果
        if isinstance(results[0], (list, np.ndarray)):
            result = np.concatenate(results, axis=0)
        else:
            # 对于非数组类型，简单拼接
            result = []
            for r in results:
                if isinstance(r, list):
                    result.extend(r)
                else:
                    result.append(r)
        
        return result
    
    def _predict_batch_single(self, batch: Any, use_cache: bool) -> Any:
        """处理单个批次"""
        # 检查缓存
        if use_cache:
            cache_key = self._generate_cache_key(batch)
            if cache_key in self.cache:
                return self.cache[cache_key]
        
        # 批量预处理
        processed_batch = self._preprocess_input(batch)
        
        # 批量推理
        start_time = time.time()
        batch_prediction = self.model.predict(processed_batch)
        inference_time = time.time() - start_time
        
        # 批量后处理
        batch_result = self._postprocess_output(batch_prediction)
        
        # 批量缓存
        if use_cache:
            cache_key = self._generate_cache_key(batch)
            self.cache[cache_key] = batch_result
        
        return batch_result
    
    def _predict_parallel(self, batches: List[Any], use_cache: bool) -> List[Any]:
        """并行预测"""
        results = []
        
        with ThreadPoolExecutor(max_workers=self.config.max_workers) as executor:
            # 提交所有批次任务
            future_to_batch = {
                executor.submit(self._predict_batch_single, batch, use_cache): batch 
                for batch in batches
            }
            
            # 收集结果
            for future in as_completed(future_to_batch):
                batch = future_to_batch[future]
                try:
                    result = future.result()
                    results.append(result)
                except Exception as exc:
                    print(f"批次 {batch} 推理失败: {exc}")
                    results.append(None)
        
        return results
    
    def _preprocess_input(self, input_data: Any) -> Any:
        """输入预处理"""
        if self.input_transformer is not None:
            input_data = self.input_transformer(input_data)
        
        return input_data
    
    def _postprocess_output(self, output_data: Any) -> Any:
        """输出后处理"""
        if self.output_transformer is not None:
            output_data = self.output_transformer(output_data)
        
        return output_data
    
    def _is_single_sample(self, data: Any) -> bool:
        """判断是否为单个样本"""
        if isinstance(data, (list, np.ndarray)):
            return len(data) == 1
        return True
    
    def _generate_cache_key(self, data: Any) -> str:
        """生成缓存键"""
        # 这里可以根据数据类型生成不同的键
        if isinstance(data, (list, np.ndarray)):
            # 对于数组类型，使用形状和数据内容生成哈希
            data_hash = hash(str(data.shape) + str(data.tobytes()))
        else:
            # 对于其他类型，直接转换为字符串
            data_hash = hash(str(data))
        
        return f"inference_{data_hash}"
    
    def _update_stats(self, inference_time: float, input_data: Any, output_data: Any) -> None:
        """更新统计信息"""
        with self._stats_lock:
            self.inference_stats["total_inferences"] += 1
            self.inference_stats["total_time"] += inference_time
            self.inference_stats["avg_inference_time"] = (
                self.inference_stats["total_time"] / self.inference_stats["total_inferences"]
            )
            
            self.inference_stats["min_inference_time"] = min(
                self.inference_stats["min_inference_time"], 
                inference_time
            )
            self.inference_stats["max_inference_time"] = max(
                self.inference_stats["max_inference_time"], 
                inference_time
            )
            
            # 记录输入/输出形状
            if hasattr(input_data, 'shape'):
                self.inference_stats["input_shapes"].append(input_data.shape)
            if hasattr(output_data, 'shape'):
                self.inference_stats["output_shapes"].append(output_data.shape)
    
    def _trigger_callbacks(self, event: str, info: Dict[str, Any]) -> None:
        """触发回调函数"""
        for callback in self.callbacks.get(event, []):
            try:
                callback(info)
            except Exception as e:
                print(f"回调函数执行失败 {event}: {e}")
    
    def get_inference_stats(self) -> Dict[str, Any]:
        """获取推理统计信息"""
        with self._stats_lock:
            return self.inference_stats.copy()
    
    def reset_stats(self) -> None:
        """重置统计信息"""
        with self._stats_lock:
            self.inference_stats = {
                "total_inferences": 0,
                "total_time": 0.0,
                "avg_inference_time": 0.0,
                "min_inference_time": float('inf'),
                "max_inference_time": 0.0,
                "batch_times": [],
                "input_shapes": [],
                "output_shapes": []
            }
    
    def clear_cache(self) -> None:
        """清空缓存"""
        self.cache.clear()
        print("推理缓存已清空")
    
    def cache_info(self) -> Dict[str, Any]:
        """获取缓存信息"""
        return {
            "cache_size": len(self.cache),
            "cache_enabled": self.cache_enabled,
            "cache_keys": list(self.cache.keys())
        }
    
    def benchmark(self, 
                  test_data: Any,
                  num_runs: int = 100,
                  batch_size: Optional[int] = None) -> Dict[str, Any]:
        """
        性能基准测试
        
        Args:
            test_data: 测试数据
            num_runs: 运行次数
            batch_size: 批处理大小
            
        Returns:
            性能测试结果
        """
        print(f"开始性能基准测试，运行次数: {num_runs}")
        
        # 重置统计
        self.reset_stats()
        
        # 执行多次推理
        for i in range(num_runs):
            result = self.predict(test_data, batch_size=batch_size, use_cache=False)
            if i % 10 == 0:
                print(f"已完成 {i}/{num_runs} 次推理")
        
        # 获取统计信息
        stats = self.get_inference_stats()
        
        benchmark_result = {
            "total_runs": num_runs,
            "total_time": stats["total_time"],
            "avg_time_per_run": stats["avg_inference_time"],
            "min_time": stats["min_inference_time"],
            "max_time": stats["max_inference_time"],
            "throughput": num_runs / stats["total_time"],  # 每秒推理次数
            "avg_batch_time": np.mean(stats["batch_times"]) if stats["batch_times"] else 0
        }
        
        print(f"性能测试完成: {benchmark_result}")
        return benchmark_result
    
    def save_inference_stats(self, save_path: Union[str, Path]) -> None:
        """保存推理统计信息"""
        save_path = Path(save_path)
        save_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(save_path, "w", encoding="utf-8") as f:
            json.dump(self.inference_stats, f, indent=2, ensure_ascii=False)
        
        print(f"推理统计信息已保存: {save_path}")
    
    def load_inference_stats(self, load_path: Union[str, Path]) -> None:
        """加载推理统计信息"""
        load_path = Path(load_path)
        
        if load_path.exists():
            with open(load_path, "r", encoding="utf-8") as f:
                self.inference_stats = json.load(f)
            
            print(f"推理统计信息已加载: {load_path}")
        else:
            raise FileNotFoundError(f"统计文件不存在: {load_path}")