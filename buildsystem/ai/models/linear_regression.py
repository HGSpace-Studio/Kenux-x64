"""
线性回归模型

实现简单的线性回归算法，用于回归任务。
"""

import numpy as np
from typing import Any, Dict, List, Optional, Tuple
from pathlib import Path
import pickle
import json

from ..core.base_model import BaseModel


class LinearRegression(BaseModel):
    """线性回归模型"""
    
    def __init__(self, model_name: str, config: Optional[Dict[str, Any]] = None):
        """
        初始化线性回归模型
        
        Args:
            model_name: 模型名称
            config: 模型配置
        """
        super().__init__(model_name, config)
        
        # 模型参数
        self.weights: Optional[np.ndarray] = None
        self.bias: Optional[float] = None
        self.learning_rate = config.get("learning_rate", 0.01) if config else 0.01
        self.n_iterations = config.get("n_iterations", 1000) if config else 1000
        self.regularization = config.get("regularization", 0.0) if config else 0.0
        
        # 训练参数
        self.loss_history = []
        
    def _prepare_data(self, data: Any) -> Tuple[np.ndarray, np.ndarray]:
        """准备数据"""
        if isinstance(data, tuple) and len(data) == 2:
            X, y = data
        else:
            raise ValueError("数据格式应为 (X, y) 元组")
        
        # 转换为numpy数组
        X = np.array(X)
        y = np.array(y)
        
        # 添加偏置项
        if X.ndim == 1:
            X = X.reshape(-1, 1)
        
        # 添加偏置列
        X = np.c_[np.ones(X.shape[0]), X]
        
        return X, y
    
    def train(self, train_data: Any, validation_data: Optional[Any] = None, 
              **kwargs) -> Dict[str, Any]:
        """
        训练线性回归模型
        
        Args:
            train_data: 训练数据 (X, y)
            validation_data: 验证数据 (可选)
            **kwargs: 其他训练参数
            
        Returns:
            训练结果字典
        """
        print(f"开始训练线性回归模型: {self.model_name}")
        
        # 准备训练数据
        X, y = self._prepare_data(train_data)
        n_samples, n_features = X.shape
        
        # 更新元数据
        self.update_metadata(
            training_samples=n_samples,
            n_features=n_features,
            learning_rate=self.learning_rate,
            n_iterations=self.n_iterations
        )
        
        # 初始化参数
        self.weights = np.zeros(n_features)
        self.bias = 0.0
        
        # 训练过程
        self.loss_history = []
        
        for iteration in range(self.n_iterations):
            # 计算预测值
            y_pred = X.dot(self.weights)
            
            # 计算损失
            error = y_pred - y
            mse = np.mean(error ** 2)
            
            # 添加L2正则化
            if self.regularization > 0:
                mse += self.regularization * np.sum(self.weights ** 2)
            
            self.loss_history.append(mse)
            
            # 计算梯度
            gradient = (2/n_samples) * X.T.dot(error)
            
            # 添加正则化梯度
            if self.regularization > 0:
                gradient[1:] += 2 * self.regularization * self.weights[1:]
            
            # 更新参数
            self.weights -= self.learning_rate * gradient
            
            # 打印进度
            if iteration % 100 == 0:
                print(f"Iteration {iteration}, MSE: {mse:.6f}")
        
        # 记录训练历史
        self.training_history.append({
            "iteration": self.n_iterations,
            "final_mse": self.loss_history[-1],
            "avg_mse": np.mean(self.loss_history)
        })
        
        self.is_trained = True
        print(f"训练完成，最终MSE: {self.loss_history[-1]:.6f}")
        
        # 验证
        validation_result = None
        if validation_data is not None:
            validation_result = self.evaluate(validation_data)
        
        return {
            "loss": self.loss_history[-1],
            "weights": self.weights.tolist(),
            "bias": self.bias,
            "training_history": self.training_history,
            "validation": validation_result
        }
    
    def predict(self, data: Any, **kwargs) -> np.ndarray:
        """
        进行预测
        
        Args:
            data: 输入数据
            **kwargs: 其他预测参数
            
        Returns:
            预测结果
        """
        if not self.is_trained:
            raise ValueError("模型尚未训练")
        
        # 准备数据
        X, _ = self._prepare_data((data, np.zeros(len(data))))
        
        # 计算预测值
        predictions = X.dot(self.weights)
        
        # 移除偏置项的影响
        predictions = predictions[1:] if len(predictions) > 1 else predictions
        
        return predictions
    
    def evaluate(self, test_data: Any, **kwargs) -> Dict[str, float]:
        """
        评估模型性能
        
        Args:
            test_data: 测试数据 (X, y)
            **kwargs: 其他评估参数
            
        Returns:
            评估指标字典
        """
        if not self.is_trained:
            raise ValueError("模型尚未训练")
        
        # 准备数据
        X, y_true = self._prepare_data(test_data)
        n_samples = len(y_true)
        
        # 预测
        y_pred = self.predict(X[:, 1:])  # 移除偏置列
        
        # 计算评估指标
        mse = np.mean((y_true - y_pred) ** 2)
        rmse = np.sqrt(mse)
        mae = np.mean(np.abs(y_true - y_pred))
        r2 = 1 - np.sum((y_true - y_pred) ** 2) / np.sum((y_true - np.mean(y_true)) ** 2)
        
        return {
            "mse": mse,
            "rmse": rmse,
            "mae": mae,
            "r2": r2
        }
    
    def save(self, path: Union[str, Path]) -> None:
        """
        保存模型到文件
        
        Args:
            path: 保存路径
        """
        path = Path(path)
        path.mkdir(parents=True, exist_ok=True)
        
        model_data = {
            "model_name": self.model_name,
            "model_version": self.model_version,
            "config": self.config,
            "weights": self.weights.tolist() if self.weights is not None else None,
            "bias": self.bias,
            "metadata": self.metadata,
            "training_history": self.training_history,
            "loss_history": self.loss_history,
            "is_trained": self.is_trained
        }
        
        # 保存模型数据
        with open(path / "model.pkl", "wb") as f:
            pickle.dump(model_data, f)
        
        # 保存模型信息
        self.save_info(path)
        
        print(f"模型已保存: {path}")
    
    def load(self, path: Union[str, Path]) -> None:
        """
        从文件加载模型
        
        Args:
            path: 模型文件路径
        """
        path = Path(path)
        
        if not path.exists():
            raise FileNotFoundError(f"模型文件不存在: {path}")
        
        # 加载模型数据
        with open(path / "model.pkl", "rb") as f:
            model_data = pickle.load(f)
        
        # 恢复模型状态
        self.model_name = model_data["model_name"]
        self.model_version = model_data["model_version"]
        self.config = model_data["config"]
        self.weights = np.array(model_data["weights"]) if model_data["weights"] is not None else None
        self.bias = model_data["bias"]
        self.metadata = model_data["metadata"]
        self.training_history = model_data["training_history"]
        self.loss_history = model_data["loss_history"]
        self.is_trained = model_data["is_trained"]
        
        print(f"模型已加载: {path}")
    
    def get_weights(self) -> Optional[np.ndarray]:
        """获取模型权重"""
        return self.weights
    
    def get_bias(self) -> Optional[float]:
        """获取模型偏置"""
        return self.bias
    
    def plot_learning_curve(self, save_path: Optional[Union[str, Path]] = None) -> None:
        """
        绘制学习曲线
        
        Args:
            save_path: 保存路径（可选）
        """
        if not self.loss_history:
            print("没有训练历史数据，无法绘制学习曲线")
            return
        
        try:
            import matplotlib.pyplot as plt
            
            plt.figure(figsize=(10, 6))
            plt.plot(range(1, len(self.loss_history) + 1), self.loss_history)
            plt.xlabel("Iteration")
            plt.ylabel("Mean Squared Error")
            plt.title(f"Learning Curve - {self.model_name}")
            plt.grid(True)
            
            if save_path:
                plt.savefig(save_path)
                print(f"学习曲线已保存: {save_path}")
            else:
                plt.show()
                
        except ImportError:
            print("未安装matplotlib，无法绘制学习曲线")
    
    def predict_with_confidence(self, data: Any, confidence_level: float = 0.95) -> Dict[str, Any]:
        """
        带置信区间的预测
        
        Args:
            data: 输入数据
            confidence_level: 置信水平
            
        Returns:
            包含置信区间的预测结果
        """
        if not self.is_trained:
            raise ValueError("模型尚未训练")
        
        # 进行多次预测以估计方差
        n_predictions = 100
        predictions = []
        
        for _ in range(n_predictions):
            # 添加噪声模拟预测的不确定性
            noisy_data = data + np.random.normal(0, 0.01, size=data.shape)
            pred = self.predict(noisy_data)
            predictions.append(pred)
        
        predictions = np.array(predictions)
        
        # 计算统计量
        mean_pred = np.mean(predictions, axis=0)
        std_pred = np.std(predictions, axis=0)
        
        # 计算置信区间
        from scipy import stats
        z_score = stats.norm.ppf((1 + confidence_level) / 2)
        lower_bound = mean_pred - z_score * std_pred
        upper_bound = mean_pred + z_score * std_pred
        
        return {
            "predictions": mean_pred.tolist(),
            "confidence_interval": {
                "lower": lower_bound.tolist(),
                "upper": upper_bound.tolist(),
                "confidence_level": confidence_level
            },
            "uncertainty": std_pred.tolist()
        }