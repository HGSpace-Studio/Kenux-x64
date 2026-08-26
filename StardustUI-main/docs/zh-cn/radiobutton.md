# RadioButton 控件

`RadioButton` 用于同组单选。

## 头文件

```cpp
#include "../../includes/components/radiobutton.hpp"
```

## 常用接口

```cpp
void set_group(const stardustui::string& group);
void set_selected(bool selected);
bool is_selected() const;
void set_change_callback(void (*func)(RadioButton&, bool));
```
