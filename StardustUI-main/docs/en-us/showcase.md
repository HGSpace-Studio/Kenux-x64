# Widget Showcase

Current showcase example path:

```text
examples/showcase
```

This example includes:

- Button
- Checkbox
- RadioButton
- TextBox
- theme switching
- generated example code preview

## Build and run

Linux:

```bash
cd examples/showcase
ninja showcase-linux
./build/linux/showcase
```

Windows:

```bash
cd examples/showcase
CXX=x86_64-w64-mingw32-g++ ninja showcase-windows
```

XJ380:

```bash
cd examples/showcase
ninja copy-xj380
```
