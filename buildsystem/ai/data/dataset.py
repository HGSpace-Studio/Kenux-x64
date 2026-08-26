"""
数据集类

提供数据集加载、管理、分割等功能。
"""

import numpy as np
import pandas as pd
from typing import Any, Dict, List, Optional, Tuple, Union
from pathlib import Path
import json
import pickle
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, MinMaxScaler


class Dataset:
    """数据集类"""
    
    def __init__(self, 
                 name: str,
                 data_path: Optional[Union[str, Path]] = None,
                 target_column: Optional[str] = None,
                 feature_columns: Optional[List[str]] = None):
        """
        初始化数据集
        
        Args:
            name: 数据集名称
            data_path: 数据文件路径
            target_column: 目标列名
            feature_columns: 特征列名列表
        """
        self.name = name
        self.data_path = Path(data_path) if data_path else None
        self.target_column = target_column
        self.feature_columns = feature_columns
        
        # 数据存储
        self.data: Optional[pd.DataFrame] = None
        self.features: Optional[np.ndarray] = None
        self.targets: Optional[np.ndarray] = None
        
        # 数据信息
        self.info = {
            "n_samples": 0,
            "n_features": 0,
            "n_classes": 0,
            "feature_names": [],
            "target_name": target_column,
            "data_type": None,
            "missing_values": 0,
            "created_at": None,
            "loaded_at": None
        }
        
        # 数据分割
        self.train_features: Optional[np.ndarray] = None
        self.train_targets: Optional[np.ndarray] = None
        self.val_features: Optional[np.ndarray] = None
        self.val_targets: Optional[np.ndarray] = None
        self.test_features: Optional[np.ndarray] = None
        self.test_targets: Optional[np.ndarray] = None
        
        # 数据预处理器
        self.scaler = None
        self.feature_scaler = None
        self.target_scaler = None
        
    def load_data(self, **kwargs) -> None:
        """
        加载数据
        
        Args:
            **kwargs: 加载参数
        """
        if self.data_path is None:
            raise ValueError("未指定数据文件路径")
        
        if not self.data_path.exists():
            raise FileNotFoundError(f"数据文件不存在: {self.data_path}")
        
        # 根据文件扩展名选择加载方式
        if self.data_path.suffix.lower() == '.csv':
            self.data = pd.read_csv(self.data_path, **kwargs)
        elif self.data_path.suffix.lower() in ['.xlsx', '.xls']:
            self.data = pd.read_excel(self.data_path, **kwargs)
        elif self.data_path.suffix.lower() == '.json':
            self.data = pd.read_json(self.data_path, **kwargs)
        elif self.data_path.suffix.lower() == '.pkl':
            with open(self.data_path, 'rb') as f:
                self.data = pickle.load(f)
        elif self.data_path.suffix.lower() == '.npz':
            data_dict = np.load(self.data_path)
            # 假设npz文件包含'features'和'targets'
            self.features = data_dict['features']
            self.targets = data_dict['targets']
            self._update_info()
            return
        else:
            raise ValueError(f"不支持的数据文件格式: {self.data_path.suffix}")
        
        # 更新数据信息
        self.info["created_at"] = self.data_path.stat().st_mtime
        self.info["loaded_at"] = pd.Timestamp.now().isoformat()
        
        # 更新特征和目标
        self._extract_features_and_targets()
        self._update_info()
        
        print(f"数据集 '{self.name}' 加载成功，共 {self.info['n_samples']} 个样本")
    
    def _extract_features_and_targets(self) -> None:
        """提取特征和目标"""
        if self.data is None:
            raise ValueError("数据未加载")
        
        # 提取特征
        if self.feature_columns:
            self.features = self.data[self.feature_columns].values
        else:
            # 如果没有指定特征列，除了目标列外的所有列都是特征
            if self.target_column:
                self.features = self.data.drop(columns=[self.target_column]).values
            else:
                self.features = self.data.values
        
        # 提取目标
        if self.target_column:
            self.targets = self.data[self.target_column].values
        else:
            # 如果没有指定目标列，使用最后一列
            self.targets = self.data.iloc[:, -1].values
    
    def _update_info(self) -> None:
        """更新数据集信息"""
        if self.features is not None:
            self.info["n_samples"] = self.features.shape[0]
            self.info["n_features"] = self.features.shape[1]
            self.info["feature_names"] = list(self.data.columns) if self.data is not None else []
            
            # 检查缺失值
            if self.data is not None:
                self.info["missing_values"] = self.data.isnull().sum().sum()
            
            # 检查目标类型
            if self.targets is not None:
                unique_targets = np.unique(self.targets)
                if len(unique_targets) < 20:  # 假设唯一值少于20的是分类问题
                    self.info["data_type"] = "classification"
                    self.info["n_classes"] = len(unique_targets)
                else:
                    self.info["data_type"] = "regression"
    
    def get_features(self) -> np.ndarray:
        """获取特征"""
        if self.features is None:
            raise ValueError("特征未加载")
        return self.features
    
    def get_targets(self) -> np.ndarray:
        """获取目标"""
        if self.targets is None:
            raise ValueError("目标未加载")
        return self.targets
    
    def get_data_info(self) -> Dict[str, Any]:
        """获取数据集信息"""
        return self.info.copy()
    
    def split_data(self, 
                  test_size: float = 0.2, 
                  val_size: float = 0.1, 
                  random_state: Optional[int] = None,
                  stratify: bool = False) -> None:
        """
        分割数据集
        
        Args:
            test_size: 测试集比例
            val_size: 验证集比例
            random_state: 随机种子
            stratify: 是否分层抽样
        """
        if self.features is None or self.targets is None:
            raise ValueError("数据未加载")
        
        if val_size == 0:
            # 只分割训练集和测试集
            stratify_param = self.targets if stratify else None
            self.train_features, self.test_features, self.train_targets, self.test_targets = train_test_split(
                self.features, self.targets, 
                test_size=test_size, 
                random_state=random_state,
                stratify=stratify_param
            )
        else:
            # 分割训练集、验证集和测试集
            # 先分割出测试集
            stratify_param = self.targets if stratify else None
            X_temp, self.test_features, y_temp, self.test_targets = train_test_split(
                self.features, self.targets, 
                test_size=test_size, 
                random_state=random_state,
                stratify=stratify_param
            )
            
            # 从剩余数据中分割出验证集
            val_adjusted_size = val_size / (1 - test_size)
            stratify_param = y_temp if stratify else None
            self.train_features, self.val_features, self.train_targets, self.val_targets = train_test_split(
                X_temp, y_temp,
                test_size=val_adjusted_size,
                random_state=random_state,
                stratify=stratify_param
            )
        
        print(f"数据集分割完成: 训练集 {len(self.train_features)} 样本")
        if self.val_features is not None:
            print(f"验证集 {len(self.val_features)} 样本")
        print(f"测试集 {len(self.test_features)} 样本")
    
    def normalize_features(self, method: str = "standard", feature_range: Optional[Tuple[float, float]] = None) -> None:
        """
        标准化特征
        
        Args:
            method: 标准化方法 ("standard", "minmax", "robust")
            feature_range: 特征范围（用于minmax）
        """
        if self.features is None:
            raise ValueError("特征未加载")
        
        if method == "standard":
            self.feature_scaler = StandardScaler()
            self.features = self.feature_scaler.fit_transform(self.features)
            if self.train_features is not None:
                self.train_features = self.feature_scaler.transform(self.train_features)
            if self.val_features is not None:
                self.val_features = self.feature_scaler.transform(self.val_features)
            if self.test_features is not None:
                self.test_features = self.feature_scaler.transform(self.test_features)
        elif method == "minmax":
            self.feature_scaler = MinMaxScaler(feature_range=feature_range)
            self.features = self.feature_scaler.fit_transform(self.features)
            if self.train_features is not None:
                self.train_features = self.feature_scaler.transform(self.train_features)
            if self.val_features is not None:
                self.val_features = self.feature_scaler.transform(self.val_features)
            if self.test_features is not None:
                self.test_features = self.feature_scaler.transform(self.test_features)
        else:
            raise ValueError(f"不支持的标准化方法: {method}")
        
        print(f"特征已 {method} 标准化")
    
    def normalize_targets(self, method: str = "standard", feature_range: Optional[Tuple[float, float]] = None) -> None:
        """
        标准化目标
        
        Args:
            method: 标准化方法 ("standard", "minmax")
            feature_range: 特征范围
        """
        if self.targets is None:
            raise ValueError("目标未加载")
        
        if method == "standard":
            self.target_scaler = StandardScaler()
            self.targets = self.target_scaler.fit_transform(self.targets.reshape(-1, 1)).flatten()
            if self.train_targets is not None:
                self.train_targets = self.target_scaler.transform(self.train_targets.reshape(-1, 1)).flatten()
            if self.val_targets is not None:
                self.val_targets = self.target_scaler.transform(self.val_targets.reshape(-1, 1)).flatten()
            if self.test_targets is not None:
                self.test_targets = self.target_scaler.transform(self.test_targets.reshape(-1, 1)).flatten()
        elif method == "minmax":
            self.target_scaler = MinMaxScaler(feature_range=feature_range)
            self.targets = self.target_scaler.fit_transform(self.targets.reshape(-1, 1)).flatten()
            if self.train_targets is not None:
                self.train_targets = self.target_scaler.transform(self.train_targets.reshape(-1, 1)).flatten()
            if self.val_targets is not None:
                self.val_targets = self.target_scaler.transform(self.val_targets.reshape(-1, 1)).flatten()
            if self.test_targets is not None:
                self.test_targets = self.target_scaler.transform(self.test_targets.reshape(-1, 1)).flatten()
        else:
            raise ValueError(f"不支持的标准化方法: {method}")
        
        print(f"目标已 {method} 标准化")
    
    def inverse_transform_targets(self, targets: np.ndarray) -> np.ndarray:
        """反向转换目标值"""
        if self.target_scaler is None:
            raise ValueError("目标尚未标准化")
        
        return self.target_scaler.inverse_transform(targets.reshape(-1, 1)).flatten()
    
    def get_train_data(self) -> Tuple[np.ndarray, np.ndarray]:
        """获取训练数据"""
        if self.train_features is None or self.train_targets is None:
            raise ValueError("训练数据未分割")
        return self.train_features, self.train_targets
    
    def get_val_data(self) -> Tuple[np.ndarray, np.ndarray]:
        """获取验证数据"""
        if self.val_features is None or self.val_targets is None:
            raise ValueError("验证数据未分割")
        return self.val_features, self.val_targets
    
    def get_test_data(self) -> Tuple[np.ndarray, np.ndarray]:
        """获取测试数据"""
        if self.test_features is None or self.test_targets is None:
            raise ValueError("测试数据未分割")
        return self.test_features, self.test_targets
    
    def save_dataset(self, save_dir: Union[str, Path]) -> None:
        """
        保存数据集
        
        Args:
            save_dir: 保存目录
        """
        save_dir = Path(save_dir)
        save_dir.mkdir(parents=True, exist_ok=True)
        
        # 保存数据
        if self.features is not None and self.targets is not None:
            np.savez(save_dir / "data.npz",
                    features=self.features,
                    targets=self.targets)
        
        # 保存分割后的数据
        if self.train_features is not None and self.train_targets is not None:
            np.savez(save_dir / "train.npz",
                    features=self.train_features,
                    targets=self.train_targets)
        
        if self.val_features is not None and self.val_targets is not None:
            np.savez(save_dir / "val.npz",
                    features=self.val_features,
                    targets=self.val_targets)
        
        if self.test_features is not None and self.test_targets is not None:
            np.savez(save_dir / "test.npz",
                    features=self.test_features,
                    targets=self.test_targets)
        
        # 保存预处理器
        if self.feature_scaler is not None:
            with open(save_dir / "feature_scaler.pkl", "wb") as f:
                pickle.dump(self.feature_scaler, f)
        
        if self.target_scaler is not None:
            with open(save_dir / "target_scaler.pkl", "wb") as f:
                pickle.dump(self.target_scaler, f)
        
        # 保存数据集信息
        with open(save_dir / "dataset_info.json", "w", encoding="utf-8") as f:
            json.dump(self.info, f, indent=2, ensure_ascii=False)
        
        print(f"数据集已保存: {save_dir}")
    
    def load_dataset(self, load_dir: Union[str, Path]) -> None:
        """
        加载数据集
        
        Args:
            load_dir: 数据集目录
        """
        load_dir = Path(load_dir)
        
        if not load_dir.exists():
            raise FileNotFoundError(f"数据集目录不存在: {load_dir}")
        
        # 加载数据
        data_file = load_dir / "data.npz"
        if data_file.exists():
            data = np.load(data_file)
            self.features = data["features"]
            self.targets = data["targets"]
            self._update_info()
        
        # 加载分割后的数据
        train_file = load_dir / "train.npz"
        if train_file.exists():
            train_data = np.load(train_file)
            self.train_features = train_data["features"]
            self.train_targets = train_data["targets"]
        
        val_file = load_dir / "val.npz"
        if val_file.exists():
            val_data = np.load(val_file)
            self.val_features = val_data["features"]
            self.val_targets = val_data["targets"]
        
        test_file = load_dir / "test.npz"
        if test_file.exists():
            test_data = np.load(test_file)
            self.test_features = test_data["features"]
            self.test_targets = test_data["targets"]
        
        # 加载预处理器
        feature_scaler_file = load_dir / "feature_scaler.pkl"
        if feature_scaler_file.exists():
            with open(feature_scaler_file, "rb") as f:
                self.feature_scaler = pickle.load(f)
        
        target_scaler_file = load_dir / "target_scaler.pkl"
        if target_scaler_file.exists():
            with open(target_scaler_file, "rb") as f:
                self.target_scaler = pickle.load(f)
        
        # 加载数据集信息
        info_file = load_dir / "dataset_info.json"
        if info_file.exists():
            with open(info_file, "r", encoding="utf-8") as f:
                self.info = json.load(f)
        
        print(f"数据集已加载: {load_dir}")
    
    def create_batch_generator(self, 
                              data_type: str = "train",
                              batch_size: int = 32,
                              shuffle: bool = True) -> "BatchGenerator":
        """
        创建批数据生成器
        
        Args:
            data_type: 数据类型 ("train", "val", "test")
            batch_size: 批次大小
            shuffle: 是否打乱数据
            
        Returns:
            批数据生成器
        """
        if data_type == "train":
            features = self.train_features
            targets = self.train_targets
        elif data_type == "val":
            features = self.val_features
            targets = self.val_targets
        elif data_type == "test":
            features = self.test_features
            targets = self.test_targets
        else:
            raise ValueError(f"未知的数据类型: {data_type}")
        
        if features is None or targets is None:
            raise ValueError(f"{data_type} 数据未加载")
        
        return BatchGenerator(features, targets, batch_size, shuffle)
    
    def get_class_distribution(self) -> Dict[str, int]:
        """获取类别分布（仅适用于分类问题）"""
        if self.info["data_type"] != "classification":
            return {}
        
        unique, counts = np.unique(self.targets, return_counts=True)
        return {str(cls): int(count) for cls, count in zip(unique, counts)}
    
    def get_feature_statistics(self) -> Dict[str, Dict[str, float]]:
        """获取特征统计信息"""
        if self.features is None:
            return {}
        
        stats = {}
        for i in range(self.features.shape[1]):
            col = self.features[:, i]
            stats[f"feature_{i}"] = {
                "mean": float(np.mean(col)),
                "std": float(np.std(col)),
                "min": float(np.min(col)),
                "max": float(np.max(col)),
                "median": float(np.median(col))
            }
        
        return stats
    
    def __len__(self) -> int:
        """返回数据集大小"""
        if self.features is not None:
            return len(self.features)
        return 0
    
    def __getitem__(self, idx: int) -> Tuple[np.ndarray, np.ndarray]:
        """获取单个样本"""
        if self.features is None or self.targets is None:
            raise ValueError("数据未加载")
        return self.features[idx], self.targets[idx]


