"""
示例数据生成器

用于生成示例数据集，用于测试和演示。
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Tuple, Optional
from pathlib import Path
import json


def generate_regression_dataset(n_samples: int = 1000, 
                              n_features: int = 10, 
                              n_informative: int = 5,
                              noise: float = 0.1,
                              random_state: Optional[int] = None) -> Tuple[np.ndarray, np.ndarray]:
    """
    生成回归数据集
    
    Args:
        n_samples: 样本数量
        n_features: 特征数量
        n_informative: 信息特征数量
        noise: 噪声水平
        random_state: 随机种子
        
    Returns:
        特征数组和目标数组
    """
    np.random.seed(random_state)
    
    # 生成特征
    X = np.random.randn(n_samples, n_features)
    
    # 生成真实目标值（仅使用部分特征）
    true_weights = np.zeros(n_features)
    true_weights[:n_informative] = np.random.randn(n_informative)
    
    # 计算目标值
    y = X.dot(true_weights) + noise * np.random.randn(n_samples)
    
    return X, y


def generate_classification_dataset(n_samples: int = 1000,
                                   n_features: int = 10,
                                   n_informative: int = 5,
                                   n_classes: int = 2,
                                   class_sep: float = 1.0,
                                   random_state: Optional[int] = None) -> Tuple[np.ndarray, np.ndarray]:
    """
    生成分类数据集
    
    Args:
        n_samples: 样本数量
        n_features: 特征数量
        n_informative: 信息特征数量
        n_classes: 类别数量
        class_sep: 类别分离度
        random_state: 随机种子
        
    Returns:
        特征数组和目标数组
    """
    np.random.seed(random_state)
    
    # 生成特征
    X = np.random.randn(n_samples, n_features)
    
    # 生成真实标签
    true_weights = np.zeros(n_features)
    true_weights[:n_informative] = np.random.randn(n_informative)
    
    # 计算分数
    scores = X.dot(true_weights)
    
    # 根据分数生成类别
    if n_classes == 2:
        y = (scores > 0).astype(int)
    else:
        # 多分类情况
        y = np.zeros(n_samples, dtype=int)
        boundaries = np.linspace(-class_sep, class_sep, n_classes + 1)
        
        for i in range(n_classes):
            mask = (scores >= boundaries[i]) & (scores < boundaries[i + 1])
            y[mask] = i
    
    return X, y


def generate_time_series_dataset(n_samples: int = 1000,
                                n_timesteps: int = 50,
                                n_features: int = 3,
                                noise: float = 0.1,
                                trend: bool = True,
                                seasonality: bool = True,
                                random_state: Optional[int] = None) -> np.ndarray:
    """
    生成时间序列数据
    
    Args:
        n_samples: 样本数量
        n_timesteps: 时间步数
        n_features: 特征数量
        noise: 噪声水平
        trend: 是否包含趋势
        seasonality: 是否包含季节性
        random_state: 随机种子
        
    Returns:
        时间序列数据数组 (n_samples, n_timesteps, n_features)
    """
    np.random.seed(random_state)
    
    data = np.zeros((n_samples, n_timesteps, n_features))
    
    for i in range(n_samples):
        for f in range(n_features):
            # 基础信号
            signal = np.sin(2 * np.pi * np.arange(n_timesteps) / 10)
            
            # 添加趋势
            if trend:
                signal += 0.01 * np.arange(n_timesteps)
            
            # 添加季节性
            if seasonality:
                signal += 0.5 * np.sin(2 * np.pi * np.arange(n_timesteps) / 5)
            
            # 添加噪声
            signal += noise * np.random.randn(n_timesteps)
            
            data[i, :, f] = signal
    
    return data


def generate_image_dataset(n_samples: int = 1000,
                          img_size: Tuple[int, int] = (32, 32),
                          n_channels: int = 3,
                          classes: List[str] = ["cat", "dog"],
                          random_state: Optional[int] = None) -> Tuple[np.ndarray, np.ndarray]:
    """
    生成图像数据集
    
    Args:
        n_samples: 样本数量
        img_size: 图像尺寸
        n_channels: 通道数
        classes: 类别列表
        random_state: 随机种子
        
    Returns:
        图像数组和标签数组
    """
    np.random.seed(random_state)
    
    n_classes = len(classes)
    samples_per_class = n_samples // n_classes
    
    images = np.zeros((n_samples, *img_size, n_channels))
    labels = np.zeros(n_samples, dtype=int)
    
    for i, class_name in enumerate(classes):
        start_idx = i * samples_per_class
        end_idx = start_idx + samples_per_class
        
        # 生成简单的图案
        for idx in range(start_idx, end_idx):
            img = np.random.rand(*img_size, n_channels)
            
            # 为不同类别生成不同的图案
            if class_name == "cat":
                # 猫：圆形图案
                center = (img_size[0] // 2, img_size[1] // 2)
                for x in range(img_size[0]):
                    for y in range(img_size[1]):
                        dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
                        if dist < img_size[0] // 4:
                            img[x, y, :] = 1.0 - dist / (img_size[0] // 4)
            elif class_name == "dog":
                # 狗：方形图案
                size = img_size[0] // 3
                start = (img_size[0] // 2 - size // 2, img_size[1] // 2 - size // 2)
                for x in range(start[0], start[0] + size):
                    for y in range(start[1], start[1] + size):
                        if 0 <= x < img_size[0] and 0 <= y < img_size[1]:
                            img[x, y, :] = 1.0
            
            images[idx] = img
            labels[idx] = i
    
    return images, labels


def generate_text_dataset(n_samples: int = 1000,
                         text_length: int = 50,
                         vocab_size: int = 1000,
                         random_state: Optional[int] = None) -> List[str]:
    """
    生成文本数据集
    
    Args:
        n_samples: 样本数量
        text_length: 文本长度
        vocab_size: 词汇表大小
        random_state: 随机种子
        
    Returns:
        文本列表
    """
    np.random.seed(random_state)
    
    # 生成词汇表
    vocab = [f"word_{i}" for i in range(vocab_size)]
    
    # 生成文本
    texts = []
    for _ in range(n_samples):
        text = " ".join(np.random.choice(vocab, size=text_length))
        texts.append(text)
    
    return texts


def save_generated_dataset(dataset_name: str,
                          data: Tuple[np.ndarray, np.ndarray],
                          output_dir: Union[str, Path],
                          metadata: Optional[Dict] = None) -> None:
    """
    保存生成的数据集
    
    Args:
        dataset_name: 数据集名称
        data: 数据 (特征, 目标)
        output_dir: 输出目录
        metadata: 元数据字典
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    X, y = data
    
    # 保存数据
    np.savez(output_dir / f"{dataset_name}.npz", features=X, targets=y)
    
    # 保存元数据
    metadata = metadata or {}
    metadata["dataset_name"] = dataset_name
    metadata["n_samples"] = X.shape[0]
    metadata["n_features"] = X.shape[1]
    metadata["n_classes"] = len(np.unique(y)) if len(y.shape) == 1 else y.shape[1]
    metadata["created_at"] = pd.Timestamp.now().isoformat()
    
    with open(output_dir / "metadata.json", "w", encoding="utf-8") as f:
        json.dump(metadata, f, indent=2, ensure_ascii=False)
    
    print(f"数据集已保存: {output_dir}")


