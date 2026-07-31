# 09 — 内置控件 API 全解

Otter 在 `OtterWidget.h` 中提供 8 个生产级控件。全部基于 `Layer + PaintChain` 实现，
**不使用 Win32 子窗口**（浏览器控件除外）。

## 目录
1. TextField — 单行输入框
2. TextArea — 多行编辑器
3. ImageView — 图片显示
4. Checkbox / CheckboxEx — 复选框
5. RadioButtonEx / RadioGroup — 单选按钮
6. Dropdown — 下拉选择框
7. TitleText — 标题文字标签
8. ClipboardGuard — 剪贴板 RAII 工具

---

## 1. TextField — 单行输入框

```cpp
Otter::TextField tf(&win, parent, "name", x, y, w, h);
```

### 核心 API
| 方法 | 说明 |
|------|------|
| `set_text(L"...")` | 设置文本（替换全部内容） |
| `text() -> std::wstring` | 读取当前文本 |
| `placeholder(L"...")` | 占位文字（空内容时显示） |
| `readonly(bool)` | 只读模式 |
| `max_length(int)` | 最大字符数（0=不限） |
| `password_mode(bool)` | 密码模式（显示 ● 代替字符） |
| `font_size(float)` | 字号 |
| `font_family(L"...")` | 字体 |
| `font_weight(int 100~900)` | 字重 |
| `text_color(Color)` / `bg_color(Color)` | 颜色 |
| `border_color(Color normal, Color focused)` | 边框颜色（失焦/聚焦） |
| `border_radius(float)` / `border_width(float)` | 边框样式 |
| `caret_color(Color)` | 光标颜色 |
| `selection_bg_color(Color)` / `selection_text_color(Color)` | 选区颜色 |
| `padding(float)` / `padding(t,r,b,l)` | 内边距 |
| `opacity(float 0~1)` | 不透明度 |
| `focus(bool)` | 获取/失去焦点（同时注册键盘目标） |
| `is_focused() -> bool` | 是否持有焦点 |
| `select_all()` | 全选 |
| `on_change(cb(wstring))` | 内容变化回调 |
| `on_enter(cb(wstring))` | 回车回调 |
| `on_focus_change(cb(bool))` | 焦点变化回调 |
| `layer() -> Layer*` | 获取底层图层 |

### 键盘操作（自动支持）
- 打字：插入字符
- Backspace / Delete：删除
- Ctrl+A：全选
- Ctrl+C / Ctrl+X / Ctrl+V：复制/剪切/粘贴
- Home / End / Left / Right：光标移动
- Shift + 方向键：扩展选区
- 鼠标拖拽：文本选区

### 实现要点（查源码用）
- `buffer_` — `std::wstring`，存储完整文本
- `cursor_pos_` — 光标在 buffer 中的字节偏移
- `sel_start_` / `sel_end_` — 选区锚点/活动端（== 无选区）
- `calc_scroll_offset()` — 根据光标位置计算水平滚动量
- `hit_test_pos(x, offset)` — 像素坐标 → buffer 字节偏移
- 光标闪烁用 `cursor_blink_` 计时器，`on_update` 驱动

---

## 2. TextArea — 多行编辑器

```cpp
Otter::TextArea ta(&win, parent, "name", x, y, w, h);
```

### 相比 TextField 的额外能力
| 特性 | 说明 |
|------|------|
| 多行文本 | 支持 `\n` 换行 |
| `word_wrap(bool)` | 自动换行（默认 true） |
| `show_line_numbers(bool)` | 行号标尺（v1.0.7+） |
| `gutter_width(float)` | 行号列宽 |
| `gutter_colors(Color bg, Color fg)` | 行号列配色 |
| `line_count() -> int` | 总行数 |
| `undo()` / `redo()` | 撤销/重做（Ctrl+Z/Y，v1.0.7+） |
| `can_undo() / can_redo() -> bool` | 是否有可撤销/重做操作 |
| `set_runs(vector<TextRun>)` | 富文本显示模式（只读样式预览，v1.0.7+） |
| `clear_runs()` | 切回纯文本编辑模式 |
| `has_runs() -> bool` | 当前是否在富文本显示模式 |
| Tab 键 | 插入 4 空格而非跳焦 |
| 选区拖拽边缘滚动 | 拖到视口外时自动滚动（v1.0.7+） |

### 富文本显示模式
```cpp
std::vector<Otter::TextRun> runs;
Otter::TextRun heading_run;
heading_run.text = L"第三章\n";
heading_run.style.font_size = 24.f;
heading_run.style.weight = Otter::TextStyle::Weight::Bold;
heading_run.style.color = Otter::Color::white();
runs.push_back(heading_run);

Otter::TextRun body_run;
body_run.text = L"\n正文内容…";
body_run.style.font_size = 16.f;
runs.push_back(body_run);

ta.set_runs(runs);        // 进入只读样式预览
// ... 用户点编辑时 ...
ta.clear_runs();          // 切回可编辑纯文本模式
ta.set_text(L"第三章\n\n正文内容…");
```

