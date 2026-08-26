# 安装与构建

## 克隆仓库

```bash
git clone https://github.com/xingji-studio/StardustUI.git
cd StardustUI
```

## 当前平台后端

StardustUI 目前支持：

- `xj380`
- `linux`
- `windows`

当前仓库使用 `ninja` 构建框架和示例。

## Linux

当前 Linux 后端使用 SDL2 和 SDL_ttf。

在 Arch Linux 上：

```bash
sudo pacman -S sdl2 sdl2_ttf
ninja -C . lib-linux
```

在 Debian/Ubuntu 上：
``` bash
sudo apt update
sudo apt install libsdl2-dev libsdl2-ttf-dev

ninja -C . lib-linux
``` 

在 RHEL 上：
``` bash
#新版本RHEL/Rocky
sudo dnf install SDL2-devel SDL2_ttf-devel
#老版本RHEL/CentOS
sudo yum install SDL2-devel SDL2_ttf-devel

ninja -C . lib-linux
``` 

产物位置：

```text
build/libStardustUI-linux.a
```

## Windows

可以使用 MinGW 交叉编译：

```bash
CXX=x86_64-w64-mingw32-g++ ninja -C . lib-windows
```

## XJ380

构建方式：

```bash
ninja -C . lib-xj380
```

这个目标依赖仓库里现有的 XJ380 工具链和相关对象文件。

## 平台自动识别

如果不手动指定平台宏，`settings.hpp` 当前规则是：

- `_WIN32` -> `STARDUSTUI_WINDOWS`
- `__linux__` -> `STARDUSTUI_LINUX`
- 其他情况 -> `XJ380`

## 构建示例

当前示例包括：

- `examples/helloworld`
- `examples/showcase`
- `examples/duckchat`

### `helloworld`

Linux：

```bash
cd examples/helloworld
ninja helloworld-linux
./build/linux/helloworld
```

Windows：

```bash
cd examples/helloworld
CXX=x86_64-w64-mingw32-g++ ninja helloworld-windows
```

XJ380：

```bash
cd examples/helloworld
ninja copy-xj380
```

### `showcase`

Linux：

```bash
cd examples/showcase
ninja showcase-linux
./build/linux/showcase
```

Windows：

```bash
cd examples/showcase
CXX=x86_64-w64-mingw32-g++ ninja showcase-windows
```

XJ380：

```bash
cd examples/showcase
ninja copy-xj380
```

### `duckchat`

Linux：

```bash
cd examples/duckchat
ninja duckchat-linux
./build/linux/duckchat
```

Windows：

```bash
cd examples/duckchat
CXX=x86_64-w64-mingw32-g++ ninja duckchat-windows
```

XJ380：

```bash
cd examples/duckchat
ninja copy-xj380
```

这个示例是一个完整聊天界面，包含：

- 首次启动配置页
- `Save And Connect` 按钮
- 聊天页 `Reconnect` 按钮
- 消息输入框和 `Send` 按钮
- JSON 配置保存
- 命令行主题名参数

如果需要自定义主题，推荐先使用在线 Material 3 主题生成器生成配色：

- <https://archzero.top/MD3color/>

## 相关文档

- [快速开始](./quickstart.md)
- [创建窗口](./create_window.md)
- [样式系统](./style.md)
- [Button 控件](./button.md)
- [Checkbox 控件](./checkbox.md)
- [RadioButton 控件](./radiobutton.md)
- [TextBox 控件](./textbox.md)
- [ScrollBar 控件](./scrollbar.md)
- [布局系统](./layout.md)
- [Canvas 控件](./canvas.md)
- [控件展示示例](./showcase.md)
- [DuckChat 教程](./duckchat_tutorial.md)
