"""
神经网络模型

实现简单的多层感知机神经网络，用于分类和回归任务。
"""

import numpy as np
from typing import Any, Dict, List, Optional, Tuple, Union
from pathlib import Path
import pickle
import json

from ..core.base_model import BaseModel


class NeuralNetwork(BaseModel):
    """神经网络模型"""
    
    def __init__(self, model_name: str, config: Optional[Dict[str, Any]] = None):
        """
        初始化神经网络模型
        
        Args:
            model_name: 模型名称
            config: 模型配置
        """
        super().__init__(model_name, config)
        
        # 网络结构
        self.layer_sizes = config.get("layer_sizes", [64, 32, 1]) if config else [64, 32, 1]
        self.activation = config.get("activation", "relu") if config else "relu"
        self.output_activation = config.get("output_activation", "linear") if config else "linear"
        
        # 训练参数
        self.learning_rate = config.get("learning_rate", 0.001) if config else 0.001
        self.n_iterations = config.get("n_iterations", 1000) if config else 1000
        self.batch_size = config.get("batch_size", 32) if config else 32
        self.regularization = config.get("regularization", 0.01) if config else 0.01
        
        # 优化参数
        self.momentum = config.get("momentum", 0.9) if config else 0.9
        self.beta1 = config.get("beta1", 0.9) if config else 0.9
        self.beta2 = config.get("beta2", 0.999) if config else 0.999
        self.epsilon = config.get("epsilon", 1e-8) if config else 1e-8
        
        # 网络参数
        self.weights: List[np.ndarray] = []
        self.biases: List[np.ndarray] = []
        self.velocity_w: List[np.ndarray] = []
        self.velocity_b: List[np.ndarray] = []
        self.m_w: List[np.ndarray] = []
        self.v_w: List[np.ndarray] = []
        self.m_b: List[np.ndarray] = []
        self.v_b: List[np.ndarray] = []
        
        # 训练历史
        self.loss_history = []
        self.accuracy_history = []
        
    def _initialize_parameters(self) -> None:
        """初始化网络参数"""
        self.weights = []
        self.biases = []
        self.velocity_w = []
        self.velocity_b = []
        self.m_w = []
        self.v_w = []
        self.m_b = []
        self.v_b = []
        
        for i in range(len(self.layer_sizes) - 1):
            # He初始化
            n_in = self.layer_sizes[i]
            n_out = self.layer_sizes[i + 1]
            
            # 权重
            W = np.random.randn(n_out, n_in) * np.sqrt(2.0 / n_in)
            self.weights.append(W)
            self.velocity_w.append(np.zeros_like(W))
            self.m_w.append(np.zeros_like(W))
            self.v_w.append(np.zeros_like(W))
            
            # 偏置
            b = np.zeros((n_out, 1))
            self.biases.append(b)
            self.velocity_b.append(np.zeros_like(b))
            self.m_b.append(np.zeros_like(b))
            self.v_b.append(np.zeros_like(b))
    
    def _activation_function(self, Z: np.ndarray) -> np.ndarray:
        """激活函数"""
        if self.activation == "relu":
            return np.maximum(0, Z)
        elif self.activation == "sigmoid":
            return 1 / (1 + np.exp(-Z))
        elif self.activation == "tanh":
            return np.tanh(Z)
        else:
            raise ValueError(f"未知的激活函数: {self.activation}")
    
    def _activation_derivative(self, Z: np.ndarray) -> np.ndarray:
        """激活函数导数"""
        if self.activation == "relu":
            return (Z > 0).astype(float)
        elif self.activation == "sigmoid":
            s = 1 / (1 + np.exp(-Z))
            return s * (1 - s)
        elif self.activation == "tanh":
            return 1 - np.tanh(Z) ** 2
        else:
            raise ValueError(f"未知的激活函数: {self.activation}")
    
    def _output_activation_function(self, Z: np.ndarray) -> np.ndarray:
        """输出层激活函数"""
        if self.output_activation == "linear":
            return Z
        elif self.output_activation == "sigmoid":
            return 1 / (1 + np.exp(-Z))
        elif self.output_activation == "softmax":
            exp_Z = np.exp(Z - np.max(Z, axis=0, keepdims=True))
            return exp_Z / np.sum(exp_Z, axis=0, keepdims=True)
        else:
            raise ValueError(f"未知的输出激活函数: {self.output_activation}")
    
    def _output_activation_derivative(self, Z: np.ndarray, Y: np.ndarray) -> np.ndarray:
        """输出层激活函数导数"""
        if self.output_activation == "linear":
            return np.ones_like(Z)
        elif self.output_activation == "sigmoid":
            return Y * (1 - Y)
        elif self.output_activation == "softmax":
            # 简化的softmax导数计算
            return Y * (1 - Y)
        else:
            raise ValueError(f"未知的输出激活函数: {self.output_activation}")
    
    def _forward_propagation(self, X: np.ndarray) -> Tuple[List[np.ndarray], List[np.ndarray]]:
        """前向传播"""
        Z_values = [X]
        A_values = [X]
        
        for i in range(len(self.weights)):
            Z = np.dot(self.weights[i], A_values[-1]) + self.biases[i]
            A = self._activation_function(Z)
            Z_values.append(Z)
            A_values.append(A)
        
        # 输出层
        Z_output = np.dot(self.weights[-1], A_values[-1]) + self.biases[-1]
        A_output = self._output_activation_function(Z_output)
        Z_values.append(Z_output)
        A_values.append(A_output)
        
        return Z_values, A_values
    
    def _backward_propagation(self, X: np.ndarray, Y: np.ndarray, 
                            Z_values: List[np.ndarray], A_values: List[np.ndarray]) -> Tuple[List[np.ndarray], List[np.ndarray]]:
        """反向传播"""
        m = X.shape[1]
        
        # 输出层误差
        dZ = A_values[-1] - Y
        if self.output_activation == "softmax":
            # 对于多分类任务，使用交叉熵损失
            dZ = (A_values[-1] - Y) / m
        
        # 存储梯度
        dW = [None] * len(self.weights)
        db = [None] * len(self.biases)
        
        # 输出层梯度
        dW[-1] = (1/m) * np.dot(dZ, A_values[-2].T)
        db[-1] = (1/m) * np.sum(dZ, axis=1, keepdims=True)
        
        # 隐藏层梯度
        for l in range(len(self.weights) - 2, -1, -1):
            dZ = np.dot(self.weights[l + 1].T, dZ) * self._activation_derivative(Z_values[l + 1])
            dW[l] = (1/m) * np.dot(dZ, A_values[l].T)
            db[l] = (1/m) * np.sum(dZ, axis=1, keepdims=True)
        
        # 添加L2正则化
        for i in range(len(dW)):
            dW[i] += (self.regularization / m) * self.weights[i]
        
        return dW, db
    
    def _update_parameters(self, dW: List[np.ndarray], db: List[np.ndarray], t: int) -> None:
        """更新参数"""
        # 使用Adam优化器
        for i in range(len(self.weights)):
            # 更新一阶矩估计
            self.m_w[i] = self.beta1 * self.m_w[i] + (1 - self.beta1) * dW[i]
            self.m_b[i] = self.beta1 * self.m_b[i] + (1 - self.beta1) * db[i]
            
            # 更新二阶矩估计
            self.v_w[i] = self.beta2 * self.v_w[i] + (1 - self.beta2) * (dW[i] ** 2)
            self.v_b[i] = self.beta2 * self.v_b[i] + (1 - self.beta2) * (db[i] ** 2)
            
            # 计算偏差修正后的估计
            m_w_corrected = self.m_w[i] / (1 - self.beta1 ** (t + 1))
            m_b_corrected = self.m_b[i] / (1 - self.beta1 ** (t + 1))
            v_w_corrected = self.v_w[i] / (1 - self.beta2 ** (t + 1))
            v_b_corrected = self.v_b[i] / (1 - self.beta2 ** (t + 1))
            
            # 更新权重和偏置
            self.weights[i] -= self.learning_rate * m_w_corrected / (np.sqrt(v_w_corrected) + self.epsilon)
            self.biases[i] -= self.learning_rate * m_b_corrected / (np.sqrt(v_b_corrected) + self.epsilon)
    
    def _compute_loss(self, Y: np.ndarray, A: np.ndarray) -> float:
        """计算损失"""
        m = Y.shape[1]
        
        if self.output_activation == "sigmoid":
            # 二分类交叉熵
            loss = -np.sum(Y * np.log(A + 1e-8) + (1 - Y) * np.log(1 - A + 1e-8)) / m
        elif self.output_activation == "softmax":
            # 多分类交叉熵
            loss = -np.sum(Y * np.log(A + 1e-8)) / m
        else:
            # 均方误差
            loss = np.mean((Y - A) ** 2)
        
        # 添加L2正则化
        for W in self.weights:
            loss += (self.regularization / (2 * m)) * np.sum(W ** 2)
        
        return loss
    
    def _prepare_data(self, data: Any) -> Tuple[np.ndarray, np.ndarray]:
        """准备数据"""
        if isinstance(data, tuple) and len(data) == 2:
            X, y = data
        else:
            raise ValueError("数据格式应为 (X, y) 元组")
        
        # 转换为numpy数组
        X = np.array(X)
        y = np.array(y)
        
        # 确保X是2D数组 (样本数, 特征数)
        if X.ndim == 1:
            X = X.reshape(-1, 1)
        
        # 确保y是列向量
        if y.ndim == 1:
            y = y.reshape(-1, 1)
        
        return X.T, y.T  # 转置为 (特征数, 样本数)
    
    def train(self, train_data: Any, validation_data: Optional[Any] = None, 
              **kwargs) -> Dict[str, Any]:
        """
        训练神经网络
        
        Args:
            train_data: 训练数据 (X, y)
            validation_data: 验证数据 (可选)
            **kwargs: 其他训练参数
            
        Returns:
            训练结果字典
        """
        print(f"开始训练神经网络模型: {self.model_name}")
        
        # 准备训练数据
        X, y = self._prepare_data(train_data)
        n_samples, n_features = X.shape
        
        # 检查输出维度
        if self.output_activation == "softmax":
            # 对于多分类，y应该是one-hot编码
            n_classes = y.shape[0]
        else:
            n_classes = y.shape[0]
        
        # 更新网络结构
        self.layer_sizes[0] = n_features
        self.layer_sizes[-1] = n_classes
        
        # 初始化参数
        self._initialize_parameters()
        
        # 更新元数据
        self.update_metadata(
            training_samples=n_samples,
            n_features=n_features,
            n_classes=n_classes,
            layer_sizes=self.layer_sizes,
            learning_rate=self.learning_rate,
            n_iterations=self.n_iterations,
            batch_size=self.batch_size
        )
        
        # 训练过程
        self.loss_history = []
        self.accuracy_history = []
        
        for epoch in range(self.n_iterations):
            # 随机打乱数据
            permutation = np.random.permutation(n_samples)
            X_shuffled = X[:, permutation]
            y_shuffled = y[:, permutation]
            
            # 分批处理
            for i in range(0, n_samples, self.batch_size):
                X_batch = X_shuffled[:, i:i + self.batch_size]
                y_batch = y_shuffled[:, i:i + self.batch_size]
                
                # 前向传播
                Z_values, A_values = self._forward_propagation(X_batch)
                
                # 反向传播
                dW, db = self._backward_propagation(X_batch, y_batch, Z_values, A_values)
                
                # 更新参数
                self._update_parameters(dW, db, epoch)
            
            # 计算训练损失和准确率
            _, A_train = self._forward_propagation(X)
            train_loss = self._compute_loss(y, A_train)
            train_accuracy = self._compute_accuracy(y, A_train)
            
            self.loss_history.append(train_loss)
            self.accuracy_history.append(train_accuracy)
            
            # 打印进度
            if epoch % 100 == 0:
                print(f"Epoch {epoch}, Loss: {train_loss:.6f}, Accuracy: {train_accuracy:.4f}")
        
        # 记录训练历史
        self.training_history.append({
            "epoch": self.n_iterations,
            "final_loss": self.loss_history[-1],
            "final_accuracy": self.accuracy_history[-1],
            "avg_loss": np.mean(self.loss_history),
            "avg_accuracy": np.mean(self.accuracy_history)
        })
        
        self.is_trained = True
        print(f"训练完成，最终损失: {self.loss_history[-1]:.6f}, 准确率: {self.accuracy_history[-1]:.4f}")
        
        # 验证
        validation_result = None
        if validation_data is not None:
            validation_result = self.evaluate(validation_data)
        
        return {
            "loss": self.loss_history[-1],
            "accuracy": self.accuracy_history[-1],
            "weights": [W.tolist() for W in self.weights],
            "biases": [b.tolist() for b in self.biases],
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
        
        # 前向传播
        _, A_values = self._forward_propagation(X)
        
        # 获取输出
        predictions = A_values[-1].T
        
        # 对于分类任务，取最大概率的类别
        if self.output_activation == "softmax":
            predictions = np.argmax(predictions, axis=1)
        
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
        X, y = self._prepare_data(test_data)
        n_samples = X.shape[1]
        
        # 预测
        _, A_values = self._forward_propagation(X)
        predictions = A_values[-1].T
        
        # 计算评估指标
        metrics = {}
        
        # 损失
        test_loss = self._compute_loss(y.T, predictions)
        metrics["loss"] = test_loss
        
        # 准确率
        accuracy = self._compute_accuracy(y.T, predictions)
        metrics["accuracy"] = accuracy
        
        # 对于分类任务，添加其他指标
        if self.output_activation in ["sigmoid", "softmax"]:
            # 精确率和召回率（简化计算）
            if self.output_activation == "sigmoid":
                # 二分类
                y_pred_binary = (predictions > 0.5).astype(int)
                y_true = y.T
            
            metrics.update(self._compute_classification_metrics(y_true, y_pred_binary))
        
        return metrics
    
    def _compute_accuracy(self, y_true: np.ndarray, y_pred: np.ndarray) -> float:
        """计算准确率"""
        if self.output_activation == "sigmoid":
            # 二分类准确率
            accuracy = np.mean((y_pred > 0.5) == y_true)
        elif self.output_activation == "softmax":
            # 多分类准确率
            accuracy = np.mean(np.argmax(y_pred, axis=1) == np.argmax(y_true, axis=1))
        else:
            # 回归任务：计算预测值与真实值的接近程度
            accuracy = 1 - np.mean(np.abs(y_pred - y_true)) / np.max(np.abs(y_true))
        
        return accuracy
    
    def _compute_classification_metrics(self, y_true: np.ndarray, y_pred: np.ndarray) -> Dict[str, float]:
        """计算分类指标"""
        # 计算混淆矩阵
        tp = np.sum((y_pred == 1) & (y_true == 1))
        tn = np.sum((y_pred == 0) & (y_true == 0))
        fp = np.sum((y_pred == 1) & (y_true == 0))
        fn = np.sum((y_pred == 0) & (y_true == 1))
        
        # 计算指标
        precision = tp / (tp + fp) if (tp + fp) > 0 else 0
        recall = tp / (tp + fn) if (tp + fn) > 0 else 0
        f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0
        
        return {
            "precision": precision,
            "recall": recall,
            "f1_score": f1
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
            "layer_sizes": self.layer_sizes,
            "activation": self.activation,
            "output_activation": self.output_activation,
            "weights": [W.tolist() for W in self.weights],
            "biases": [b.tolist() for b in self.biases],
            "metadata": self.metadata,
            "training_history": self.training_history,
            "loss_history": self.loss_history,
            "accuracy_history": self.accuracy_history,
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
        self.layer_sizes = model_data["layer_sizes"]
        self.activation = model_data["activation"]
        self.output_activation = model_data["output_activation"]
        self.weights = [np.array(W) for W in model_data["weights"]]
        self.biases = [np.array(b) for b in model_data["biases"]]
        self.metadata = model_data["metadata"]
        self.training_history = model_data["training_history"]
        self.loss_history = model_data["loss_history"]
        self.accuracy_history = model_data["accuracy_history"]
        self.is_trained = model_data["is_trained"]
        
        # 初始化优化器状态
        self._initialize_parameters()
        
        print(f"模型已加载: {path}")
    
    def get_network_summary(self) -> Dict[str, Any]:
        """获取网络结构摘要"""
        return {
            "model_name": self.model_name,
            "layer_sizes": self.layer_sizes,
            "activation": self.activation,
            "output_activation": self.output_activation,
            "n_parameters": sum(W.size + b.size for W, b in zip(self.weights, self.biases)),
            "is_trained": self.is_trained
        }
    
    def plot_learning_curves(self, save_path: Optional[Union[str, Path]] = None) -> None:
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
            
            fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
            
            # 损失曲线
            ax1.plot(range(1, len(self.loss_history) + 1), self.loss_history)
            ax1.set_xlabel("Epoch")
            ax1.set_ylabel("Loss")
            ax1.set_title(f"Loss Curve - {self.model_name}")
            ax1.grid(True)
            
            # 准确率曲线
            if self.accuracy_history:
                ax2.plot(range(1, len(self.accuracy_history) + 1), self.accuracy_history)
                ax2.set_xlabel("Epoch")
                ax2.set_ylabel("Accuracy")
                ax2.set_title(f"Accuracy Curve - {self.model_name}")
                ax2.grid(True)
            
            plt.tight_layout()
            
            if save_path:
                plt.savefig(save_path)
                print(f"学习曲线已保存: {save_path}")
            else:
                plt.show()
                
        except ImportError:
            print("未安装matplotlib，无法绘制学习曲线")