/**
 * 单点测试器 (Sandbox) - 主入口
 * 操作系统模块化开发和测试框架
 */

const Kernel = require('./kernel/kernel');
const Sandbox = require('./sandbox/sandbox');
const FeatureLayer = require('./feature/feature-layer');
const { FileSystemModule, filesystemInterface } = require('./examples/filesystem-module');
const { NetworkModule, networkInterface } = require('./examples/network-module');

class SandboxSystem {
  constructor() {
    this.kernel = new Kernel();
    this.sandbox = new Sandbox(this.kernel);
    this.featureLayer = new FeatureLayer(this.kernel, this.sandbox);
    this.isRunning = false;
  }

  /**
   * 初始化系统
   */
  async initialize() {
    console.log('🚀 初始化单点测试器系统...');
    
    // 初始化内核
    await this.kernel.initialize();
    
    // 定义模块接口
    this.featureLayer.defineModuleInterface('FileSystemModule', filesystemInterface);
    this.featureLayer.defineModuleInterface('NetworkModule', networkInterface);
    
    console.log('✅ 系统初始化完成');
    this.isRunning = true;
  }

  /**
   * 创建并加载模块
   */
  async createAndLoadModule(moduleName, moduleConfig) {
    if (!this.isRunning) {
      throw new Error('系统未初始化');
    }

    const moduleId = this.featureLayer.createFeatureModule(moduleName, moduleConfig);
    await this.featureLayer.loadFeatureModule(moduleId);
    return moduleId;
  }

  /**
   * 运行模块测试
   */
  async runModuleTest(moduleName, testCases) {
    if (!this.isRunning) {
      throw new Error('系统未初始化');
    }

    // 查找对应的模块
    const modules = this.featureLayer.listFeatureModules();
    const module = modules.find(m => m.name === moduleName);
    
    if (!module) {
      throw new Error(`模块 ${moduleName} 不存在`);
    }

    return await this.featureLayer.runModuleTest(module.id, testCases);
  }

  /**
   * 获取测试结果
   */
  getTestResult(moduleName) {
    const modules = this.featureLayer.listFeatureModules();
    const module = modules.find(m => m.name === moduleName);
    
    if (module) {
      return this.featureLayer.getModuleTestResult(module.id);
    }
    return null;
  }

  /**
   * 热重载模块
   */
  async hotReloadModule(moduleName) {
    if (!this.isRunning) {
      throw new Error('系统未初始化');
    }

    const modules = this.featureLayer.listFeatureModules();
    const module = modules.find(m => m.name === moduleName);
    
    if (!module) {
      throw new Error(`模块 ${moduleName} 不存在`);
    }

    await this.featureLayer.hotReloadModule(module.id);
  }

  /**
   * 卸载模块
   */
  async unloadModule(moduleName) {
    if (!this.isRunning) {
      throw new Error('系统未初始化');
    }

    const modules = this.featureLayer.listFeatureModules();
    const module = modules.find(m => m.name === moduleName);
    
    if (module) {
      await this.featureLayer.unloadFeatureModule(module.id);
    }
  }

  /**
   * 获取系统状态
   */
  getStatus() {
    return {
      isRunning: this.isRunning,
      kernel: {
        initialized: this.kernel.initialized,
        modules: this.kernel.listModules()
      },
      sandbox: {
        activeTests: this.sandbox.getActiveTestEnvironments().length,
        testResults: this.sandbox.testResults.size
      },
      featureLayer: {
        modules: this.featureLayer.listFeatureModules().length,
        loadedModules: this.featureLayer.listFeatureModules().filter(m => m.state === 'loaded').length
      }
    };
  }

  /**
   * 关闭系统
   */
  async shutdown() {
    console.log('🔄 关闭单点测试器系统...');
    
    // 卸载所有模块
    const modules = this.featureLayer.listFeatureModules();
    for (const module of modules) {
      if (module.state === 'loaded') {
        await this.featureLayer.unloadFeatureModule(module.id);
      }
    }
    
    // 清理测试环境
    for (const testEnv of this.sandbox.getActiveTestEnvironments()) {
      this.sandbox.destroyTestEnvironment(testEnv.id);
    }
    
    this.isRunning = false;
    console.log('✅ 系统已关闭');
  }
}

