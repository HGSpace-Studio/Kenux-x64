const { FileSystemModule, filesystemInterface } = require('../examples/filesystem-module');

describe('FileSystemModule - 单点测试', () => {
  let fileSystemModule;
  let testEnvId;

  beforeEach(() => {
    fileSystemModule = new FileSystemModule({
      maxFiles: 10,
      maxSize: '100KB'
    });
    testEnvId = `filesystem_test_${Date.now()}`;
  });

  describe('模块生命周期', () => {
    test('应该能够初始化模块', async () => {
      await expect(fileSystemModule.initialize()).resolves.not.toThrow();
      expect(fileSystemModule.initialized).toBe(true);
    });

    test('应该能够启动和停止模块', async () => {
      await fileSystemModule.initialize();
      await expect(fileSystemModule.start()).resolves.not.toThrow();
      await expect(fileSystemModule.stop()).resolves.not.toThrow();
    });
  });

  describe('文件操作', () => {
    beforeEach(async () => {
      await fileSystemModule.initialize();
      await fileSystemModule.start();
    });

    test('应该能够创建文件', async () => {
      const filePath = '/test/file.txt';
      const content = 'Hello, World!';
      
      const file = await fileSystemModule.createFile(filePath, content);
      expect(file.path).toBe(filePath);
      expect(file.content).toBe(content);
      expect(file.size).toBe(content.length);
    });

    test('应该能够读取文件', async () => {
      const filePath = '/test/file.txt';
      const content = 'Hello, World!';
      
      await fileSystemModule.createFile(filePath, content);
      const readContent = await fileSystemModule.readFile(filePath);
      expect(readContent).toBe(content);
    });

    test('应该能够写入文件', async () => {
      const filePath = '/test/file.txt';
      const originalContent = 'Hello, World!';
      const newContent = 'Updated content';
      
      await fileSystemModule.createFile(filePath, originalContent);
      await fileSystemModule.writeFile(filePath, newContent);
      
      const file = fileSystemModule.files.get(filePath);
      expect(file.content).toBe(newContent);
      expect(file.size).toBe(newContent.length);
      expect(file.modifiedAt.getTime()).toBeGreaterThan(file.createdAt.getTime());
    });

    test('应该能够删除文件', async () => {
      const filePath = '/test/file.txt';
      const content = 'Hello, World!';
      
      await fileSystemModule.createFile(filePath, content);
      expect(fileSystemModule.files.has(filePath)).toBe(true);
      
      await fileSystemModule.deleteFile(filePath);
      expect(fileSystemModule.files.has(filePath)).toBe(false);
    });

    test('应该能够列出文件', async () => {
      await fileSystemModule.createFile('/test/file1.txt', 'Content 1');
      await fileSystemModule.createFile('/test/file2.txt', 'Content 2');
      await fileSystemModule.createFile('/other/file.txt', 'Content 3');
      
      const testFiles = await fileSystemModule.listFiles('/test');
      expect(testFiles.length).toBe(2);
      
      const otherFiles = await fileSystemModule.listFiles('/other');
      expect(otherFiles.length).toBe(1);
    });

    test('读取不存在的文件应该抛出错误', async () => {
      await expect(fileSystemModule.readFile('/nonexistent/file.txt')).rejects.toThrow('文件不存在');
    });
  });

  describe('配置和限制', () => {
    test('应该应用最大文件数限制', async () => {
      const smallModule = new FileSystemModule({ maxFiles: 2 });
      await smallModule.initialize();
      
      await smallModule.createFile('/file1.txt', 'Content 1');
      await smallModule.createFile('/file2.txt', 'Content 2');
      
      await expect(smallModule.createFile('/file3.txt', 'Content 3')).rejects.toThrow('已达到最大文件数量限制');
    });

    test('应该正确报告模块状态', async () => {
      await fileSystemModule.initialize();
      await fileSystemModule.start();
      
      const status = fileSystemModule.getStatus();
      expect(status.name).toBe('FileSystemModule');
      expect(status.state).toBe('running');
      expect(status.filesCount).toBe(0);
    });
  });

  describe('接口契约验证', () => {
    test('应该实现所有必需的接口方法', () => {
      for (const [methodName, expectedType] of Object.entries(filesystemInterface)) {
        expect(typeof fileSystemModule[methodName]).toBe(expectedType);
      }
    });

    test('应该正确响应模块事件', (done) => {
      fileSystemModule.on('initialized', () => {
        done();
      });
      
      fileSystemModule.initialize();
    });
  });
});