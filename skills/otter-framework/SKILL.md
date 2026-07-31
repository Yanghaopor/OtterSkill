---
name: otter-framework
description: 用 Otter（C++ header-only GUI 框架）构建、重构或排查桌面/跨平台 UI，涵盖窗口、图层、绘制、布局、输入、控件、性能、GPU/后端切换、P2P 和嵌入浏览器；当要教 AI 用这个框架写代码时使用本技能。
---

# Otter 框架使用指南（写给 AI — v4.0 完整版）

Otter 是一个 **header-only 的 C++20 GUI 框架**。核心范式：
**窗口（otterwindow）→ 图层树（Layer）→ 绘画链（PaintChain）**。
你通过给图层挂回调（`on_render` / `on_update` / `on_click` …）来描述界面与交互，
框架每帧驱动 `tick()`（逻辑）+ `flush()`（绘制）。

> **阅读顺序**：先读本文件（共 12 节），再按需读 `references/` 下的 11 个分册。
> **改动接口前必读仓库根目录的 `接口.md`，改完同步更新它。**

---

## 0. 框架安装位置（先知道文件在哪）

Otter 通过 **OtterFrameworkInstaller.exe**（Python + Tkinter GUI）自动部署到 Visual Studio。

### Windows（MSVC 编译）
安装器把框架部署到：
```
<VS安装目录>\VC\Auxiliary\VS\
  ├── OtterCreat.h          ← 顶层入口（只 include 这一个即可）
  ├── OtterLayer.h          ← 核心：所有数据类型 + Layer + PaintChain + AnimManager
  ├── OtterRenderer.h       ← D2D 渲染实现
  ├── OtterOpenGLRenderer.h ← OpenGL 渲染实现
  ├── OtterWindow.h         ← otterwindow 窗口类 + 主循环
  ├── OtterWidget.h         ← 内置控件（TextField / TextArea / ImageView / Dropdown 等）
  ├── OtterDebug.h          ← 日志系统（Logger / OTTER_LOG_* 宏 / ScopeTimer）
  ├── OtterInput.h          ← 键盘 Key 枚举 + 键盘状态查询
  ├── OtterText.h           ← 字体加载（FontLibrary）+ 富文本构建器（TextBuilder）
  ├── OtterPlatform.h       ← 平台抽象层（文件 / 网络 / 线程）
  ├── OtterOnline.h         ← P2P UDP 网络（NAT 穿透 / STUN / Relay）
  ├── OtterWindowBackend.h  ← 窗口后端接口（IWindowBackend）
  ├── OtterWindowFactory.h  ← 跨平台窗口工厂
  ├── OtterPortableOpenGLRenderer.h ← 便携 OpenGL 后端
  ├── OtterRaylibRenderer.h ← Raylib 渲染后端
  ├── app/                  ← 扩展组件（OtterChromeNode / OtterWebView2Node）
  ├── platform/             ← 平台后端（GLFW / 安卓）
  ├── examples/             ← 可编译运行的示例
  ├── tests/                ← 测试（test_core_headless.cpp）
  ├── docs/                 ← 完整文档（62 篇）
  └── OtterFramework.props  ← MSBuild 属性文件（自动引入头文件路径和库链接）
```

安装后，在 VS 项目中只需：
```cpp
#include "OtterCreat.h"  // 或 #include "OtterWindow.h"
```
MSBuild 通过 `OtterFramework.props` 自动注入 include 路径和所有必需的 `.lib` 链接。

### 跨平台 / 手动部署
如果不用安装器，直接把 `OtterFrameworkStandalone/` 目录下的头文件放到项目 include 路径中。
**头文件间有相对 include**（如 `#include "app/OtterChromeNode.h"`），不可打散目录结构。

### 源码学习路径
当需要深入理解框架行为时，按这个顺序读源码：
1. `OtterLayer.h` — 数据类型（Color / Rect / LayerStyle / TextStyle / GradientStop）+ Layer 类 + PaintChain
2. `OtterWindow.h` — otterwindow 类 + Win32 消息循环 + 鼠标/键盘事件分发
3. `OtterRenderer.h` — D2D 渲染后端（路径构建 / 阴影 / 文字 / 图片）
4. `OtterWidget.h` — 内置控件实现
5. `OtterOpenGLRenderer.h` — OpenGL 渲染后端（立即模式 / 纹理 / 着色器）
6. `OtterOnline.h` — P2P 网络（UDP / STUN / NAT 穿透 / Relay）