// 示例使用
async function demo() {
  const system = new SandboxSystem();
  
  try {
    // 初始化系统
    console.log('🚀 初始化系统...');
    await system.initialize();
    
    console.log('📊 初始系统状态:', JSON.stringify(system.getStatus(), null, 2));
    
    // 创建文件系统模块
    console.log('\n🔧 创建文件系统模块...');
    const fsModuleId = await system.createAndLoadModule('FileSystemModule', {
      maxFiles: 100,
      maxSize: '10MB'
    });
    
    // 创建网络模块
    console.log('\n🔧 创建网络模块...');
    const netModuleId = await system.createAndLoadModule('NetworkModule', {
      maxConnections: 50,
      bufferSize: 8192
    });
    
    console.log('📊 模块加载后状态:', JSON.stringify(system.getStatus(), null, 2));
    
    // 运行文件系统测试
    console.log('\n🧪 运行文件系统测试...');
    const fsTestCases = [
      {
        name: '创建文件测试',
        test: async (kernel, config) => {
          const fsModule = kernel.getModule('FileSystemModule');
          await fsModule.initialize();
          const file = await fsModule.createFile('/test/hello.txt', 'Hello, World!');
          return file.content === 'Hello, World!';
        }
      },
      {
        name: '读取文件测试',
        test: async (kernel, config) => {
          const fsModule = kernel.getModule('FileSystemModule');
          await fsModule.initialize();
          await fsModule.createFile('/test/hello.txt', 'Hello, World!');
          const content = await fsModule.readFile('/test/hello.txt');
          return content === 'Hello, World!';
        }
      },
      {
        name: '列出文件测试',
        test: async (kernel, config) => {
          const fsModule = kernel.getModule('FileSystemModule');
          await fsModule.initialize();
          await fsModule.createFile('/test/file1.txt', 'Content 1');
          await fsModule.createFile('/test/file2.txt', 'Content 2');
          const files = await fsModule.listFiles('/test');
          return files.length === 2;
        }
      }
    ];
    
    const fsTestResults = await system.runModuleTest('FileSystemModule', fsTestCases);
    console.log('📊 文件系统测试结果:', fsTestResults);
    
    // 运行网络模块测试
    console.log('\n🧪 运行网络模块测试...');
    const netTestCases = [
      {
        name: '监听端口测试',
        test: async (kernel, config) => {
          const netModule = kernel.getModule('NetworkModule');
          await netModule.initialize();
          const listenerId = await netModule.listen(8080, 'localhost', () => {});
          return listenerId === 'localhost:8080';
        }
      },
      {
        name: '建立连接测试',
        test: async (kernel, config) => {
          const netModule = kernel.getModule('NetworkModule');
          await netModule.initialize();
          const connectionId = await netModule.connect('example.com', 80);
          return connectionId.startsWith('conn_');
        }
      },
      {
        name: '添加路由测试',
        test: async (kernel, config) => {
          const netModule = kernel.getModule('NetworkModule');
          await netModule.initialize();
          await netModule.addRoute('/api/test', () => ({ success: true }));
          return netModule.routes.has('/api/test');
        }
      }
    ];
    
    const netTestResults = await system.runModuleTest('NetworkModule', netTestCases);
    console.log('📊 网络模块测试结果:', netTestResults);
    
    // 热重载测试
    console.log('\n🔥 热重载文件系统模块...');
    await system.hotReloadModule('FileSystemModule');
    
    // 获取最终状态
    console.log('\n📊 最终系统状态:', JSON.stringify(system.getStatus(), null, 2));
    
  } catch (error) {
    console.error('❌ 演示过程中发生错误:', error);
    console.error('错误堆栈:', error.stack);
  } finally {
    try {
      await system.shutdown();
    } catch (shutdownError) {
      console.error('关闭系统时发生错误:', shutdownError);
    }
  }
}

// 如果直接运行此文件，执行演示
if (require.main === module) {
  demo().catch(console.error);
}

module.exports = SandboxSystem;