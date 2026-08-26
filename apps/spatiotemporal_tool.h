#ifndef SPATIOTEMPORAL_TOOL_H
#define SPATIOTEMPORAL_TOOL_H

// Spaitotemporal Composability 工具头文件
// 集成到现有操作系统中的应用程序

// 声明主函数
void spatiotemporal_tool_main();

// 编译指示符，用于控制是否编译Spaitotemporal功能
// 在编译时可以通过定义 SPATIOTEMPORAL_API_AVAILABLE 来启用此功能
#ifndef SPATIOTEMPORAL_API_AVAILABLE
#define SPATIOTEMPORAL_API_AVAILABLE 0
#endif

#endif // SPATIOTEMPORAL_TOOL_H