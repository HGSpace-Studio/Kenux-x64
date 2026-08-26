"""
AI框架集成模块

将AI框架与现有的构建系统集成，提供AI模型作为构建目标的支持。
"""

import os
import json
import pickle
from pathlib import Path
from typing import Any, Dict, List, Optional, Union, Tuple
from datetime import datetime

from .model import Target, BuildGraph, GraphError
from .runner import BuildRunner, BuildSettings, ActionContext
from .state import BuildPaths, TaskStore

from ai import (
    BaseModel, ModelManager, ModelTrainer, TrainingConfig,
    ModelInference, InferenceConfig
)


class AIModelTarget(Target):
    """AI模型目标类，继承自Target"""
    
    def __init__(self, 
                 name: str,
                 model_type: str,
                 model_name: str,
                 dataset_path: Optional[Union[str, Path]] = None,
                 training_config: Optional[Dict[str, Any]] = None,
                 inference_config: Optional[Dict[str, Any]] = None,
                 outputs: Optional[Tuple[Path, ...]] = (),
                 **kwargs):
        """
        初始化AI模型目标
        
        Args:
            name: 目标名称
            model_type: 模型类型（如 "linear_regression", "neural_network"）
            model_name: 模型实例名称
            dataset_path: 数据集路径
            training_config: 训练配置
            inference_config: 推理配置
            outputs: 输出文件路径
            **kwargs: 其他目标参数
        """
        super().__init__(name, outputs=outputs, **kwargs)
        
        self.model_type = model_type
        self.model_name = model_name
        self.dataset_path = Path(dataset_path) if dataset_path else None
        self.training_config = training_config or {}
        self.inference_config = inference_config or {}
        
        # AI框架组件
        self.model_manager = ModelManager()
        self.model = None
        self.dataset = None
        
    def setup_model(self, build_paths: BuildPaths) -> None:
        """设置模型"""
        try:
            # 创建模型
            self.model = self.model_manager.create_model(
                self.model_type, 
                self.model_name, 
                self.training_config
            )
            print(f"AI模型 '{self.model_name}' 已创建")
        except Exception as e:
            raise GraphError(f"创建模型失败: {e}")
    
    def setup_dataset(self, build_paths: BuildPaths) -> None:
        """设置数据集"""
        if self.dataset_path:
            self.dataset = Dataset(f"dataset_{self.name}")
            self.dataset.data_path = build_paths.root / self.dataset_path
            self.dataset.load_data()
            print(f"数据集已加载: {self.dataset_path}")
    
    def train_model(self, context: ActionContext) -> None:
        """训练模型"""
        if not self.model or not self.dataset:
            raise GraphError("模型或数据集未设置")
        
        # 准备训练数据
        train_data = self.dataset.get_train_data()
        
        # 创建训练器
        training_config = TrainingConfig.from_dict(self.training_config)
        trainer = ModelTrainer(self.model, training_config)
        
        # 训练模型
        result = trainer.train(train_data)
        
        # 保存模型
        model_output_dir = context.paths.out / f"{self.name}_model"
        model_output_dir.mkdir(parents=True, exist_ok=True)
        
        self.model.save(model_output_dir)
        
        # 保存训练结果
        with open(model_output_dir / "training_result.json", "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2, ensure_ascii=False)
        
        print(f"模型训练完成，结果已保存到: {model_output_dir}")
    
    def evaluate_model(self, context: ActionContext) -> None:
        """评估模型"""
        if not self.model or not self.dataset:
            raise GraphError("模型或数据集未设置")
        
        # 准备测试数据
        test_data = self.dataset.get_test_data()
        
        # 评估模型
        metrics = self.model.evaluate(test_data)
        
        # 保存评估结果
        eval_output_dir = context.paths.out / f"{self.name}_evaluation"
        eval_output_dir.mkdir(parents=True, exist_ok=True)
        
        with open(eval_output_dir / "evaluation_result.json", "w", encoding="utf-8") as f:
            json.dump(metrics, f, indent=2, ensure_ascii=False)
        
        print(f"模型评估完成，结果已保存到: {eval_output_dir}")
    
    def run_inference(self, context: ActionContext) -> None:
        """运行推理"""
        if not self.model:
            raise GraphError("模型未设置")
        
        # 创建推理器
        inference_config = InferenceConfig.from_dict(self.inference_config)
        inference = ModelInference(self.model, inference_config)
        
        # 这里假设使用测试数据进行推理
        if self.dataset and hasattr(self.dataset, 'get_test_data'):
            test_data = self.dataset.get_test_data()[0]  # 只使用特征
            predictions = inference.predict(test_data)
            
            # 保存推理结果
            inference_output_dir = context.paths.out / f"{self.name}_inference"
            inference_output_dir.mkdir(parents=True, exist_ok=True)
            
            with open(inference_output_dir / "predictions.json", "w", encoding="utf-8") as f:
                json.dump({"predictions": predictions.tolist()}, f, indent=2, ensure_ascii=False)
            
            print(f"推理完成，结果已保存到: {inference_output_dir}")


class AIBuildGraph(BuildGraph):
    """AI构建图类，扩展BuildGraph以支持AI模型目标"""
    
    def __init__(self, root: Path) -> None:
        """初始化AI构建图"""
        super().__init__(root)
        
        # AI模型管理器
        self.model_manager = ModelManager()
        
        # 注册默认模型类型
        self._register_default_models()
    
    def _register_default_models(self) -> None:
        """注册默认模型类型"""
        try:
            from ..ai.models import LinearRegression, NeuralNetwork
            
            self.model_manager.register_model_type("linear_regression", LinearRegression)
            self.model_manager.register_model_type("neural_network", NeuralNetwork)
        except ImportError:
            print("警告: 无法导入默认模型类型")
    
    def add_ai_model_target(self, 
                          name: str,
                          model_type: str,
                          model_name: str,
                          dataset_path: Optional[Union[str, Path]] = None,
                          training_config: Optional[Dict[str, Any]] = None,
                          inference_config: Optional[Dict[str, Any]] = None,
                          outputs: Optional[Tuple[Path, ...]] = (),
                          action: Optional[str] = "train") -> Target:
        """
        添加AI模型目标
        
        Args:
            name: 目标名称
            model_type: 模型类型
            model_name: 模型实例名称
            dataset_path: 数据集路径
            training_config: 训练配置
            inference_config: 推理配置
            outputs: 输出文件路径
            action: 执行动作 ("train", "evaluate", "inference")
            
        Returns:
            创建的目标
        """
        # 创建AI模型目标
        ai_target = AIModelTarget(
            name=name,
            model_type=model_type,
            model_name=model_name,
            dataset_path=dataset_path,
            training_config=training_config,
            inference_config=inference_config,
            outputs=outputs
        )
        
        # 根据动作设置action
        if action == "train":
            ai_target.action = lambda ctx: ai_target.train_model(ctx)
            ai_target.kind = "ai_train"
        elif action == "evaluate":
            ai_target.action = lambda ctx: ai_target.evaluate_model(ctx)
            ai_target.kind = "ai_evaluate"
        elif action == "inference":
            ai_target.action = lambda ctx: ai_target.run_inference(ctx)
            ai_target.kind = "ai_inference"
        else:
            raise ValueError(f"未知的动作: {action}")
        
        # 添加到构建图
        return self.add(ai_target)
    
    def create_ai_pipeline(self, 
                          dataset_path: Union[str, Path],
                          model_configs: List[Dict[str, Any]],
                          pipeline_name: str = "ai_pipeline") -> List[Target]:
        """
        创建AI流水线
        
        Args:
            dataset_path: 数据集路径
            model_configs: 模型配置列表
            pipeline_name: 流水线名称
            
        Returns:
            创建的目标列表
        """
        targets = []
        
        # 数据集目标
        dataset_target = Target(
            name="load_dataset",
            kind="dataset",
            action=lambda ctx: None  # 简单的数据加载动作
        )
        targets.append(self.add(dataset_target))
        
        # 为每个模型创建训练、评估和推理目标
        for i, model_config in enumerate(model_configs):
            model_name = f"{model_config['model_type']}_{i}"
            
            # 训练目标
            train_target = self.add_ai_model_target(
                name=f"train_{model_name}",
                model_type=model_config["model_type"],
                model_name=model_name,
                dataset_path=dataset_path,
                training_config=model_config.get("training_config", {}),
                action="train"
            )
            train_target.depends_on = ("load_dataset",)
            targets.append(train_target)
            
            # 评估目标
            eval_target = self.add_ai_model_target(
                name=f"eval_{model_name}",
                model_type=model_config["model_type"],
                model_name=model_name,
                dataset_path=dataset_path,
                action="evaluate"
            )
            eval_target.depends_on = (f"train_{model_name}",)
            targets.append(eval_target)
            
            # 推理目标
            inference_target = self.add_ai_model_target(
                name=f"inference_{model_name}",
                model_type=model_config["model_type"],
                model_name=model_name,
                dataset_path=dataset_path,
                inference_config=model_config.get("inference_config", {}),
                action="inference"
            )
            inference_target.depends_on = (f"train_{model_name}",)
            targets.append(inference_target)
        
        # 创建聚合目标
        pipeline_target = Target(
            name=pipeline_name,
            depends_on=tuple(t.name for t in targets if t.name != "load_dataset"),
            group=True
        )
        targets.append(self.add(pipeline_target))
        
        return targets
    
    def get_ai_targets(self) -> List[Target]:
        """获取所有AI目标"""
        return [target for target in self.targets.values() 
                if target.kind.startswith("ai_")]


def create_ai_build_graph(root: Path, 
                         dataset_path: Optional[Union[str, Path]] = None,
                         model_configs: Optional[List[Dict[str, Any]]] = None) -> AIBuildGraph:
    """
    创建AI构建图
    
    Args:
        root: 根目录
        dataset_path: 数据集路径
        model_configs: 模型配置列表
        
    Returns:
        创建的AI构建图
    """
    graph = AIBuildGraph(root)
    
    # 如果提供了模型配置，创建流水线
    if model_configs and dataset_path:
        graph.create_ai_pipeline(dataset_path, model_configs)
    
    return graph


# 便捷函数
def setup_ai_build(paths: BuildPaths, 
                  graph_name: str = "ai_build",
                  dataset_path: Optional[Union[str, Path]] = None,
                  model_configs: Optional[List[Dict[str, Any]]] = None) -> Tuple[BuildRunner, BuildGraph]:
    """
    设置AI构建
    
    Args:
        paths: 构建路径
        graph_name: 图名称
        dataset_path: 数据集路径
        model_configs: 模型配置列表
        
    Returns:
        构建运行器和构建图
    """
    # 创建AI构建图
    graph = create_ai_build_graph(paths.root, dataset_path, model_configs)
    
    # 创建构建运行器
    settings = BuildSettings.automatic()
    runner = BuildRunner(graph, paths, settings, graph_name)
    
    return runner, graph


def run_ai_training_task(paths: BuildPaths,
                         task_name: str,
                         model_type: str,
                         model_name: str,
                         dataset_path: Union[str, Path],
                         training_config: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    运行AI训练任务
    
    Args:
        paths: 构建路径
        task_name: 任务名称
        model_type: 模型类型
        model_name: 模型名称
        dataset_path: 数据集路径
        training_config: 训练配置
        
    Returns:
        任务结果
    """
    # 创建构建图
    graph = AIBuildGraph(paths.root)
    
    # 添加训练目标
    train_target = graph.add_ai_model_target(
        name=task_name,
        model_type=model_type,
        model_name=model_name,
        dataset_path=dataset_path,
        training_config=training_config,
        action="train"
    )
    
    # 创建运行器
    settings = BuildSettings.automatic()
    runner = BuildRunner(graph, paths, settings, task_name)
    
    # 运行任务
    metrics = runner.run((train_target,), f"ai_train_{task_name}")
    
    return {
        "status": "completed",
        "metrics": metrics.__dict__,
        "model_saved": True
    }