# Checkbox 控件

`Checkbox` 是带标签的二值切换控件。

## 头文件

```cpp
#include "../../includes/components/checkbox.hpp"
```

## 常用接口

```cpp
void set_checked(bool checked);
bool is_checked() const;
void set_text(const stardustui::string& text);
void set_toggle_callback(void (*func)(Checkbox&, bool));
```
