const { NetworkModule, networkInterface } = require('../examples/network-module');

describe('NetworkModule - 单点测试', () => {
  let networkModule;
  let testEnvId;

  beforeEach(() => {
    networkModule = new NetworkModule({
      maxConnections: 10,
      bufferSize: 4096,
      timeout: 5000
    });
    testEnvId = `network_test_${Date.now()}`;
  });

  describe('模块生命周期', () => {
    test('应该能够初始化模块', async () => {
      await expect(networkModule.initialize()).resolves.not.toThrow();
      expect(networkModule.initialized).toBe(true);
    });

    test('应该能够启动和停止模块', async () => {
      await networkModule.initialize();
      await expect(networkModule.start()).resolves.not.toThrow();
      await expect(networkModule.stop()).resolves.not.toThrow();
    });
  });

  describe('监听和连接', () => {
    beforeEach(async () => {
      await networkModule.initialize();
      await networkModule.start();
    });

    test('应该能够监听端口', async () => {
      const port = 8080;
      const host = 'localhost';
      const listenerId = await networkModule.listen(port, host, () => {});
      
      expect(listenerId).toBe(`${host}:${port}`);
      expect(networkModule.listeners.has(listenerId)).toBe(true);
    });

    test('应该能够停止监听', async () => {
      const port = 8080;
      const host = 'localhost';
      const listenerId = await networkModule.listen(port, host, () => {});
      
      await networkModule.stopListening(listenerId);
      expect(networkModule.listeners.has(listenerId)).toBe(false);
    });

    test('应该能够建立连接', async () => {
      const connectionId = await networkModule.connect('example.com', 80);
      
      expect(connectionId).toBeDefined();
      expect(networkModule.connections.has(connectionId)).toBe(true);
      expect(networkModule.connections.get(connectionId).state).toBe('connecting');
    });

    test('连接建立后状态应该变为connected', async (done) => {
      const connectionId = await networkModule.connect('example.com', 80);
      
      setTimeout(() => {
        const connection = networkModule.connections.get(connectionId);
        expect(connection.state).toBe('connected');
        done();
      }, 150);
    });

    test('应该能够关闭连接', async () => {
      const connectionId = await networkModule.connect('example.com', 80);
      
      await networkModule.closeConnection(connectionId);
      expect(networkModule.connections.has(connectionId)).toBe(false);
    });

    test('关闭不存在的连接应该抛出错误', async () => {
      await expect(networkModule.closeConnection('nonexistent')).rejects.toThrow('连接不存在');
    });
  });

  describe('数据传输', () => {
    beforeEach(async () => {
      await networkModule.initialize();
      await networkModule.start();
    });

    test('应该能够发送数据', async () => {
      const connectionId = await networkModule.connect('example.com', 80);
      
      // 等待连接建立
      await new Promise(resolve => setTimeout(resolve, 150));
      
      const testData = Buffer.from('Hello, Network!');
      await expect(networkModule.send(connectionId, testData)).resolves.not.toThrow();
    });

    test('发送数据到未建立的连接应该抛出错误', async () => {
      const connectionId = await networkModule.connect('example.com', 80);
      
      await expect(networkModule.send(connectionId, Buffer.from('test'))).rejects.toThrow('连接未建立');
    });
  });

  describe('路由管理', () => {
    beforeEach(async () => {
      await networkModule.initialize();
      await networkModule.start();
    });

    test('应该能够添加路由', async () => {
      const path = '/api/test';
      const handler = jest.fn();
      
      await networkModule.addRoute(path, handler);
      expect(networkModule.routes.has(path)).toBe(true);
      expect(networkModule.routes.get(path)).toBe(handler);
    });

    test('应该能够移除路由', async () => {
      const path = '/api/test';
      const handler = jest.fn();
      
      await networkModule.addRoute(path, handler);
      await networkModule.removeRoute(path);
      expect(networkModule.routes.has(path)).toBe(false);
    });

    test('应该能够处理请求', async () => {
      const path = '/api/test';
      const handler = jest.fn().mockResolvedValue({ success: true });
      const request = { method: 'GET', headers: {} };
      
      await networkModule.addRoute(path, handler);
      const response = await networkModule.handleRequest(path, request);
      
      expect(response).toEqual({ success: true });
      expect(handler).toHaveBeenCalledWith(request);
    });

    test('处理不存在路由的请求应该抛出错误', async () => {
      const path = '/nonexistent';
      const request = { method: 'GET', headers: {} };
      
      await expect(networkModule.handleRequest(path, request)).rejects.toThrow('路由不存在');
    });
  });

  describe('配置和限制', () => {
    test('应该应用最大连接数限制', async () => {
      const smallModule = new NetworkModule({ maxConnections: 2 });
      await smallModule.initialize();
      await smallModule.start();
      
      await smallModule.connect('example.com', 80);
      await smallModule.connect('example.com', 81);
      
      await expect(smallModule.connect('example.com', 82)).rejects.toThrow('已达到最大连接数限制');
    });

    test('应该正确报告模块状态', async () => {
      await networkModule.initialize();
      await networkModule.start();
      
      const status = networkModule.getStatus();
      expect(status.name).toBe('NetworkModule');
      expect(status.state).toBe('running');
      expect(status.connectionsCount).toBe(0);
      expect(status.listenersCount).toBe(0);
      expect(status.routesCount).toBe(0);
    });
  });

  describe('接口契约验证', () => {
    test('应该实现所有必需的接口方法', () => {
      for (const [methodName, expectedType] of Object.entries(networkInterface)) {
        expect(typeof networkModule[methodName]).toBe(expectedType);
      }
    });

    test('应该正确响应模块事件', (done) => {
      networkModule.on('initialized', () => {
        done();
      });
      
      networkModule.initialize();
    });
  });
});