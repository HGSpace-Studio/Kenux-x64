"""
模型评估模块

提供各种评估指标和评估工具。
"""

import numpy as np
from typing import Any, Dict, List, Optional, Union, Tuple
from sklearn.metrics import (
    accuracy_score, precision_score, recall_score, f1_score,
    mean_squared_error, mean_absolute_error, r2_score,
    roc_auc_score, confusion_matrix, classification_report
)


class ModelEvaluator:
    """模型评估器"""
    
    def __init__(self):
        """初始化评估器"""
        self.metrics_history = []
    
    def evaluate_classification(self, 
                              y_true: np.ndarray, 
                              y_pred: np.ndarray,
                              y_proba: Optional[np.ndarray] = None,
                              average: str = "binary") -> Dict[str, float]:
        """
        评估分类模型性能
        
        Args:
            y_true: 真实标签
            y_pred: 预测标签
            y_proba: 预测概率（可选）
            average: 多分类时的平均方法
            
        Returns:
            评估指标字典
        """
        metrics = {}
        
        # 基础指标
        metrics["accuracy"] = accuracy_score(y_true, y_pred)
        metrics["precision"] = precision_score(y_true, y_pred, average=average, zero_division=0)
        metrics["recall"] = recall_score(y_true, y_pred, average=average, zero_division=0)
        metrics["f1_score"] = f1_score(y_true, y_pred, average=average, zero_division=0)
        
        # ROC AUC (如果有概率输出)
        if y_proba is not None:
            if len(np.unique(y_true)) == 2:  # 二分类
                metrics["roc_auc"] = roc_auc_score(y_true, y_proba)
            else:  # 多分类
                metrics["roc_auc_ovr"] = roc_auc_score(y_true, y_proba, multi_class="ovr")
                metrics["roc_auc_ovo"] = roc_auc_score(y_true, y_proba, multi_class="ovo")
        
        # 混淆矩阵
        cm = confusion_matrix(y_true, y_pred)
        metrics["confusion_matrix"] = cm.tolist()
        
        # 分类报告
        report = classification_report(y_true, y_pred, output_dict=True, zero_division=0)
        metrics["classification_report"] = report
        
        # 保存历史
        self.metrics_history.append({
            "type": "classification",
            "metrics": metrics.copy(),
            "timestamp": np.datetime64('now')
        })
        
        return metrics
    
    def evaluate_regression(self, 
                           y_true: np.ndarray, 
                           y_pred: np.ndarray) -> Dict[str, float]:
        """
        评估回归模型性能
        
        Args:
            y_true: 真实值
            y_pred: 预测值
            
        Returns:
            评估指标字典
        """
        metrics = {}
        
        # 基础指标
        metrics["mse"] = mean_squared_error(y_true, y_pred)
        metrics["rmse"] = np.sqrt(metrics["mse"])
        metrics["mae"] = mean_absolute_error(y_true, y_pred)
        metrics["r2"] = r2_score(y_true, y_pred)
        
        # 平均绝对百分比误差
        mask = y_true != 0
        if np.any(mask):
            mape = np.mean(np.abs((y_true[mask] - y_pred[mask]) / y_true[mask])) * 100
            metrics["mape"] = mape
        
        # 保存历史
        self.metrics_history.append({
            "type": "regression",
            "metrics": metrics.copy(),
            "timestamp": np.datetime64('now')
        })
        
        return metrics
    
    def evaluate_custom(self,
                      y_true: np.ndarray,
                      y_pred: np.ndarray,
                      metrics: List[str]) -> Dict[str, float]:
        """
        使用自定义指标评估模型
        
        Args:
            y_true: 真实值
            y_pred: 预测值
            metrics: 指标列表
            
        Returns:
            评估指标字典
        """
        results = {}
        
        for metric in metrics:
            if metric == "mse":
                results["mse"] = mean_squared_error(y_true, y_pred)
            elif metric == "rmse":
                results["rmse"] = np.sqrt(mean_squared_error(y_true, y_pred))
            elif metric == "mae":
                results["mae"] = mean_absolute_error(y_true, y_pred)
            elif metric == "r2":
                results["r2"] = r2_score(y_true, y_pred)
            elif metric == "accuracy":
                results["accuracy"] = np.mean(np.round(y_pred) == y_true)
            elif metric == "mape":
                mask = y_true != 0
                if np.any(mask):
                    results["mape"] = np.mean(np.abs((y_true[mask] - y_pred[mask]) / y_true[mask])) * 100
                else:
                    results["mape"] = np.nan
            else:
                raise ValueError(f"未知的指标: {metric}")
        
        # 保存历史
        self.metrics_history.append({
            "type": "custom",
            "metrics": results.copy(),
            "timestamp": np.datetime64('now')
        })
        
        return results
    
    def cross_validate(self,
                      model: Any,
                      X: np.ndarray,
                      y: np.ndarray,
                      cv: int = 5,
                      scoring: str = "accuracy",
                      **kwargs) -> Dict[str, Any]:
        """
        交叉验证评估
        
        Args:
            model: 要评估的模型
            X: 特征数据
            y: 目标数据
            cv: 交叉验证折数
            scoring: 评估指标
            **kwargs: 其他参数
            
        Returns:
            交叉验证结果
        """
        try:
            from sklearn.model_selection import cross_validate
        except ImportError:
            raise ImportError("scikit-learn is required for cross-validation")
        
        # 执行交叉验证
        cv_results = cross_validate(
            model, X, y,
            cv=cv,
            scoring=scoring,
            **kwargs
        )
        
        # 计算统计量
        results = {
            f"test_{scoring}_mean": np.mean(cv_results[f"test_{scoring}"]),
            f"test_{scoring}_std": np.std(cv_results[f"test_{scoring}"]),
            f"fit_time_mean": np.mean(cv_results["fit_time"]),
            f"score_time_mean": np.mean(cv_results["score_time"]),
            "cv_folds": cv,
            "n_samples": len(y)
        }
        
        # 保存历史
        self.metrics_history.append({
            "type": "cross_validation",
            "metrics": results.copy(),
            "timestamp": np.datetime64('now')
        })
        
        return results
    
    def compare_models(self,
                      models: Dict[str, Any],
                      X_test: np.ndarray,
                      y_test: np.ndarray,
                      task_type: str = "classification") -> Dict[str, Dict[str, float]]:
        """
        比较多个模型的性能
        
        Args:
            models: 模型字典 {name: model}
            X_test: 测试特征
            y_test: 测试目标
            task_type: 任务类型 ("classification" 或 "regression")
            
        Returns:
            模型比较结果
        """
        comparison_results = {}
        
        for name, model in models.items():
            try:
                # 预测
                if hasattr(model, 'predict_proba') and task_type == "classification":
                    y_pred = model.predict(X_test)
                    y_proba = model.predict_proba(X_test) if hasattr(model, 'predict_proba') else None
                    metrics = self.evaluate_classification(y_test, y_pred, y_proba)
                else:
                    y_pred = model.predict(X_test)
                    metrics = self.evaluate_regression(y_test, y_pred) if task_type == "regression" else self.evaluate_classification(y_test, y_pred)
                
                comparison_results[name] = metrics
                
            except Exception as e:
                print(f"评估模型 {name} 时出错: {e}")
                comparison_results[name] = {"error": str(e)}
        
        # 保存历史
        self.metrics_history.append({
            "type": "model_comparison",
            "models": list(models.keys()),
            "metrics": comparison_results,
            "timestamp": np.datetime64('now')
        })
        
        return comparison_results
    
    def get_metrics_summary(self) -> Dict[str, Any]:
        """
        获取评估指标的摘要信息
        
        Returns:
            摘要信息
        """
        if not self.metrics_history:
            return {"message": "没有评估历史"}
        
        summary = {
            "total_evaluations": len(self.metrics_history),
            "by_type": {},
            "latest_metrics": self.metrics_history[-1]["metrics"]
        }
        
        # 按类型统计
        for record in self.metrics_history:
            eval_type = record["type"]
            if eval_type not in summary["by_type"]:
                summary["by_type"][eval_type] = 0
            summary["by_type"][eval_type] += 1
        
        return summary
    
    def clear_history(self) -> None:
        """清除评估历史"""
        self.metrics_history = []
        print("评估历史已清除")


