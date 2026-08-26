const EventEmitter = require('events');

/**
 * Sandbox - 单点测试器
 * 为每个模块提供独立的测试环境，支持Mock框架和自动化验证
 */
class Sandbox extends EventEmitter {
  constructor(kernel) {
    super();
    this.kernel = kernel;
    this.activeTests = new Map();
    this.mockProviders = new Map();
    this.testResults = new Map();
    this.contractValidators = new Map();
  }

  /**
   * 创建模块测试环境
   */
  createTestEnvironment(moduleName, moduleConfig = {}) {
    const testEnvId = `${moduleName}_${Date.now()}`;
    const testEnvironment = {
      id: testEnvId,
      moduleName,
      config: moduleConfig,
      state: 'created',
      startTime: Date.now(),
      mocks: new Map(),
      testCases: []
    };

    this.activeTests.set(testEnvId, testEnvironment);
    console.log(`🧪 Sandbox - 测试环境已创建: ${testEnvId}`);
    
    this.emit('test:environment:created', testEnvironment);
    return testEnvId;
  }

  /**
   * 设置Mock提供者
   */
  setMockProvider(serviceName, mockProvider) {
    this.mockProviders.set(serviceName, mockProvider);
    console.log(`🎭 Sandbox - Mock提供者已设置: ${serviceName}`);
  }

  /**
   * 注册契约验证器
   */
  registerContractValidator(interfaceName, validator) {
    this.contractValidators.set(interfaceName, validator);
    console.log(`📄 Sandbox - 契约验证器已注册: ${interfaceName}`);
  }

  /**
   * 运行模块测试
   */
  async runModuleTest(testEnvId, testCases) {
    const testEnvironment = this.activeTests.get(testEnvId);
    if (!testEnvironment) {
      throw new Error(`测试环境不存在: ${testEnvId}`);
    }

    testEnvironment.state = 'running';
    testEnvironment.testCases = testCases;
    testEnvironment.startTime = Date.now();

    console.log(`🧪 Sandbox - 开始运行测试: ${testEnvId}`);
    
    const results = [];
    
    for (const testCase of testCases) {
      try {
        const result = await this.executeTestCase(testEnvId, testCase);
        results.push(result);
      } catch (error) {
        results.push({
          testCase: testCase.name,
          status: 'failed',
          error: error.message,
          timestamp: Date.now()
        });
      }
    }

    testEnvironment.state = 'completed';
    testEnvironment.endTime = Date.now();
    testEnvironment.results = results;

    const summary = this.generateTestSummary(results);
    this.testResults.set(testEnvId, {
      ...testEnvironment,
      summary
    });

    console.log(`🧪 Sandbox - 测试完成: ${testEnvId}`);
    this.emit('test:completed', testEnvironment);
    
    return summary;
  }

  /**
   * 执行单个测试用例
   */
  async executeTestCase(testEnvId, testCase) {
    const testEnvironment = this.activeTests.get(testEnvId);
    
    // 应用Mock
    const appliedMocks = this.applyMocks(testEnvironment, testCase.mocks || {});
    
    try {
      // 执行测试逻辑
      const result = await testCase.test(this.kernel, testEnvironment.config);
      
      // 验证契约
      if (testCase.contract) {
        this.validateContract(testCase.contract, result);
      }
      
      return {
        testCase: testCase.name,
        status: 'passed',
        result,
        timestamp: Date.now()
      };
    } catch (error) {
      throw error;
    } finally {
      // 清理Mock
      this.cleanupMocks(appliedMocks);
    }
  }

  /**
   * 应用Mock
   */
  applyMocks(testEnvironment, mocks) {
    const appliedMocks = [];
    
    for (const [serviceName, mockConfig] of Object.entries(mocks)) {
      const mockProvider = this.mockProviders.get(serviceName);
      if (mockProvider) {
        const mock = mockProvider.createMock(mockConfig);
        testEnvironment.mocks.set(serviceName, mock);
        appliedMocks.push({ serviceName, mock });
      }
    }
    
    return appliedMocks;
  }

  /**
   * 清理Mock
   */
  cleanupMocks(appliedMocks) {
    for (const { serviceName } of appliedMocks) {
      this.mockProviders.delete(serviceName);
    }
  }

  /**
   * 验证契约
   */
  validateContract(contract, result) {
    const validator = this.contractValidators.get(contract.interface);
    if (validator) {
      validator.validate(result, contract.specification);
    } else {
      throw new Error(`未找到契约验证器: ${contract.interface}`);
    }
  }

  /**
   * 生成测试摘要
   */
  generateTestSummary(results) {
    const passed = results.filter(r => r.status === 'passed').length;
    const failed = results.filter(r => r.status === 'failed').length;
    const total = results.length;
    
    return {
      total,
      passed,
      failed,
      successRate: total > 0 ? (passed / total * 100).toFixed(2) + '%' : '0%',
      duration: results.length > 0 ? 
        Math.max(...results.map(r => r.timestamp)) - 
        Math.min(...results.map(r => r.timestamp)) : 0
    };
  }

  /**
   * 获取测试结果
   */
  getTestResult(testEnvId) {
    return this.testResults.get(testEnvId);
  }

  /**
   * 销毁测试环境
   */
  destroyTestEnvironment(testEnvId) {
    if (this.activeTests.has(testEnvId)) {
      this.activeTests.delete(testEnvId);
      console.log(`🧪 Sandbox - 测试环境已销毁: ${testEnvId}`);
      this.emit('test:environment:destroyed', { testEnvId });
    }
  }

  /**
   * 获取所有活跃测试环境
   */
  getActiveTestEnvironments() {
    return Array.from(this.activeTests.values());
  }
}

module.exports = Sandbox;