---

## 1. 第一性原理（务必先理解）

1. **header-only**：`#include "OtterWindow.h"` 即可，无需预编译库。
2. **图层是观察指针**：`creat()` / `find()` 返回 `Layer*`，**所有权属于父层的 `unique_ptr`**。绝不要 `delete` 它。需要长期持有就用弱句柄 `LayerHandle`（见 §9）。
3. **回调里捕获状态要用 `shared_ptr`**：lambda 长期存活，捕获裸引用易悬垂。标准写法 `auto st = std::make_shared<State>();` 然后各回调 `[st]` 捕获。
4. **两类坐标**：
   - 根画布 `__otter_canvas__` 上的 `on_render`：坐标是**窗口客户区像素**。
   - 用了 `LayoutPos` / `layer_bounds` 的子层 `on_render`：坐标是**相对自身 bounds 的局部坐标**（左上角 = 0,0）。
5. **禁止推导变量**（项目规则）：用 `Type var = value;` 风格显式类型，C++ 里即写 `int i = 0;` / `float w = ...;`，**不要 `auto` 滥用**在能写明类型处。
6. **渲染回调只做绘制**：结构创建、资源初始化、事件绑定尽量放在 `run()` 前完成；`on_render` 里不要再 `creat()`、`new` 或做重活。
7. **后端选择**：`RenderBackend::Direct2D`（Windows 高质量 2D，默认）、`RenderBackend::OpenGL`（跨平台 / 自定义 shader）。Direct2D 支持 `SetAntialiasMode` / `SetTextAntialiasMode`（ClearType 子像素渲染）；OpenGL 支持 `create_shader()` + `begin_layer_shader()` 自定义着色器后处理。

---

## 2. 做大型 UI 的固定流程

当用户要你"用 Otter 做一个应用"时，按这个顺序落地：

1. 先选平台和后端：Windows 走 `Direct2D`，跨平台走 `OpenGL`/GLFW。
2. 再定 UI 骨架：窗口级头部、侧边栏、内容区、浮层/弹窗。
3. 把业务状态收进一个 `shared_ptr<State>`，不要散落到多个裸全局变量。
4. 先建图层树，再绑回调，再做局部绘制。
5. 复杂界面优先拆成"组件类"或"面板类"，每个类只拥有一段图层子树。
6. 最后验证：先跑 `tests/test_core_headless.cpp`，再跑对应示例，最后做 GUI 回归。

如果是多页面或大型面板，先把"状态 / 组件 / 布局 / 交互"分开想：
- 状态：纯数据，少引用 UI 对象。
- 组件：负责创建图层、绑定事件、刷新显示。
- 布局：负责坐标和尺寸。
- 交互：负责事件到状态的转换。

这套拆分比把逻辑写进一个巨型 `on_render` 更稳，也更容易让 AI 改代码。

---

## 3. 最小可运行程序（背下来）

```cpp
#include "../OtterWindow.h"

int main()
{
    // OpenGL 后端 = 跨平台路径；Windows 默认 Direct2D
    Otter::otterwindow win(800, 600, L"标题", Otter::RenderBackend::OpenGL);
    win.borderless().draggable().rounded_corners()
       .set_clear_color(Otter::Color::from_rgb_hex(0x101418));

    Otter::Layer* root = win.get["__otter_canvas__"];   // 根画布
    if (!root) return 1;

    root->on_render([&win](Otter::PaintChain& c, float dt) -> bool {
        float W = static_cast<float>(win.width());
        float H = static_cast<float>(win.height());
        c.fill_round_rect(20, 20, W - 40, H - 40, 16,
                          Otter::Color::from_rgb_hex(0x1E2530));
        Otter::TextStyle ts;
        ts.font_family = L"Segoe UI";
        ts.font_size = 28;
        ts.color = Otter::Color::white();
        c.text(L"Hello Otter", 48, 48, ts);
        return true;          // 返回 true = 下一帧继续触发；false = 注销该回调
    });

    win.run();                // 进入主循环（阻塞直到窗口关闭）
    return 0;
}
```

