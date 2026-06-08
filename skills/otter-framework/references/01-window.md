# 01 — 窗口（otterwindow）

`otterwindow` 是 Windows 默认窗口（Win32 + Direct2D/OpenGL）。所有配置方法返回
`otterwindow&`，可链式调用。配置应在 `win.run()` **之前**完成。

## 构造

```cpp
Otter::otterwindow win(width, height, L"标题");                       // 默认 D2D
Otter::otterwindow win(w, h, L"标题", Otter::RenderBackend::OpenGL);  // OpenGL 后端
```

## 窗体外观 / 行为

| 方法 | 作用 |
|------|------|
| `borderless(bool=true)` | 无系统边框 |
| `draggable(bool=true)` | 拖动客户区移动窗口 |
| `rounded_corners(bool=true,bool small=false)` | 系统圆角（Win11） |
| `rounded_region(float r=18)` | 用窗口区域裁出圆角（兼容旧系统） |
| `resizable(bool=true)` / `min_size(w,h)` / `max_size(w,h)` | 缩放 |
| `topmost(bool=true)` | 置顶 |
| `set_clear_color(Color)` | 每帧清屏色 |

## 玻璃 / 透明材质

```cpp
win.glass_mode(Otter::otterwindow::GlassMode::Acrylic)   // 亚克力模糊
   .glass_tint(Otter::Color::from_rgb_hex(0x1D2430, 0.42f))
   .keep_glass_active_on_blur(true);
```
其它：`dwm_transparent(true)`、`color_key_transparent()`、`layered(alpha)`。

## 性能 / 算力（v1.0.2）

```cpp
win.vsync(true);                              // 默认开，锁刷新率（强烈推荐）
win.target_fps(60);                           // vsync(false) 时自定义限速；0=不限
win.render_mode(Otter::RenderMode::GPU);      // GPU/CPU/Auto；默认 GPU
int fps = win.current_fps();
float ms = win.frame_time_ms();
```
> 默认 GPU 是为了避免部分核显在 Auto 下被驱动悄悄回退到 WARP 软件渲染，跑满显存/CPU。

## 后端运行时切换

```cpp
win.render_backend(Otter::RenderBackend::OpenGL);  // 会重建渲染器
```

## 生命周期回调

```cpp
win.on_ready([]{ /* 窗口就绪 */ });
win.on_close([]{ /* 关闭前 */ });
```

## 滚动

```cpp
win.enable_scroll(content_height);   // 开启竖向滚动
win.auto_scroll(true);               // 自动按内容计算
```

## 取根画布与 overlay

```cpp
Otter::Layer* root = win.get["__otter_canvas__"];  // 所有内容挂这下面
Otter::Layer* ov   = win.overlay();                // 浮层（弹窗/提示，绘制在最上）
// 创建子层的语法糖：win.creat["name"] 返回 LayerRef（可继续 .creat[...]）
```

## 跨平台窗口（GLFW）

```cpp
Otter::WindowCreateInfo info;
info.width = 960; info.height = 640; info.title = L"App";
info.backend = Otter::RenderBackend::OpenGL;   // GLFW 仅 OpenGL
auto win = Otter::create_platform_window(info); // 需定义 OTTER_USE_GLFW
win->run();
```
