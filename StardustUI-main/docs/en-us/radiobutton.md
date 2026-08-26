# RadioButton Component

`RadioButton` is used for single selection inside a group.

## Header

```cpp
#include "../../includes/components/radiobutton.hpp"
```

## Common API

```cpp
void set_group(const stardustui::string& group);
void set_selected(bool selected);
bool is_selected() const;
void set_change_callback(void (*func)(RadioButton&, bool));
```