### D2D 版本（Windows 默认，ClearType 文字）
```cpp
Otter::otterwindow win(800, 600, L"Hello Otter");  // 默认 Direct2D
// 其余完全一样——两个后端 API 100% 兼容
```

---

## 4. 编译与测试

### MSVC（Windows — 安装器部署后）
```batch
cl /EHsc /std:c++20 /MD /W3 /nologo /utf-8 myapp.cpp ^
   user32.lib ole32.lib oleaut32.lib d2d1.lib dwrite.lib ^
   shell32.lib gdi32.lib opengl32.lib windowscodecs.lib ^
   winhttp.lib ws2_32.lib kernel32.lib /Fe:myapp.exe
```
如果安装了 `OtterFramework.props`，VS 项目会自动包含这些库。

### g++（MSYS2 MinGW-w64 — 跨平台）
```bash
g++ -std=c++20 -I. examples/counter.cpp -o counter.exe \
    -ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 \
    -ldwmapi -lopengl32 -lwinhttp -limm32
```

**链接库清单（Windows GUI 必需）**：
`d2d1 dwrite windowscodecs gdi32 user32 ole32 dwmapi opengl32 winhttp imm32 shell32`

**附加库（用到特定功能时）**：
`ws2_32`（OtterOnline P2P 网络）、`comctl32`（原生 Edit 控件）、`comdlg32`（文件对话框）

### 测试命令
```bash
# 核心层无渲染单元测试（无需 GPU/窗口）
g++ -std=c++20 -I. tests/test_core_headless.cpp -o test_core && ./test_core

# 一键全量测试
bash tests/run_tests.sh

# 生成文档
bash generate_docs.sh
```

> ⚠️ 安卓必须用 **C++20**（`OtterLayer.h` 用了 `unordered_map` 异质查找，C++17 在严格 libc++ 下报错）。

---

## 5. 你最常用的 API（速查）

### 窗口（`otterwindow`，构造后链式配置）
| 方法 | 说明 |
|------|------|
| `borderless(bool=true)` | 无系统边框 |
| `draggable(bool=true)` | 拖动客户区移动窗口 |
| `rounded_corners(bool,bool)` | Win11 系统圆角 |
| `resizable(bool)` / `min_size(w,h)` / `max_size(w,h)` | 缩放控制 |
| `topmost(bool=true)` | 置顶 |
| `set_clear_color(Color)` | 每帧清屏颜色 |
| `glass_mode(GlassMode)` | Acrylic/Mica 玻璃材质 |
| `glass_tint(Color)` | 玻璃色调 |
| `vsync(bool)` | 垂直同步（默认开，推荐） |
| `target_fps(int)` | 帧率限制（0=不限） |
| `render_mode(RenderMode)` | GPU/CPU/Auto 算力模式 |
| `render_backend(RenderBackend)` | 运行时切换后端 |
| `Layout(cols,rows,...)` | 窗口级网格布局 |
| `enable_scroll(content_h)` / `auto_scroll(bool)` | 窗口级滚动 |
| `enable_drop_files(bool)` | 文件拖放 |
| `set_keyboard_target(...)` | 注册键盘焦点 |
| `on_ready(cb)` / `on_close(cb)` | 生命周期回调 |
| `creat["name"]` → `LayerRef` | 语法糖创建子层 |
| `get["name"]` → `Layer*` | 取根画布或子层 |
| `overlay()` → `Layer*` | 取顶层浮层 |
| `run()` | 阻塞进入主循环 |
| `close()` | 请求关闭 |

