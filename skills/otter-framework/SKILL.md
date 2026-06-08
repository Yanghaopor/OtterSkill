---
name: otter-framework
description: 用 Otter（C++ header-only GUI 框架）构建桌面/跨平台界面。当需要在本仓库创建窗口、图层、绘制、交互、布局、键鼠输入、P2P 网络或嵌入浏览器时使用本技能。
---

# Otter 框架使用指南（写给 AI）

Otter 是一个 **header-only 的 C++20 GUI 框架**。核心范式：
**窗口（otterwindow）→ 图层树（Layer）→ 绘画链（PaintChain）**。
你通过给图层挂回调（`on_render` / `on_update` / `on_click` …）来描述界面与交互，
框架每帧驱动 `tick()`（逻辑）+ `flush()`（绘制）。

> 阅读顺序：先读本文件，再按需读 `references/` 下的分册。
> **改动接口前必读仓库根目录的 `接口.md`，改完同步更新它。**

---

## 0. 第一性原理（务必先理解）

1. **header-only**：`#include "OtterWindow.h"` 即可，无需预编译库。
2. **图层是观察指针**：`creat()` / `find()` 返回 `Layer*`，**所有权属于父层的
   `unique_ptr`**。绝不要 `delete` 它。需要长期持有就用弱句柄 `LayerHandle`（见 §6）。
3. **回调里捕获状态要用 `shared_ptr`**：lambda 长期存活，捕获裸引用易悬垂。
   标准写法 `auto st = std::make_shared<State>();` 然后各回调 `[st]` 捕获。
4. **两类坐标**：
   - 根画布 `__otter_canvas__` 上的 `on_render`：坐标是**窗口客户区像素**。
   - 用了 `LayoutPos` / `layer_bounds` 的子层 `on_render`：坐标是**相对自身 bounds 的局部坐标**（左上角 = 0,0）。
5. **禁止推导变量**（项目规则）：用 `var:类型` 风格的显式类型，C++ 里即写
   `int i = 0;` / `float w = ...;`，**不要 `auto` 滥用**在能写明类型处。

---

## 1. 最小可运行程序（背下来）

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

---

## 2. 编译与测试（Windows / MinGW g++）

本仓库验证用 **g++ 15.2.0（MSYS2 MinGW-w64）**，C++20。

```bash
# 跨平台核心层单元测试（无需 GPU/窗口，可在任意平台跑）
g++ -std=c++20 -I. tests/test_core_headless.cpp -o test_core && ./test_core

# 一键全量测试：核心测试 + 全头文件语法检查 + 示例链接成 .exe
bash tests/run_tests.sh            # 或 core / headers / examples 单跑

# 手动链接一个 GUI 示例（Windows）
g++ -std=c++20 -I. examples/counter.cpp -o counter.exe \
    -ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 \
    -ldwmapi -lopengl32 -lwinhttp -limm32
```

**链接库清单（Windows GUI 必需）**：
`d2d1 dwrite windowscodecs gdi32 user32 ole32 dwmapi opengl32 winhttp imm32`

> ⚠️ 安卓必须用 **C++20**（`OtterLayer.h` 用了 unordered_map 异质查找，C++17 在严格
> libc++ 下报错）。MSVC 在 C++17 也能编是因为它把异质查找当扩展放行了。

---

## 3. 你最常用的 API（速查）

### 窗口（`otterwindow`，链式）
`borderless()` `draggable()` `rounded_corners()` `resizable()` `topmost()`
`set_clear_color(Color)` `vsync(bool)` `target_fps(int)` `render_mode(RenderMode)`
`glass_mode(GlassMode)` `glass_tint(Color)` `Layout(cols,rows,...)` `close()` `run()`

### 图层（`Layer`，链式）
`creat("name")→Layer*` `find("name")→Layer*` `get_child("name")→Layer*`
`layer_bounds(x,y,w,h)` `background(Color)` `border_radius(r)` `opacity(f)`
`visible(bool)` `hit_area(x,y,w,h)` `auto_hit_area(bool)` `LayoutPos(col,row,...)`
`bring_to_front()` `handle()→LayerHandle` `resolved_bounds()→const Rect*`
`animate_opacity(to,dur,Easing)` `animate_rotate(to,dur,Easing)`

