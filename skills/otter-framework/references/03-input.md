# 03 — 输入（鼠标 / 键盘）

## 鼠标事件

事件挂在图层上。需要先用 `hit_area` 或 `auto_hit_area` 给图层一个命中区域，
否则点击不会命中（除非该层有 bounds 且 auto_hit_area）。

```cpp
Otter::Layer* btn = root->creat("btn");
btn->hit_area(50, 50, 120, 40);     // 命中矩形（坐标系同父层）
// 或：btn->auto_hit_area(true);    // 命中区 = 自身 resolved bounds

btn->on_click([st](const Otter::MouseEvent& e) -> bool {
    // 按下并在原处松开（构成点击）时触发
    return true;   // 返回 true = 消费事件，停止向其它层传播
});
btn->on_mouse_enter([st](const Otter::MouseEvent&){ st->hover=true;  return false; });
btn->on_mouse_leave([st](const Otter::MouseEvent&){ st->hover=false; return false; });
btn->on_mouse_down([](const Otter::MouseEvent&){ return true; });
btn->on_mouse_up  ([](const Otter::MouseEvent&){ return true; });
btn->on_mouse_move([](const Otter::MouseEvent& e){ /* e.delta_x/delta_y */ return false; });
btn->on_wheel     ([](const Otter::MouseEvent& e){ /* e.wheel_delta */ return false; });
```

### MouseEvent 字段
```
float x, y;                      // 当前位置（客户区像素）
float delta_x, delta_y;          // 移动增量（仅 on_mouse_move）
float wheel_delta;               // 滚轮量（正=上滚）
bool  left_down, right_down, middle_down;
bool  ctrl_down, shift_down, alt_down;
```

### 事件传播
子层优先（视觉上层先收到），任一回调返回 `true` 即停止传播（类似 HTML 冒泡）。
hover 高亮类回调通常返回 `false`（不消费，让其它层也能响应）。

## 键盘输入

键盘是**窗口级**的：用 `set_keyboard_target` 注册当前焦点的处理回调。
典型流程：点击某输入框 → 调 `set_keyboard_target` 把键盘指向它。

跨平台重载（推荐，用 `Otter::Key` 枚举）：

```cpp
win.set_keyboard_target(
    // 1) 字符回调：可见字符输入（UTF-16 wchar_t）
    [st](wchar_t ch) -> bool {
        if (ch >= 32 && ch != 127) { st->text.push_back(ch); return true; }
        return false;
    },
    // 2) 按键回调：功能键
    [st, &win](Otter::Key key) -> bool {
        if (key == Otter::Key::Backspace) { if(!st->text.empty()) st->text.pop_back(); return true; }
        if (key == Otter::Key::Enter)     { submit(st->text); return true; }
        if (key == Otter::Key::Escape)    { win.close(); return true; }
        return false;
    },
    // 3) 失焦回调（可选）：切走键盘焦点时调用
    [st]() { st->focused = false; });

win.clear_keyboard_target();   // 取消键盘焦点
```

### Key 枚举（OtterInput.h）
`Unknown Backspace Tab Enter Escape Space Left Up Right Down`
`MouseLeft MouseRight MouseMiddle`，可见字符用其 ASCII 码值（大写）。
辅助：`key_from_ascii(char)` / `key_code(Key)`。

> 字符回调 vs 按键回调：字符回调拿到的是「输入法/Shift 处理后的真实字符」
> （适合文本录入）；按键回调拿到的是「物理键语义」（适合快捷键/方向键）。
