"""
AI框架集成测试

验证AI框架与构建系统的集成是否正确。
"""

import sys
import os
import tempfile
from pathlib import Path

# 添加项目路径
sys.path.insert(0, str(Path(__file__).parent))

def test_imports():
    """测试导入是否正常"""
    print("测试模块导入...")
    
    try:
        # 测试核心模块导入
        from buildsystem import BuildPaths, BuildRunner, BuildSettings
        from buildsystem.ai_integration import (
            create_ai_build_graph, 
            run_ai_training_task,
            setup_ai_build,
            AIModelTarget,
            AIBuildGraph
        )
        from buildsystem.ai import (
            Dataset, 
            ModelManager,
            TrainingConfig,
            InferenceConfig,
            LinearRegression,
            NeuralNetwork,
            ModelEvaluator,
            Config,
            Logger
        )
        print("All modules imported successfully")
        return True
    except ImportError as e:
        print(f"Module import failed: {e}")
        return False

def test_basic_functionality():
    """测试基本功能"""
    print("测试基本功能...")
    
    try:
        # 测试配置
        from buildsystem.ai import Config
        config = Config()
        lr = config.get("training.default_learning_rate")
        if lr == 0.001:
            print("Configuration functionality works correctly")
        else:
            print(f"Configuration functionality error: expected 0.001, got {lr}")
            return False
        
        # Test dataset
        from buildsystem.ai import Dataset
        import pandas as pd
        import numpy as np
        
        # Create temporary test data
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            n_samples = 100
            X = np.random.randn(n_samples, 3)
            y = X @ np.array([1.5, -2.0, 1.0]) + np.random.randn(n_samples) * 0.1
            df = pd.DataFrame(X, columns=["f1", "f2", "f3"])
            df["target"] = y
            df.to_csv(f.name, index=False)
            temp_file = f.name
        
        dataset = Dataset("test_dataset", data_path=temp_file, target_column="target")
        dataset.load_data()
        
        if dataset.info["n_samples"] == n_samples:
            print("Dataset functionality works correctly")
        else:
            print("Dataset functionality error")
            return False
        
        # Test model manager
        from buildsystem.ai import ModelManager
        model_manager = ModelManager()
        
        lr_model = LinearRegression("test_lr", {"learning_rate": 0.01})
        model_manager.register_model("test_lr", lr_model)
        
        if "test_lr" in model_manager.models:
            print("Model manager functionality works correctly")
        else:
            print("Model manager functionality error")
            return False
        
        # Test evaluator
        from buildsystem.ai import ModelEvaluator
        evaluator = ModelEvaluator()
        
        # Create simple test data
        y_true = np.array([0, 1, 1, 0, 1])
        y_pred = np.array([0, 1, 0, 0, 1])
        
        metrics = evaluator.evaluate_classification(y_true, y_pred)
        
        if "accuracy" in metrics and "precision" in metrics:
            print("Evaluator functionality works correctly")
        else:
            print("Evaluator functionality error")
            return False
        
        # Clean up temporary file
        os.unlink(temp_file)
        
        return True
        
    except Exception as e:
        print(f"Basic functionality test failed: {e}")
        return False

