# Checkbox Component

`Checkbox` is a labeled boolean toggle component.

## Header

```cpp
#include "../../includes/components/checkbox.hpp"
```

## Common API

```cpp
void set_checked(bool checked);
bool is_checked() const;
void set_text(const stardustui::string& text);
void set_toggle_callback(void (*func)(Checkbox&, bool));
```
