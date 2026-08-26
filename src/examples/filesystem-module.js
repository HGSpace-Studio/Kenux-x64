/**
 * 文件系统模块示例
 * 展示如何创建一个独立的内核模块
 */

const { EventEmitter } = require('events');

class FileSystemModule extends EventEmitter {
  constructor(config = {}) {
    super();
    this.config = {
      maxFiles: config.maxFiles || 1000,
      maxSize: config.maxSize || '1GB',
      ...config
    };
    this.files = new Map();
    this.mountPoints = new Map();
    this.initialized = false;
  }

  // 模块生命周期方法
  async initialize() {
    console.log('📁 文件系统模块初始化...');
    this.initialized = true;
    this.emit('initialized');
  }

  async start() {
    console.log('📁 文件系统模块启动...');
    this.emit('started');
  }

  async stop() {
    console.log('📁 文件系统模块停止...');
    this.emit('stopped');
  }

  // 文件系统操作接口
  async mount(path, device) {
    console.log(`📁 挂载设备到路径: ${path}`);
    this.mountPoints.set(path, device);
    this.emit('mounted', { path, device });
  }

  async unmount(path) {
    if (this.mountPoints.has(path)) {
      const device = this.mountPoints.get(path);
      this.mountCases.delete(path);
      console.log(`📁 卸载设备: ${path}`);
      this.emit('unmounted', { path, device });
    }
  }

  async createFile(path, content = '') {
    if (this.files.size >= this.config.maxFiles) {
      throw new Error('已达到最大文件数量限制');
    }

    const file = {
      path,
      content,
      size: Buffer.byteLength(content),
      createdAt: new Date(),
      modifiedAt: new Date()
    };

    this.files.set(path, file);
    console.log(`📁 创建文件: ${path}`);
    this.emit('file:created', file);
    return file;
  }

  async readFile(path) {
    const file = this.files.get(path);
    if (!file) {
      throw new Error(`文件不存在: ${path}`);
    }
    return file.content;
  }

  async writeFile(path, content) {
    const file = this.files.get(path);
    if (file) {
      file.content = content;
      file.size = Buffer.byteLength(content);
      file.modifiedAt = new Date();
      console.log(`📁 写入文件: ${path}`);
      this.emit('file:modified', file);
    } else {
      await this.createFile(path, content);
    }
  }

  async deleteFile(path) {
    const file = this.files.get(path);
    if (file) {
      this.files.delete(path);
      console.log(`📁 删除文件: ${path}`);
      this.emit('file:deleted', { path, file });
    }
  }

  async listFiles(directory = '/') {
    const files = [];
    for (const [path, file] of this.files) {
      if (path.startsWith(directory)) {
        files.push(file);
      }
    }
    return files;
  }

  // 获取模块状态
  getStatus() {
    return {
      name: 'FileSystemModule',
      state: this.initialized ? 'running' : 'stopped',
      config: this.config,
      filesCount: this.files.size,
      mountPoints: Array.from(this.mountPoints.keys()),
      uptime: this.initialized ? Date.now() - this.startTime : 0
    };
  }
}

// 模块接口定义
const filesystemInterface = {
  initialize: 'function',
  start: 'function',
  stop: 'function',
  mount: 'function',
  unmount: 'function',
  createFile: 'function',
  readFile: 'function',
  writeFile: 'function',
  deleteFile: 'function',
  listFiles: 'function',
  getStatus: 'function'
};

module.exports = {
  FileSystemModule,
  filesystemInterface
};