### 图层（`Layer`，链式返回 `Layer&`）
| 方法 | 说明 |
|------|------|
| `creat("name")→Layer*` | 创建/复用同名子层（幂等） |
| `find("name")→Layer*` | 递归查找子树 |
| `get_child("name")→Layer*` | 仅查找直接子层 |
| `layer_bounds(x,y,w,h)` | 手动指定可视边界 |
| `background(Color)` | 背景色（框架自动绘制） |
| `border_radius(float)` | 圆角半径 |
| `border(float w, Color c)` | 边框 |
| `clip_content(bool)` | 内容裁切到边界 |
| `blur(float r)` | 高斯模糊（D2D 1.0 近似） |
| `feather(float w)` | 边缘羽化 |
| `shader(unsigned int id)` | OpenGL 着色器后处理 |
| `opacity(float 0~1)` | 不透明度 |
| `visible(bool)` | 可见性（false=不渲染+不响应事件） |
| `blend(BlendMode)` | 混合模式 |
| `hit_area(x,y,w,h)` | 手动命中矩形 |
| `auto_hit_area(bool)` | 命中区=resolved bounds |
| `hit_test(cb)` | 自定义命中测试 |
| `translate(tx,ty)` / `scale(sx,sy)` / `rotate(rad)` | 变换 |
| `bring_to_front()` / `send_to_back()` | z 序 |
| `Layout(cols,rows,...)` | 设为布局容器 |
| `LayoutPos(col,row,span_c,span_r)` | 在父网格中占位 |
| `enable_scroll(cfg)` / `disable_scroll()` | 图层级滚动 |
| `scroll_to(x,y)` / `scroll_by(dx,dy)` | 滚动控制 |
| `animate_opacity/translate/scale/rotate(to,dur,easing,cb)` | 补间动画 |
| `cancel_animations()` | 取消该层所有动画 |
| `on_render(cb)` / `on_update(cb)` | 帧回调 |
| `on_click / on_mouse_down/up/move/enter/leave/hover / on_wheel` | 鼠标事件 |
| `on_double_click / on_right_click` | 双击/右键 |
| `on_drop_files(cb)` | 文件拖放 |
| `remove_mouse_callbacks(MouseEventType)` | 清除指定类型回调 |
| `handle()→LayerHandle` | 获取弱引用句柄 |
| `resolved_bounds()→const Rect*` | 读取当前有效边界 |
| `world_x() / world_y()` | 读取世界坐标 |
| `set_native_component(p,dtor)` | 挂接原生组件 |
| `native_component<T>()→T*` | 读取原生组件 |

### 绘画链（`PaintChain`，链式返回 `PaintChain&`）
**路径构造**：`move_to(x,y)` `line_to(x,y)` `bezier_to(cx1,cy1,cx2,cy2,ex,ey)` `arc(cx,cy,r,start,end)` `close()`

**填充/描边**：`fill(Color)` `fill(FillStyle)` `fill(LinearGradient)` `fill(RadialGradient)` `fill(ConicGradient)` `stroke(Color,w)` `stroke(StrokeStyle)` `shadow(ShadowStyle)`

**快捷形状**：`fill_rect` `fill_round_rect` `stroke_round_rect` `fill_circle` `stroke_circle` `stroke_line`

**文字**：`text(L"...",x,y,style)` `text(L"...",x,y,max_w,max_h,style)`（带框换行+对齐）

**图片**：`img(path,x,y,w,h,opacity,radius)` `bitmap_bgra(pixels,w,h,stride,x,y,w,h)`

**裁切**：`push_clip_rect(x,y,w,h)` `push_round_clip(x,y,w,h,r)` `pop_clip()`

**自定义**：`custom(unique_ptr<PaintOp>)`

### 内置控件（`OtterWidget.h` — 详见 `references/09-widgets.md`）
- `TextField` — 单行输入框（光标/选择/复制粘贴/密码模式）
- `TextArea` — 多行编辑器（行号/undo/redo/Tab缩进/富文本显示模式）
- `ImageView` — 图片显示
- `Checkbox` / `CheckboxEx` — 复选框
- `RadioButtonEx` / `RadioGroup` — 单选按钮
- `Dropdown` — 下拉选择框
- `TitleText` — 标题文字标签

### 颜色（`Color`）
```cpp
Otter::Color::from_rgb_hex(0x39D0FF);           // 不透明
Otter::Color::from_rgb_hex(0x39D0FF, 0.5f);     // 带 alpha
Otter::Color::from_rgba_hex(0x39D0FF80);         // RGBA 一体
Otter::Color::white();   Otter::Color::black();
Otter::Color::red();     Otter::Color::green();   Otter::Color::blue();
Otter::Color::transparent();                      // alpha=0 真透明
Otter::Color::transparent_key();                  // 颜色键透明（纯黑）
Otter::Color c{r, g, b, a};                       // 直接构造（分量 0~1）
Otter::Color::lerp(a, b, t);                      // 颜色插值
c.is_pure_black();  c.to_near_black();            // 颜色键辅助
```