### 事件回调（都返回 `bool`：true=消费/继续）
`on_render(cb(PaintChain&,float))` `on_update(cb(float dt))`
`on_click` `on_mouse_down` `on_mouse_up` `on_mouse_move`
`on_mouse_enter` `on_mouse_leave` `on_wheel`（参数 `const MouseEvent&`）

### 绘画链（`PaintChain`，链式）
路径：`move_to` `line_to` `bezier_to` `arc` `close`
填充/描边：`fill(Color/Gradient)` `stroke(Color,width)`
快捷：`fill_rect` `fill_round_rect` `stroke_round_rect` `fill_circle` `stroke_line`
文字：`text(L"...", x, y, style)` / `text(L"...", x, y, w, h, style)`（带框换行）
图片：`img(path,x,y,w,h)` 裁切：`push_clip_rect/pop_clip`

详细分册见 `references/`。

---

## 4. 关键陷阱（AI 最易犯）

- ❌ **在 `on_render` 里 `new Layer` 或 `creat`**：图层结构应在初始化时建好，
  渲染回调只画。每帧 creat 会反复创建/查找，性能与逻辑都错。
- ❌ **裸引用捕获局部状态**：`[&state]` 当 state 是栈变量时，`win.run()` 期间它已失效。
  用 `shared_ptr` + `[st]`。
- ❌ **`auto` 滥用**（违反项目规则）：能写明类型就写，如 `Otter::Layer* p = ...`。
- ❌ **混淆坐标系**：子层（LayoutPos/layer_bounds）的 `on_render` 用局部坐标，
  不要再加父层偏移。
- ❌ **后台线程碰 UI**：`OtterOnline` 的网络回调必须经 `poll_events()` 投递到主线程，
  不要在 worker 线程直接改图层（见 references/05-online.md）。
- ❌ **保存裸 `Layer*` 跨帧又可能被销毁**：改用 `LayerHandle`，访问前 `if (h.get())`。

---

## 5. 选哪个后端 / 平台？

| 目标 | 怎么做 |
|------|--------|
| Windows 高质量 2D | 默认 `RenderBackend::Direct2D` |
| 跨平台（Linux/macOS via GLFW） | `RenderBackend::OpenGL` + 定义 `OTTER_USE_GLFW`，用 `create_platform_window` |
| 安卓 APK | `create_android_window(android_app*)`，C++20，见 `接口.md` §6 |
| 只测核心逻辑（CI/无显卡） | 不实例化窗口，直接构造 `Layer` 测树/事件，见 `tests/test_core_headless.cpp` |
| 嵌入网页/浏览器 | CEF（`OtterChromeLayer`，OSR，可透明混合）或 WebView2（原生子窗口，最流畅）。**必须 MSVC 编译**，见 `references/06-browser-cef.md` |

---

## 6. 内存安全：LayerHandle

```cpp
Otter::LayerHandle h = layer->handle();   // 弱引用，零成本懒创建
if (Otter::Layer* p = h.get()) p->visible(false);  // 失效则 get()==nullptr
if (h) h->bring_to_front();                // operator bool + operator->
```
图层（或其父子树）被销毁后，所有指向它的 `LayerHandle` 自动失效，杜绝悬垂访问。

---

## 参考分册（references/）

- `references/01-window.md` — 窗口创建、玻璃材质、无边框拖动、性能接口
- `references/02-layer-paint.md` — 图层树、PaintChain 全量绘制 API、坐标系
- `references/03-input.md` — 鼠标事件、键盘输入、命中测试
- `references/04-layout-anim.md` — 网格布局、动画
- `references/05-online.md` — P2P 网络与线程安全
- `references/06-browser-cef.md` — 嵌入浏览器（CEF/WebView2）、视频编解码器、GPU 路径陷阱

仓库根 `接口.md` 是接口权威清单——任何接口改动都要读它并同步更新。
