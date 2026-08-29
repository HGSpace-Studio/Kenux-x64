/**
 * 简单的测试运行器，用于验证单点测试器系统
 */

const { FileSystemModule, filesystemInterface } = require('./src/examples/filesystem-module');
const { NetworkModule, networkInterface } = require('./src/examples/network-module');
const Kernel = require('./src/kernel/kernel');
const Sandbox = require('./src/sandbox/sandbox');
const FeatureLayer = require('./src/feature/feature-layer');

console.log('🧪 开始运行单点测试器系统测试...\n');

async function runTests() {
  const failures = [];
  const recordFailure = (name, error) => {
    const message = error instanceof Error ? error.message : String(error);
    failures.push({ name, message });
    console.error(`❌ ${name}: ${message}`);
  };

  // 测试1: 内核模块管理
  console.log('📋 测试1: 内核模块管理');
  try {
    const kernel = new Kernel();
    await kernel.initialize();
    
    // 测试模块加载
    const fsModule = new FileSystemModule();
    await kernel.loadModule('FileSystemModule', fsModule);
    
    // 测试模块获取
    const loadedModule = kernel.getModule('FileSystemModule');
    console.log('✅ 模块加载成功:', loadedModule.name);
    
    // 测试模块列表
    const modules = kernel.listModules();
    console.log('✅ 已加载模块:', modules);
    
  } catch (error) {
    recordFailure('内核模块管理测试失败', error);
  }

  // 测试2: 文件系统模块功能
  console.log('\n📁 测试2: 文件系统模块功能');
  try {
    const fsModule = new FileSystemModule();
    
    // 测试初始化
    await fsModule.initialize();
    console.log('✅ 模块初始化成功');
    
    // 测试文件操作
    await fsModule.createFile('/test/file1.txt', 'Hello World');
    const content = await fsModule.readFile('/test/file1.txt');
    console.log('✅ 文件读写测试:', content);
    
    // 测试文件列表
    const files = await fsModule.listFiles('/');
    console.log('✅ 文件列表测试:', files.length, '个文件');
    
    // 测试状态获取
    const status = fsModule.getStatus();
    console.log('✅ 状态获取测试:', status.state);
    
  } catch (error) {
    recordFailure('文件系统模块测试失败', error);
  }

  // 测试3: 网络模块功能
  console.log('\n🌐 测试3: 网络模块功能');
  try {
    const netModule = new NetworkModule();
    
    // 测试初始化
    await netModule.initialize();
    console.log('✅ 模块初始化成功');
    
    // 测试监听
    const listenerId = await netModule.listen(8080, 'localhost', () => {});
    console.log('✅ 端口监听测试:', listenerId);
    
    // 测试连接
    const connectionId = await netModule.connect('example.com', 80);
    console.log('✅ 连接建立测试:', connectionId);
    
    // 测试路由
    await netModule.addRoute('/api/test', () => ({ success: true }));
    console.log('✅ 路由添加测试:', netModule.routes.size, '个路由');
    
    // 测试状态获取
    const status = netModule.getStatus();
    console.log('✅ 状态获取测试:', status.state);
    
  } catch (error) {
    recordFailure('网络模块测试失败', error);
  }

  // 测试4: 接口契约验证
  console.log('\n📄 测试4: 接口契约验证');
  try {
    // 验证文件系统模块接口
    let interfaceValid = true;
    for (const [methodName, expectedType] of Object.entries(filesystemInterface)) {
      if (typeof FileSystemModule.prototype[methodName] !== expectedType) {
        interfaceValid = false;
        break;
      }
    }
    if (!interfaceValid) {
      throw new Error('文件系统模块接口验证失败');
    }
    console.log('✅ 文件系统模块接口验证: 通过');
    
    // 验证网络模块接口
    interfaceValid = true;
    for (const [methodName, expectedType] of Object.entries(networkInterface)) {
      if (typeof NetworkModule.prototype[methodName] !== expectedType) {
        interfaceValid = false;
        break;
      }
    }
    if (!interfaceValid) {
      throw new Error('网络模块接口验证失败');
    }
    console.log('✅ 网络模块接口验证: 通过');
    
  } catch (error) {
    recordFailure('接口契约验证测试失败', error);
  }

  // 测试5: 完整系统集成测试
  console.log('\n🔗 测试5: 完整系统集成测试');
  try {
    // 创建系统组件
    const kernel = new Kernel();
    const sandbox = new Sandbox(kernel);
    const featureLayer = new FeatureLayer(kernel, sandbox);
    
    // 初始化
    await kernel.initialize();
    
    // 定义接口
    featureLayer.defineModuleInterface('FileSystemModule', filesystemInterface);
    featureLayer.defineModuleInterface('NetworkModule', networkInterface);
    
    // 创建模块
    const fsModuleId = featureLayer.createFeatureModule('FileSystemModule', {});
    const netModuleId = featureLayer.createFeatureModule('NetworkModule', {});
    
    // 加载模块
    await featureLayer.loadFeatureModule(fsModuleId);
    await featureLayer.loadFeatureModule(netModuleId);
    
    console.log('✅ 模块创建和加载成功');
    console.log('✅ 已加载模块:', kernel.listModules().length);
    
    // 运行测试
    const fsTestCases = [
      {
        name: '创建文件测试',
        test: async (kernel, config) => {
          const fsModule = kernel.getModule('FileSystemModule');
          await fsModule.initialize();
          const file = await fsModule.createFile('/test/hello.txt', 'Hello, World!');
          return file.content === 'Hello, World!';
        }
      }
    ];
    
    const testEnvId = sandbox.createTestEnvironment('FileSystemModule');
    const results = await sandbox.runModuleTest(testEnvId, fsTestCases);
    if (results.failed !== 0) {
      throw new Error(`Sandbox 测试失败: ${results.failed}/${results.total}`);
    }
    
    console.log('✅ 模块测试执行成功');
    console.log('✅ 测试结果:', results);
    
  } catch (error) {
    recordFailure('完整系统集成测试失败', error);
    console.error('错误堆栈:', error.stack);
  }

  if (process.env.KENUX_TEST_INJECT_FAILURE === '1') {
    recordFailure('失败传播测试', new Error('注入的测试失败'));
  }

  if (failures.length > 0) {
    console.error(`\n测试失败: ${failures.length} 项`);
    process.exitCode = 1;
    return false;
  }
  console.log('\n🎉 所有测试完成！');
  return true;
}

// 运行测试
runTests().catch((error) => {
  console.error('❌ 测试运行器异常:', error);
  process.exitCode = 1;
});
