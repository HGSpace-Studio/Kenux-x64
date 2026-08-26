# 控件展示示例

当前展示示例路径：

```text
examples/showcase
```

这个示例包含：

- Button
- Checkbox
- RadioButton
- TextBox
- 主题切换
- 示例代码显示

## 构建与运行

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
