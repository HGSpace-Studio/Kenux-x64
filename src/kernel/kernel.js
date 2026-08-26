const EventEmitter = require('events');

/**
 * KAL (Kernel Abstraction Layer) - 内核抽象层
 * 负责管理所有模块的生命周期和模块间的通信
 */
class Kernel extends EventEmitter {
  constructor() {
    super();
    this.modules = new Map();
    this.moduleInterfaces = new Map();
    this.dependencies = new Map();
    this.initialized = false;
  }

  /**
   * 初始化内核
   */
  async initialize() {
    console.log('🚀 KAL - 内核初始化中...');
    this.initialized = true;
    this.emit('kernel:initialized');
  }

  /**
   * 注册模块接口定义
   */
  registerModuleInterface(moduleName, interfaceDefinition) {
    this.moduleInterfaces.set(moduleName, interfaceDefinition);
    console.log(`📋 KAL - 模块接口已注册: ${moduleName}`);
  }

  /**
   * 加载模块
   */
  async loadModule(moduleName, moduleInstance) {
    if (this.modules.has(moduleName)) {
      throw new Error(`模块 ${moduleName} 已经加载`);
    }

    // 验证模块接口
    const interfaceDefinition = this.moduleInterfaces.get(moduleName);
    if (interfaceDefinition) {
      this.validateModuleInterface(moduleInstance, interfaceDefinition);
    }

    this.modules.set(moduleName, moduleInstance);
    console.log(`🔧 KAL - 模块已加载: ${moduleName}`);
    
    this.emit('module:loaded', { moduleName, moduleInstance });
    return moduleInstance;
  }

  /**
   * 卸载模块
   */
  async unloadModule(moduleName) {
    if (!this.modules.has(moduleName)) {
      throw new Error(`模块 ${moduleName} 未加载`);
    }

    const moduleInstance = this.modules.get(moduleName);
    this.modules.delete(moduleName);
    
    console.log(`🔧 KAL - 模块已卸载: ${moduleName}`);
    this.emit('module:unloaded', { moduleName, moduleInstance });
  }

  /**
   * 验证模块接口
   */
  validateModuleInterface(module, interfaceDefinition) {
    const requiredMethods = Object.keys(interfaceDefinition);
    
    for (const method of requiredMethods) {
      if (typeof module[method] !== 'function') {
        throw new Error(`模块 ${module.constructor.name} 缺少必需方法: ${method}`);
      }
    }
    
    console.log(`✅ KAL - 模块接口验证通过: ${module.constructor.name}`);
  }

  /**
   * 获取模块实例
   */
  getModule(moduleName) {
    return this.modules.get(moduleName);
  }

  /**
   * 列出所有已加载的模块
   */
  listModules() {
    return Array.from(this.modules.keys());
  }

  /**
   * 检查模块是否已加载
   */
  isModuleLoaded(moduleName) {
    return this.modules.has(moduleName);
  }
}

module.exports = Kernel;