### Undo/Redo 机制
- 每次插入/删除操作自动记录 `EditRecord`（含操作类型、位置、文本）
- 连续单字符输入自动合并为一个记录（输入 "hello" → 一次 undo 全撤销）
- `record_insert(pos, text)` / `record_delete(pos, text)` 内部自动管理栈
- `undo_stack_` 最多保留……条（无上限，由 vector 自动扩容）

### 实现要点
- `buffer_` — 全局纯文本
- `cursor_line_` / `cursor_col_` — 逻辑行列坐标
- VisualLine 系统：`build_visual_lines()` 将 buffer 按 word_wrap 拆为视觉行
- `line_height_` — 基于字体量度的固定行高
- `scroll_x_` / `scroll_y_` — 滚动偏移（像素）
- 文字渲染：OpenGL 后端用 GDI `DrawTextW` 光栅化→纹理上传；D2D 后端用 DirectWrite `IDWriteTextLayout`

---

## 3. ImageView — 图片显示

```cpp
Otter::ImageView iv(parent, "name", x, y, w, h);
iv.src(L"path/to/image.png");
```

### API
| 方法 | 说明 |
|------|------|
| `src(L"path")` | 设置图片路径 |
| `border_radius(float)` | 圆角 |
| `opacity(float 0~1)` | 不透明度 |
| `fit(Fit)` | 缩放模式：Fill / Contain / Cover |
| `bg_color(Color)` | 背景色（图片加载前显示） |
| `layer() -> Layer*` | 底层图层 |

支持的图片格式：PNG / JPG / BMP / GIF（通过 WIC 解码）。

---

## 4. Checkbox / CheckboxEx — 复选框

```cpp
Otter::Checkbox cb(parent, "name", x, y, label);
cb.on_change([st](bool checked) { st->option = checked; });
bool v = cb.checked();
cb.set_checked(true);
```

`CheckboxEx` 支持自定义颜色：`check_color(Color)` / `box_bg(Color)` / `box_border(Color)`。

---

## 5. RadioButtonEx / RadioGroup — 单选按钮

```cpp
Otter::RadioGroup group;
Otter::RadioButtonEx rb1(parent, "opt_a", x, y,   L"Option A", &group);
Otter::RadioButtonEx rb2(parent, "opt_b", x, y+30, L"Option B", &group);
// 互斥自动：点 rb1 时 rb2 自动取消选中
int sel = group.selected_index();
```

---

## 6. Dropdown — 下拉选择框

```cpp
Otter::Dropdown dd(&win, parent, overlay_layer, "name", x, y);
dd.set_items({L"选项 A", L"选项 B", L"选项 C"});
dd.on_select([st](int idx, std::wstring_view item) {
    st->selected = idx;
});
int sel = dd.selected_index();
dd.select(0);  // 编程式选择
```

**注意**：Dropdown 需要 `overlay()` 层（弹出列表不被父层裁剪）。

---

## 7. TitleText — 标题文字标签

```cpp
Otter::TitleText tt(parent, "name", x, y, L"标题文字");
tt.font_size(18.f).color(Otter::Color::white());
```

---

## 8. ClipboardGuard — 剪贴板 RAII

```cpp
// 直接使用底层函数
Otter::clipboard_set(L"复制这段文字");
std::wstring text = Otter::clipboard_get();

// 内部 ClipboardGuard 确保异常安全：
// 构造时 OpenClipboard()，析构时 CloseClipboard()
```

---

## 控件通用模式

所有控件遵循一致的构造签名：
```cpp
ClassName(otterwindow*, Layer* parent, std::string_view name, float x, float y, float w, float h);
```
- `name` 用于图层树中标识（同父层内唯一）
- 构造时自动创建子图层、绑定事件、设置命中区域
- 返回的是栈对象，析构时自动从父层移除子图层（父层 `unique_ptr` 释放）

### 自定义控件模板
```cpp
class MyWidget
{
public:
    MyWidget(Otter::Layer* parent, std::string_view name, float x, float y, float w, float h)
        : layer_(parent->creat(std::string(name)))
    {
        layer_->translate(x, y).layer_bounds(0, 0, w, h).hit_area(0, 0, w, h);
        layer_->on_render([this](Otter::PaintChain& c, float) -> bool { render(c); return true; });
        layer_->on_click([this](const Otter::MouseEvent& e) -> bool { return on_click(e); });
    }

    Otter::Layer* layer() const { return layer_; }

private:
    void render(Otter::PaintChain& c) { /* 绘制 */ }
    bool on_click(const Otter::MouseEvent& e) { /* 处理点击 */ return true; }

    Otter::Layer* layer_ = nullptr;
};
```