---

## 6. 关键陷阱（AI 最易犯 — 14 条）

1. ❌ **在 `on_render` 里 `new Layer` 或 `creat`**：图层结构应在初始化时建好，渲染回调只画。每帧 creat 会反复创建/查找，性能与逻辑都错。
2. ❌ **裸引用捕获局部状态**：`[&state]` 当 state 是栈变量时，`win.run()` 期间它已失效。用 `shared_ptr` + `[st]`。
3. ❌ **`auto` 滥用**（违反项目规则）：能写明类型就写，如 `Otter::Layer* p = ...`。
4. ❌ **混淆坐标系**：子层（LayoutPos/layer_bounds）的 `on_render` 用局部坐标，不要再加父层偏移。
5. ❌ **后台线程碰 UI**：`OtterOnline` 的网络回调必须经 `poll_events()` 投递到主线程，不要在 worker 线程直接改图层。
6. ❌ **保存裸 `Layer*` 跨帧又可能被销毁**：改用 `LayerHandle`，访问前 `if (h.get())`。
7. ❌ **在布局层里再手工叠加父偏移**：`LayoutPos` 子层的 `on_render` 已经是局部坐标。
8. ❌ **忘记返回 `true` 持续渲染**：`on_render` 返回 `false` 会静默注销回调——内容只画一帧就消失。
9. ❌ **`set_text(L"...")` 后直接读 `text()` 做光标计算**：`set_text` 内部调 `get_lines()` 重建了光标位置，之后再改 `buffer_` 会让光标与行号不同步。用 `set_text` 做初始化，用 `handle_char` 做增量编辑。
10. ❌ **窗口还没 `run()` 就读 `width()/height()` 做复杂布局**：构造后尺寸是初始值，但 `borderless()` / `dwm_transparent()` 会在 `run()` 内部调 `SetWindowPos` 改变实际尺寸。需要在 `on_ready` 或首帧 `on_render` 里读真实尺寸。
11. ❌ **`include` 顺序随意**：必须先 `#include "OtterWindow.h"`（它内部定义 `NOMINMAX`、`WIN32_LEAN_AND_MEAN`、`OTTER_PI`），再 include 自己的东西。反序会导致 Windows 头文件污染。
12. ❌ **D2D 后端下用纯黑色 (`0,0,0,1`) 做颜色键透明窗口**：纯黑色会被颜色键抠掉变透明。要用 `Color::to_near_black()` 或关闭颜色键模式。
13. ❌ **CEF 浏览器目标用 g++ 编译**：`libcef.lib` 是 MSVC ABI。含 CEF 目标的编译必须用 `cl`（MSVC）。
14. ❌ **多次调用 `set_keyboard_target` 但不保存上一个**：框架会在设置新的之前调用上一个的 defocus 回调，但如果你的 defocus 回调里做了清理，确保数据一致性。

---

## 7. 选哪个后端 / 平台？

| 目标 | 怎么做 |
|------|--------|
| Windows 高质量 2D（ClearType 文字） | 默认 `RenderBackend::Direct2D` |
| 跨平台（Linux/macOS via GLFW） | `RenderBackend::OpenGL` + 定义 `OTTER_USE_GLFW`，用 `create_platform_window` |
| OpenGL 自定义 shader 后处理 | `RenderBackend::OpenGL` + `renderer().create_shader(src)` + `layer->shader(id)` |
| 安卓 APK | `create_android_window(android_app*)`，C++20，见 `接口.md` §6 |
| 只测核心逻辑（CI/无显卡） | 不实例化窗口，直接构造 `Layer` 测树/事件，见 `tests/test_core_headless.cpp` |
| 嵌入网页/浏览器 | CEF（`OtterChromeLayer`，OSR，可透明混合）或 WebView2（原生子窗口，最流畅）。**必须 MSVC 编译**，见 `references/06-browser-cef.md` |
| 最高 GPU 性能（跳过 CPU 回读） | Direct2D + `render_mode(GPU)` + `vsync(true)` |

---

## 8. 自建 UI 的写法

Otter 不是"画一张大图"，而是"用图层组成控件树"。常见做法：