def create_sample_datasets(output_dir: Union[str, Path]) -> None:
    """
    创建多个示例数据集
    
    Args:
        output_dir: 输出目录
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("创建示例数据集...")
    
    # 1. 回归数据集
    X_reg, y_reg = generate_regression_dataset(n_samples=1000, n_features=10, random_state=42)
    save_generated_dataset("regression_dataset", (X_reg, y_reg), output_dir / "regression")
    
    # 2. 分类数据集（二分类）
    X_clf_bin, y_clf_bin = generate_classification_dataset(n_samples=1000, n_features=10, n_classes=2, random_state=42)
    save_generated_dataset("classification_binary_dataset", (X_clf_bin, y_clf_bin), output_dir / "classification_binary")
    
    # 3. 分类数据集（多分类）
    X_clf_multi, y_clf_multi = generate_classification_dataset(n_samples=1000, n_features=10, n_classes=3, random_state=42)
    save_generated_dataset("classification_multi_dataset", (X_clf_multi, y_clf_multi), output_dir / "classification_multi")
    
    # 4. 时间序列数据集
    ts_data = generate_time_series_dataset(n_samples=100, n_timesteps=50, n_features=3, random_state=42)
    save_generated_dataset("timeseries_dataset", (ts_data, np.zeros(len(ts_data))), output_dir / "timeseries")
    
    # 5. 图像数据集
    images, labels = generate_image_dataset(n_samples=200, img_size=(32, 32), classes=["cat", "dog"], random_state=42)
    save_generated_dataset("image_dataset", (images, labels), output_dir / "image")
    
    # 6. 文本数据集
    texts = generate_text_dataset(n_samples=500, text_length=30, vocab_size=500, random_state=42)
    # 将文本转换为数值特征（简单的词袋模型）
    word_counts = np.random.randint(0, 10, size=(len(texts), 100))  # 简化处理
    save_generated_dataset("text_dataset", (word_counts, np.random.randint(0, 2, size=len(texts))), output_dir / "text")
    
    print("所有示例数据集已创建完成")


if __name__ == "__main__":
    # 创建示例数据集
    create_sample_datasets("sample_datasets")
    
    # 加载并查看数据集信息
    from ..data.dataset import Dataset
    
    # 加载回归数据集
    reg_dataset = Dataset("regression_sample")
    reg_dataset.load_data()
    print("\n回归数据集信息:")
    print(reg_dataset.get_data_info())
    
    # 加载分类数据集
    clf_dataset = Dataset("classification_sample")
    clf_dataset.load_data()
    print("\n分类数据集信息:")
    print(clf_dataset.get_data_info())