class BatchGenerator:
    """批数据生成器"""
    
    def __init__(self, 
                 features: np.ndarray,
                 targets: np.ndarray,
                 batch_size: int,
                 shuffle: bool = True):
        """
        初始化批数据生成器
        
        Args:
            features: 特征数组
            targets: 目标数组
            batch_size: 批次大小
            shuffle: 是否打乱数据
        """
        self.features = features
        self.targets = targets
        self.batch_size = batch_size
        self.shuffle = shuffle
        self.n_samples = len(features)
        self.current_idx = 0
        
        if shuffle:
            self.indices = np.random.permutation(self.n_samples)
        else:
            self.indices = np.arange(self.n_samples)
    
    def __iter__(self):
        """迭代器"""
        self.current_idx = 0
        return self
    
    def __next__(self) -> Tuple[np.ndarray, np.ndarray]:
        """获取下一个批次"""
        if self.current_idx >= self.n_samples:
            # 重置索引
            if self.shuffle:
                self.indices = np.random.permutation(self.n_samples)
            self.current_idx = 0
            raise StopIteration
        
        # 计算批次的结束索引
        end_idx = min(self.current_idx + self.batch_size, self.n_samples)
        batch_indices = self.indices[self.current_idx:end_idx]
        
        # 获取批次数据
        batch_features = self.features[batch_indices]
        batch_targets = self.targets[batch_indices]
        
        self.current_idx = end_idx
        
        return batch_features, batch_targets
    
    def __len__(self) -> int:
        """返回批次数"""
        return (self.n_samples + self.batch_size - 1) // self.batch_size