- 页面根：挂在 `__otter_canvas__`。
- 卡片：`layer_bounds + background + border_radius + opacity`。
- 按钮：`hit_area` 或 `auto_hit_area(true)` + `on_click`/`on_mouse_enter`/`leave`。
- 输入框：`set_keyboard_target`，并在点击时切焦点。
- 列表/网格：`Layout` + `LayoutPos`，子层局部坐标绘制。
- 弹窗/提示：放到 `overlay()` 或单独浮层，避免被主体内容遮住。
- 滚动区域：`enable_scroll(content_h)` 或 `win.auto_scroll(true)`。

如果要做可复用控件，优先写成一个小类：
```cpp
class ToggleCard
{
public:
    ToggleCard(Otter::Layer* parent, std::shared_ptr<State> st);
private:
    void build();
    void bind();
    Otter::Layer* root_ = nullptr;
    std::shared_ptr<State> st_;
};
```

原则：构造时建图层，绑定时接事件，运行时只改状态不改结构。

---

## 9. 内存安全：LayerHandle

```cpp
Otter::LayerHandle h = layer->handle();   // 弱引用，零成本懒创建
if (Otter::Layer* p = h.get()) p->visible(false);  // 失效则 get()==nullptr
if (h) h->bring_to_front();                // operator bool + operator->
```
图层（或其父子树）被销毁后，所有指向它的 `LayerHandle` 自动失效，杜绝悬垂访问。

---

## 10. 排错与性能

优先按这个顺序排：
1. 先看图层是否真的挂上了：`root` 是否为空、`Layer*` 是否为 `nullptr`。
2. 再看坐标：根画布用窗口坐标，子层用局部坐标。
3. 再看事件：有没有 `hit_area`、有没有切到键盘焦点、回调有没有返回 `true` 把事件吃掉。
4. 再看性能：有没有在 `on_render` 里创建对象、反复查找、重复分配。
5. 最后再切后端或改 GPU 路径。

性能守则：
- `win.vsync(true)` 默认应保留。
- `win.render_mode(Otter::RenderMode::GPU)` 用来强制硬件路径。
- `win.target_fps(0)` 只在确实知道自己在做什么时才用。
- 绘制链和状态对象尽量复用，不要每帧创建临时大对象。

---

## 11. 参考分册（references/ — 共 11 册）

| # | 文件 | 内容 |
|---|------|------|
| 01 | `01-window.md` | 窗口创建、玻璃材质、无边框拖动、性能接口 |
| 02 | `02-layer-paint.md` | 图层树、PaintChain 全量绘制 API、坐标系 |
| 03 | `03-input.md` | 鼠标事件、键盘输入、命中测试 |
| 04 | `04-layout-anim.md` | 网格布局、动画（补间+逐帧） |
| 05 | `05-online.md` | P2P 网络（UDP/STUN/NAT/Relay）与线程安全 |
| 06 | `06-browser-cef.md` | 嵌入浏览器（CEF/WebView2）、视频编解码器、GPU 路径 |
| 07 | `07-oop-ui.md` | 大型 UI 的 OOP 拆分、组件组织、状态管理 |
| 08 | `08-debug-performance.md` | 排错清单、性能调优、后端切换、GPU/浏览器疑难 |
| 09 | `09-widgets.md` | **内置控件 API 全解**（TextField / TextArea / ImageView / Checkbox / Dropdown 等） |
| 10 | `10-file-io-patterns.md` | **文件读写 / 剪贴板 / 编码 / 持久化模式** |
| 11 | `11-source-internals.md` | **源码结构导航 + 关键实现细节 + 如何阅读框架源码排查问题** |

仓库根 `接口.md` 是接口权威清单——任何接口改动都要读它并同步更新。

---

## 12. 技能元数据

- **框架名称**: Otter（水獭图形框架） / OtterCreat 4.0 Professional
- **语言标准**: C++20（header-only）
- **主要平台**: Windows 10/11（Direct2D + DirectWrite）、跨平台（OpenGL + GLFW）、Android
- **命名空间**: `Otter`（核心）、`Otter::Demo`（示例）、`Otter::Online`（P2P）、`Otter::Platform`（平台抽象）
- **编译要求**: MSVC 2022+ 或 g++ 15.2+（MinGW-w64），C++20，安卓需 C++20
- **安装方式**: `OtterFrameworkInstaller.exe`（自动部署到 VS）或手动复制头文件到 include 路径