def test_ai_integration():
    """测试AI集成功能"""
    print("测试AI集成功能...")
    
    try:
        from buildsystem.ai_integration import AIModelTarget, AIBuildGraph
        from buildsystem import BuildPaths
        
        # 创建临时构建路径
        with tempfile.TemporaryDirectory() as temp_dir:
            build_paths = BuildPaths(
                root=Path(temp_dir),
                build="build",
                out="output",
                temp="temp"
            )
            
            # 测试AI构建图
            graph = AIBuildGraph(build_paths.root)
            
            # 创建临时测试数据
            import pandas as pd
            import numpy as np
            n_samples = 50
            X = np.random.randn(n_samples, 3)
            y = X @ np.array([1.5, -2.0, 1.0]) + np.random.randn(n_samples) * 0.1
            dataset_df = pd.DataFrame(X, columns=["f1", "f2", "f3"])
            dataset_df["target"] = y
            dataset_path = build_paths.root / "test_dataset.csv"
            dataset_df.to_csv(dataset_path, index=False)
            
            # 添加AI模型目标
            target = graph.add_ai_model_target(
                name="test_model",
                model_type="linear_regression",
                model_name="test_lr",
                dataset_path=dataset_path,
                training_config={
                    "learning_rate": 0.01,
                    "n_iterations": 10,
                    "batch_size": 16
                },
                action="train"
            )
            
            if target.name == "test_model" and target.kind == "ai_train":
                print("AI model target created successfully")
            else:
                print("AI model target creation error")
                return False
            
            # Test pipeline creation
            model_configs = [
                {
                    "model_type": "linear_regression",
                    "training_config": {
                        "learning_rate": 0.01,
                        "n_iterations": 10
                    }
                }
            ]
            
            pipeline_targets = graph.create_ai_pipeline(
                dataset_path=dataset_path,
                model_configs=model_configs,
                pipeline_name="test_pipeline"
            )
            
            if len(pipeline_targets) > 0:
                print("AI pipeline created successfully")
            else:
                print("AI pipeline creation error")
                return False
            
            return True
        
    except Exception as e:
        print(f"AI integration test failed: {e}")
        return False

def test_model_training():
    """测试模型训练功能"""
    print("测试模型训练功能...")
    
    try:
        import numpy as np
        from buildsystem.ai import LinearRegression, NeuralNetwork
        from buildsystem.ai import TrainingConfig
        
        # 创建训练配置
        training_config = TrainingConfig(
            learning_rate=0.01,
            n_iterations=10,
            batch_size=32
        )
        
        # 创建线性回归模型
        lr_model = LinearRegression("test_lr", {"learning_rate": 0.01})
        
        # 创建训练数据
        n_samples = 100
        X = np.random.randn(n_samples, 3)
        y = X @ np.array([1.5, -2.0, 1.0]) + np.random.randn(n_samples) * 0.1
        train_data = (X, y)
        
        # 训练模型
        result = lr_model.train(train_data, validation_data=None, **training_config.to_dict())
        
        if "loss" in result and lr_model.is_trained:
            print("Linear regression model trained successfully")
        else:
            print("Linear regression model training error")
            return False
        
        # Test neural network model
        nn_model = NeuralNetwork("test_nn", {
            "layer_sizes": [64, 32, 1],
            "activation": "relu",
            "learning_rate": 0.01
        })
        
        result = nn_model.train(train_data, validation_data=None, **training_config.to_dict())
        
        if "loss" in result and nn_model.is_trained:
            print("Neural network model trained successfully")
        else:
            print("Neural network model training error")
            return False
        
        # Test prediction functionality
        test_X = np.random.randn(5, 3)
        lr_predictions = lr_model.predict(test_X)
        nn_predictions = nn_model.predict(test_X)
        
        if len(lr_predictions) == 5 and len(nn_predictions) == 5:
            print("Model prediction functionality works correctly")
        else:
            print("Model prediction functionality error")
            return False
        
        return True
        
    except Exception as e:
        print(f"Model training test failed: {e}")
        return False

def run_all_tests():
    """运行所有测试"""
    print("开始运行AI框架集成测试...")
    print("=" * 50)
    
    tests = [
        ("模块导入", test_imports),
        ("基本功能", test_basic_functionality),
        ("AI集成", test_ai_integration),
        ("模型训练", test_model_training)
    ]
    
    passed = 0
    total = len(tests)
    
    for test_name, test_func in tests:
        print(f"\n{test_name}测试:")
        if test_func():
            passed += 1
        else:
            print(f"  {test_name}测试失败")
    
    print("\n" + "=" * 50)
    print(f"测试结果: {passed}/{total} 通过")
    
    if passed == total:
        print("All tests passed! AI framework integration works correctly.")
        return True
    else:
        print("Some tests failed, please check the error messages.")
        return False

if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)