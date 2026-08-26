/**
 * 网络模块示例
 * 展示如何创建一个独立的内核模块
 */

const { EventEmitter } = require('events');

class NetworkModule extends EventEmitter {
  constructor(config = {}) {
    super();
    this.config = {
      maxConnections: config.maxConnections || 1000,
      bufferSize: config.bufferSize || 8192,
      timeout: config.timeout || 30000,
      ...config
    };
    this.connections = new Map();
    this.listeners = new Map();
    this.routes = new Map();
    this.initialized = false;
  }

  // 模块生命周期方法
  async initialize() {
    console.log('🌐 网络模块初始化...');
    this.initialized = true;
    this.emit('initialized');
  }

  async start() {
    console.log('🌐 网络模块启动...');
    this.emit('started');
  }

  async stop() {
    console.log('🌐 网络模块停止...');
    // 关闭所有连接
    for (const [connectionId, connection] of this.connections) {
      await this.closeConnection(connectionId);
    }
    this.emit('stopped');
  }

  // 网络操作接口
  async listen(port, host = '0.0.0.0', callback) {
    const listenerId = `${host}:${port}`;
    this.listeners.set(listenerId, {
      port,
      host,
      callback,
      connections: 0,
      startedAt: new Date()
    });
    console.log(`🌐 监听端口: ${listenerId}`);
    this.emit('listener:created', { listenerId, port, host });
    return listenerId;
  }

  async stopListening(listenerId) {
    if (this.listeners.has(listenerId)) {
      const listener = this.listeners.get(listenerId);
      this.listeners.delete(listenerId);
      console.log(`🌐 停止监听: ${listenerId}`);
      this.emit('listener:removed', { listenerId, listener });
    }
  }

  async connect(host, port, options = {}) {
    const connectionId = `conn_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    
    if (this.connections.size >= this.config.maxConnections) {
      throw new Error('已达到最大连接数限制');
    }

    const connection = {
      id: connectionId,
      host,
      port,
      state: 'connecting',
      createdAt: new Date(),
      lastActivity: new Date(),
      buffer: Buffer.alloc(this.config.bufferSize),
      ...options
    };

    this.connections.set(connectionId, connection);
    console.log(`🌐 建立连接: ${connectionId} -> ${host}:${port}`);
    
    // 模拟连接过程
    setTimeout(() => {
      connection.state = 'connected';
      this.emit('connection:established', connection);
    }, 100);

    return connectionId;
  }

  async send(connectionId, data) {
    const connection = this.connections.get(connectionId);
    if (!connection) {
      throw new Error(`连接不存在: ${connectionId}`);
    }

    if (connection.state !== 'connected') {
      throw new Error(`连接未建立: ${connectionId}`);
    }

    connection.lastActivity = new Date();
    console.log(`🌐 发送数据: ${connectionId} (${data.length} bytes)`);
    this.emit('data:sent', { connectionId, data, size: data.length });
  }

  async closeConnection(connectionId) {
    const connection = this.connections.get(connectionId);
    if (connection) {
      connection.state = 'closed';
      this.connections.delete(connectionId);
      console.log(`🌐 关闭连接: ${connectionId}`);
      this.emit('connection:closed', { connectionId, connection });
    }
  }

  // 路由管理
  async addRoute(path, handler) {
    this.routes.set(path, handler);
    console.log(`🌐 添加路由: ${path}`);
    this.emit('route:added', { path, handler });
  }

  async removeRoute(path) {
    if (this.routes.has(path)) {
      const handler = this.routes.get(path);
      this.routes.delete(path);
      console.log(`🌐 移除路由: ${path}`);
      this.emit('route:removed', { path, handler });
    }
  }

  async handleRequest(path, request) {
    const handler = this.routes.get(path);
    if (handler) {
      return await handler(request);
    } else {
      throw new Error(`路由不存在: ${path}`);
    }
  }

  // 获取模块状态
  getStatus() {
    return {
      name: 'NetworkModule',
      state: this.initialized ? 'running' : 'stopped',
      config: this.config,
      connectionsCount: this.connections.size,
      listenersCount: this.listeners.size,
      routesCount: this.routes.size,
      uptime: this.initialized ? Date.now() - this.startTime : 0
    };
  }
}

// 模块接口定义
const networkInterface = {
  initialize: 'function',
  start: 'function',
  stop: 'function',
  listen: 'function',
  stopListening: 'function',
  connect: 'function',
  send: 'function',
  closeConnection: 'function',
  addRoute: 'function',
  removeRoute: 'function',
  handleRequest: 'function',
  getStatus: 'function'
};

module.exports = {
  NetworkModule,
  networkInterface
};