# 便捷函数
def evaluate_model(model: Any,
                  X_test: np.ndarray,
                  y_test: np.ndarray,
                  task_type: str = "classification",
                  **kwargs) -> Dict[str, float]:
    """
    评估单个模型的便捷函数
    
    Args:
        model: 要评估的模型
        X_test: 测试特征
        y_test: 测试目标
        task_type: 任务类型
        **kwargs: 其他参数
        
    Returns:
        评估指标字典
    """
    evaluator = ModelEvaluator()
    
    if task_type == "classification":
        y_pred = model.predict(X_test)
        y_proba = model.predict_proba(X_test) if hasattr(model, 'predict_proba') else None
        return evaluator.evaluate_classification(y_test, y_pred, y_proba, **kwargs)
    elif task_type == "regression":
        y_pred = model.predict(X_test)
        return evaluator.evaluate_regression(y_test, y_pred, **kwargs)
    else:
        raise ValueError(f"不支持的任务类型: {task_type}")


def compare_models(models: Dict[str, Any],
                  X_test: np.ndarray,
                  y_test: np.ndarray,
                  task_type: str = "classification") -> Dict[str, Dict[str, float]]:
    """
    比较多个模型的便捷函数
    
    Args:
        models: 模型字典 {name: model}
        X_test: 测试特征
        y_test: 测试目标
        task_type: 任务类型
        
    Returns:
        模型比较结果
    """
    evaluator = ModelEvaluator()
    return evaluator.compare_models(models, X_test, y_test, task_type)