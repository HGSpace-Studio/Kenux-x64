"""
模型管理器

负责模型的注册、加载、保存、版本控制等管理功能。
"""

from typing import Dict, List, Optional, Type, Any, Union
from pathlib import Path
import json
import shutil
from datetime import datetime
import hashlib

from .base_model import BaseModel


class ModelManager:
    """模型管理器类"""
    
    def __init__(self, model_registry_path: Union[str, Path] = "models"):
        """
        初始化模型管理器
        
        Args:
            model_registry_path: 模型注册表路径
        """
        self.model_registry_path = Path(model_registry_path)
        self.model_registry_path.mkdir(parents=True, exist_ok=True)
        
        # 存储已注册的模型类型
        self.model_types: Dict[str, Type[BaseModel]] = {}
        
        # 存储模型实例
        self.models: Dict[str, BaseModel] = {}
        
        # 模型注册表文件
        self.registry_file = self.model_registry_path / "model_registry.json"
        
        # 加载已存在的注册表
        self._load_registry()
    
    def register_model_type(self, name: str, model_class: Type[BaseModel]) -> None:
        """
        注册模型类型
        
        Args:
            name: 模型类型名称
            model_class: 模型类
        """
        self.model_types[name] = model_class
        print(f"已注册模型类型: {name}")
    
    def create_model(self, model_type: str, model_name: str, 
                    config: Optional[Dict[str, Any]] = None) -> BaseModel:
        """
        创建模型实例
        
        Args:
            model_type: 模型类型
            model_name: 模型名称
            config: 模型配置
            
        Returns:
            模型实例
        """
        if model_type not in self.model_types:
            raise ValueError(f"未知的模型类型: {model_type}")
        
        model_class = self.model_types[model_type]
        model = model_class(model_name, config)
        
        self.models[model_name] = model
        self._update_registry()
        
        print(f"已创建模型: {model_name} (类型: {model_type})")
        return model
    
    def get_model(self, model_name: str) -> Optional[BaseModel]:
        """
        获取模型实例
        
        Args:
            model_name: 模型名称
            
        Returns:
            模型实例，如果不存在则返回None
        """
        return self.models.get(model_name)
    
    def list_models(self) -> List[Dict[str, Any]]:
        """
        列出所有注册的模型
        
        Returns:
            模型信息列表
        """
        models_info = []
        for model_name, model in self.models.items():
            models_info.append({
                "name": model_name,
                "type": type(model).__name__,
                "version": model.model_version,
                "is_trained": model.is_trained,
                "created_at": model.metadata.get("created_at", ""),
                "updated_at": model.metadata.get("last_updated", "")
            })
        return models_info
    
    def save_model(self, model_name: str, version: Optional[str] = None, 
                  save_path: Optional[Union[str, Path]] = None) -> Path:
        """
        保存模型到文件
        
        Args:
            model_name: 模型名称
            version: 模型版本（可选）
            save_path: 保存路径（可选）
            
        Returns:
            模型保存路径
        """
        model = self.get_model(model_name)
        if model is None:
            raise ValueError(f"模型不存在: {model_name}")
        
        if save_path is None:
            save_path = self.model_registry_path / f"{model_name}_{version or model.model_version}"
        
        save_path = Path(save_path)
        save_path.mkdir(parents=True, exist_ok=True)
        
        # 保存模型
        model.save(save_path)
        
        # 保存模型信息
        model.save_info(save_path / f"{model_name}.info.json")
        
        # 更新元数据
        model.update_metadata(
            saved_to=str(save_path),
            saved_at=datetime.now().isoformat()
        )
        
        # 更新注册表
        self._update_registry()
        
        print(f"模型已保存: {model_name} -> {save_path}")
        return save_path
    
    def load_model(self, model_path: Union[str, Path]) -> BaseModel:
        """
        从文件加载模型
        
        Args:
            model_path: 模型文件路径
            
        Returns:
            加载的模型实例
        """
        model_path = Path(model_path)
        
        if not model_path.exists():
            raise FileNotFoundError(f"模型文件不存在: {model_path}")
        
        # 查找对应的模型信息文件
        info_file = None
        for file in model_path.glob("*.info.json"):
            info_file = file
            break
        
        if info_file is None:
            raise FileNotFoundError("未找到模型信息文件")
        
        # 加载模型信息
        with open(info_file, "r", encoding="utf-8") as f:
            info = json.load(f)
        
        model_name = info["name"]
        model_type = info.get("type", "Unknown")
        
        # 如果模型类型已注册，创建对应实例
        if model_type in self.model_types:
            model_class = self.model_types[model_type]
            model = model_class(model_name, info.get("config", {}))
        else:
            raise ValueError(f"未知的模型类型: {model_type}")
        
        # 加载模型数据
        model.load(model_path)
        
        # 更新实例信息
        self.models[model_name] = model
        self._update_registry()
        
        print(f"模型已加载: {model_name} from {model_path}")
        return model
    
    def delete_model(self, model_name: str) -> None:
        """
        删除模型
        
        Args:
            model_name: 模型名称
        """
        if model_name not in self.models:
            raise ValueError(f"模型不存在: {model_name}")
        
        # 从注册表中移除
        del self.models[model_name]
        
        # 更新注册表文件
        self._update_registry()
        
        print(f"模型已删除: {model_name}")
    
    def get_model_versions(self, model_name: str) -> List[str]:
        """
        获取模型的所有版本
        
        Args:
            model_name: 模型名称
            
        Returns:
            版本列表
        """
        versions = []
        model_path = self.model_registry_path
        
        # 查找所有与该模型相关的文件夹
        for path in model_path.glob(f"{model_name}_*"):
            if path.is_dir():
                # 提取版本号
                parts = path.name.split("_")
                if len(parts) > 1:
                    versions.append(parts[1])
        
        return sorted(versions)
    
    def _load_registry(self) -> None:
        """加载模型注册表"""
        if self.registry_file.exists():
            try:
                with open(self.registry_file, "r", encoding="utf-8") as f:
                    registry = json.load(f)
                
                # 这里可以从注册表恢复模型实例
                # 目前只记录模型类型映射
                for model_type, model_class_name in registry.get("model_types", {}).items():
                    print(f"注册表中找到模型类型: {model_type}")
                    
            except Exception as e:
                print(f"加载注册表失败: {e}")
    
    def _update_registry(self) -> None:
        """更新模型注册表"""
        registry = {
            "model_types": {name: cls.__name__ for name, cls in self.model_types.items()},
            "models": {
                name: {
                    "type": type(model).__name__,
                    "version": model.model_version,
                    "created_at": model.metadata.get("created_at", ""),
                    "updated_at": model.metadata.get("last_updated", "")
                }
                for name, model in self.models.items()
            },
            "updated_at": datetime.now().isoformat()
        }
        
        with open(self.registry_file, "w", encoding="utf-8") as f:
            json.dump(registry, f, indent=2, ensure_ascii=False)
    
    def export_registry(self, export_path: Union[str, Path]) -> None:
        """
        导出模型注册表
        
        Args:
            export_path: 导出路径
        """
        export_path = Path(export_path)
        export_path.parent.mkdir(parents=True, exist_ok=True)
        
        # 复制注册表文件
        shutil.copy2(self.registry_file, export_path)
        
        print(f"注册表已导出: {export_path}")
    
    def import_registry(self, import_path: Union[str, Path]) -> None:
        """
        导入模型注册表
        
        Args:
            import_path: 导入路径
        """
        import_path = Path(import_path)
        
        if not import_path.exists():
            raise FileNotFoundError(f"注册表文件不存在: {import_path}")
        
        # 复制导入文件到注册表目录
        shutil.copy2(import_path, self.registry_file)
        
        # 重新加载注册表
        self._load_registry()
        
        print(f"注册表已导入: {import_path}")