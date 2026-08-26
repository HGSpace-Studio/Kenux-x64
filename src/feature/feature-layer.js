const EventEmitter = require('events');

/**
 * FeatureLayer - 单点开发层
 * 管理所有独立的功能模块，提供模块开发和测试的统一接口
 */
class FeatureLayer extends EventEmitter {
  constructor(kernel, sandbox) {
    super();
    this.kernel = kernel;
    this.sandbox = sandbox;
    this.featureModules = new Map();
    this.moduleInterfaces = new Map();
    this.activeModuleTests = new Map();
  }

  /**
   * 定义模块接口
   */
  defineModuleInterface(moduleName, interfaceDefinition) {
    this.moduleInterfaces.set(moduleName, interfaceDefinition);
    this.kernel.registerModuleInterface(moduleName, interfaceDefinition);
    console.log(`🏗️ FeatureLayer - 模块接口已定义: ${moduleName}`);
  }

  /**
   * 创建功能模块
   */
  createFeatureModule(moduleName, moduleConfig) {
    const moduleId = `${moduleName}_${Date.now()}`;
    const module = {
      id: moduleId,
      name: moduleName,
      config: moduleConfig,
      state: 'created',
      dependencies: moduleConfig.dependencies || [],
      interfaces: moduleConfig.interfaces || []
    };

    this.featureModules.set(moduleId, module);
    console.log(`🏗️ FeatureLayer - 功能模块已创建: ${moduleId}`);
    
    this.emit('module:created', module);
    return moduleId;
  }

  /**
   * 加载功能模块到内核
   */
  async loadFeatureModule(moduleId) {
    const module = this.featureModules.get(moduleId);
    if (!module) {
      throw new Error(`功能模块不存在: ${moduleId}`);
    }

    if (module.state === 'loaded') {
      console.log(`⚠️ FeatureLayer - 模块已加载: ${moduleId}`);
      return module;
    }

    // 验证依赖
    await this.validateDependencies(module);

    // 创建模块实例
    const moduleInstance = await this.createModuleInstance(module);
    
    // 加载到内核
    await this.kernel.loadModule(module.name, moduleInstance);
    
    module.state = 'loaded';
    module.loadTime = Date.now();
    
    console.log(`🔧 FeatureLayer - 模块已加载到内核: ${moduleId}`);
    this.emit('module:loaded', module);
    
    return module;
  }

  /**
   * 卸载功能模块
   */
  async unloadFeatureModule(moduleId) {
    const module = this.featureModules.get(moduleId);
    if (!module) {
      throw new Error(`功能模块不存在: ${moduleId}`);
    }

    if (module.state !== 'loaded') {
      console.log(`⚠️ FeatureLayer - 模块未加载: ${moduleId}`);
      return module;
    }

    // 从内核卸载
    await this.kernel.unloadModule(module.name);
    
    module.state = 'unloaded';
    module.unloadTime = Date.now();
    
    console.log(`🔧 FeatureLayer - 模块已从内核卸载: ${moduleId}`);
    this.emit('module:unloaded', module);
    
    return module;
  }

  /**
   * 验证模块依赖
   */
  async validateDependencies(module) {
    for (const dependency of module.dependencies) {
      if (!this.kernel.isModuleLoaded(dependency)) {
        throw new Error(`模块 ${module.name} 依赖 ${dependency} 但未加载`);
      }
    }
  }

  /**
   * 创建模块实例
   */
  async createModuleInstance(module) {
    // 根据模块名称创建对应的实例
    if (module.name === 'FileSystemModule') {
      const { FileSystemModule } = require('../examples/filesystem-module');
      return new FileSystemModule(module.config);
    } else if (module.name === 'NetworkModule') {
      const { NetworkModule } = require('../examples/network-module');
      return new NetworkModule(module.config);
    } else {
      // 默认模块基类
      class FeatureModule {
        constructor(name, config) {
          this.name = name;
          this.config = config;
          this.state = 'initialized';
        }

        async initialize() {
          this.state = 'initialized';
          console.log(`🔧 模块 ${this.name} 已初始化`);
        }

        async start() {
          this.state = 'running';
          console.log(`🚀 模块 ${this.name} 已启动`);
        }

        async stop() {
          this.state = 'stopped';
          console.log(`🛑 模块 ${this.name} 已停止`);
        }

        async getStatus() {
          return {
            name: this.name,
            state: this.state,
            config: this.config
          };
        }
      }

      return new FeatureModule(module.name, module.config);
    }
  }

  /**
   * 运行模块测试
   */
  async runModuleTest(moduleId, testCases) {
    const module = this.featureModules.get(moduleId);
    if (!module) {
      throw new Error(`功能模块不存在: ${moduleId}`);
    }

    // 创建测试环境
    const testEnvId = this.sandbox.createTestEnvironment(module.name, module.config);
    
    // 运行测试
    const results = await this.sandbox.runModuleTest(testEnvId, testCases);
    
    // 存储测试结果
    this.activeModuleTests.set(moduleId, {
      testEnvId,
      results,
      timestamp: Date.now()
    });

    console.log(`🧪 FeatureLayer - 模块测试完成: ${moduleId}`);
    return results;
  }

  /**
   * 获取模块测试结果
   */
  getModuleTestResult(moduleId) {
    const testRecord = this.activeModuleTests.get(moduleId);
    if (testRecord) {
      return this.sandbox.getTestResult(testRecord.testEnvId);
    }
    return null;
  }

  /**
   * 列出所有功能模块
   */
  listFeatureModules() {
    return Array.from(this.featureModules.values());
  }

  /**
   * 获取功能模块状态
   */
  getFeatureModule(moduleId) {
    return this.featureModules.get(moduleId);
  }

  /**
   * 模块热重载
   */
  async hotReloadModule(moduleId) {
    const module = this.featureModules.get(moduleId);
    if (!module) {
      throw new Error(`功能模块不存在: ${moduleId}`);
    }

    if (module.state === 'loaded') {
      // 先卸载
      await this.unloadFeatureModule(moduleId);
    }

    // 重新加载
    await this.loadFeatureModule(moduleId);
    
    console.log(`🔥 FeatureLayer - 模块热重载完成: ${moduleId}`);
    this.emit('module:hotreloaded', module);
  }

  /**
   * 批量加载模块
   */
  async loadModules(moduleIds) {
    const results = [];
    
    for (const moduleId of moduleIds) {
      try {
        const module = await this.loadFeatureModule(moduleId);
        results.push({ moduleId, status: 'loaded', module });
      } catch (error) {
        results.push({ moduleId, status: 'failed', error: error.message });
      }
    }
    
    return results;
  }

  /**
   * 批量卸载模块
   */
  async unloadModules(moduleIds) {
    const results = [];
    
    for (const moduleId of moduleIds) {
      try {
        const module = await this.unloadFeatureModule(moduleId);
        results.push({ moduleId, status: 'unloaded', module });
      } catch (error) {
        results.push({ moduleId, status: 'failed', error: error.message });
      }
    }
    
    return results;
  }
}

module.exports = FeatureLayer;