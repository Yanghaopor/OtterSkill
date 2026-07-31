# 11 — 源码结构导航 + 关键实现细节

当框架行为与预期不符、需要查源码定位问题时，按本册导航。

## 目录
1. 源码文件地图（每个文件里有什么）
2. 数据流向（一帧的生命周期）
3. 渲染管线（begin_frame → flush → end_frame）
4. 事件分发链路（WM_* → Layer::dispatch_*）
5. 图层树管理（creat / find / flush 递归）
6. OpenGL vs D2D 后端差异
7. 关键常量和宏
8. 常见源码级排查路径

---

## 1. 源码文件地图

### OtterCreat.h（1KB）
顶层聚合头文件。只做一件事：
```cpp
#include "OtterOnline.h"
```
仅当需要 P2P 网络功能时 include。一般直接用 `OtterWindow.h`。

### OtterLayer.h（~135KB — 核心）
**框架最大的文件**，包含所有数据类型定义 + Layer 类 + PaintChain + AnimManager：

| 行号范围 | 内容 |
|---------|------|
| 1-65 | 前向声明、Rect、MouseEvent、BlendMode、LayerStyle |
| 65-225 | Color、StrokeStyle、GradientStop、各种渐变 |
| 225-450 | FillStyle、ShadowStyle、Easing、AnimTrack、Animation、AnimManager |
| 450-650 | TextStyle（字体/颜色/对齐/装饰/渲染模式） |
| 650-850 | ScrollBarStyle、ScrollConfig、TextMetrics |
| 850-1050 | LayerTransform、LayoutConfig、LayoutPosition、LayerEffect |
| 1050-1100 | RenderContext 纯虚接口（所有渲染后端的抽象基类） |
| 1100-1450 | PaintOp 节点（MoveToOp、LineToOp、FillOp...）+ PaintChain |
| 1450-2800 | **Layer 类**（核心：creat/find、flush/tick、事件分发、滚动、CSS 效果） |
| 2800+ | 私有辅助（compute_sb_states、s_collect_bounds 等） |

### OtterWindow.h（~65KB）
`otterwindow` 类 + Win32 消息循环。关键段：

| 位置 | 内容 |
|------|------|
| 构造 | 注册窗口类、CreateWindowExW、初始化渲染器、设置动画管理器 |
| `run()` | PeekMessage 循环 → anim_manager_.tick → canvas_.tick → render_one_frame |
| `render_one_frame()` | begin_frame + flush + end_frame + D2DERR_RECREATE_TARGET 恢复 |
| `WndProc` | WM_SIZE/WM_MOUSEMOVE/WM_LBUTTONDOWN... → dispatch_mouse_* |
| `WM_KEYDOWN` | → keyboard_key_cb_ / DefWindowProc |
| `WM_CHAR` | → keyboard_char_cb_ |
| `set_keyboard_target` | Win32 重载 vs 跨平台重载（Key 枚举） |
| 透明/无边框 | borderless / dwm_transparent / color_key_transparent |
| `initialize_renderer` | 根据 backend_ 创建 D2DRenderContext 或 OpenGLRenderContext |

### OtterRenderer.h（~80KB）
Direct2D 渲染后端 `D2DRenderContext`：

| 位置 | 内容 |
|------|------|
| `initialize` | 创建 D2D 工厂 + HwndRenderTarget（固定 96 DPI） + DirectWrite 工厂 |
| `begin_frame` / `end_frame` | BeginDraw → Clear → ... → EndDraw |
| `push_transform` / `pop_transform` | 矩阵栈（D2D::Matrix3x2F） |
| `push_style` / `pop_style` | 透明度继承栈 |
| `cmd_move_to` / `cmd_line_to` / ... | 路径构建（GeometrySink 状态机） |
| `cmd_fill` / `cmd_stroke` | 提交路径 → FillGeometry / DrawGeometry |
| `cmd_fill_round_rect` | FillRectangle 或 RoundedRectangleGeometry |
| `cmd_text` | DirectWrite TextLayout（阴影→描边→文字本体） |
| `cmd_draw_bitmap` | WIC 解码 → ID2D1Bitmap |
| `cmd_blur_rect` / `cmd_feather_rect` | 多层半透明叠加近似 |
| `make_brush` | 画笔 LRU 缓存（256 条目，v1.0.7） |
| `get_text_format` | TextFormat 缓存（按字体族+字号+字重+字形） |
| 图片异步下载 | `download_thread_` + `download_queue_` + WinHTTP |
| `release_render_resources` | 设备丢失时释放所有渲染目标相关资源 |

### OtterOpenGLRenderer.h（~85KB）
OpenGL 立即模式渲染后端 `OpenGLRenderContext`：

| 位置 | 内容 |
|------|------|
| `initialize` | WGL 上下文创建、扩展加载（glCreateShader 等）、shader 入口点 |
| `begin_frame` | 重置矩阵栈、透明度栈、clip 栈、路径 |
| `cmd_move_to` / `cmd_line_to` / `cmd_arc` / `cmd_bezier_to` | 路径顶点积累到 `path_` |
| `cmd_fill` | `draw_polygon` → GL_TRIANGLE_FAN + 逐顶点颜色（支持渐变） |
| `cmd_stroke` | GL_LINE_LOOP/STRIP + 阴影支持 |
| `cmd_text` | GDI DrawTextW 光栅化 → 纹理上传 → 绘制 textured quad |
| `cmd_draw_bitmap` | WIC 解码 → `glTexImage2D` → textured quad |
| `color_for_point` | 根据 FillStyle::Type 计算逐顶点颜色（Solid/Linear/Radial/Conic） |
| `subdivide_polygon_edges` | 锥形渐变时细分边界到 72 顶点（v1.0.7） |
| `create_shader` / `begin_layer_shader` / `end_layer_shader` | GLSL 着色器后处理 |
| `rebuild_stencil_clip` | 用模板缓冲实现多层裁切 |
| 文字纹理缓存 | `text_cache_`（512 条目 LRU，v1.0.7） |
| 位图缓存 | `bitmap_cache_`（256 条目 LRU，v1.0.7） |

### OtterWidget.h（~95KB）
内置控件。全部基于 Layer + PaintChain 实现，不使用 Win32 子窗口：

| 类 | 行号 | 说明 |
|----|------|------|
| `ClipboardGuard` | ~50 | RAII 剪贴板守卫 |
| `TextField` | ~140 | 单行输入框 |
| `TextArea` | ~900 | 多行编辑器 |
| `ImageView` | ~2000 | 图片显示 |
| `Checkbox` | ~2050 | 复选框 |
| `TitleText` | ~2185 | 标题标签 |
| `Dropdown` | ~2330 | 下拉选择框 |
| `RadioButtonEx` / `RadioGroup` | ~2740 | 单选按钮 |
| `CheckboxEx` | ~2975 | 可定制颜色复选框 |

### 其他文件
| 文件 | 大小 | 内容 |
|------|------|------|
| `OtterDebug.h` | 5KB | Logger（单例）、`format_log`、ScopeTimer、`OTTER_LOG_*` 宏 |
| `OtterInput.h` | 3KB | `Key` 枚举、`key_from_ascii`、`key_from_native`、`is_key_down` |
| `OtterText.h` | 15KB | `FontLibrary`（字体加载）、`TextBuilder`（链式样式构建）、`measure_text` |
| `OtterOnline.h` | 36KB | `UdpSocket`、`StunClient`、`RendezvousServer`、`Peer`（P2P） |
| `OtterPlatform.h` | 4KB | 平台抽象（`NativeSocket`、`sleep_ms`、`monotonic_ms`、`NetworkSystem`） |
| `OtterWindowBackend.h` | 1KB | `IWindowBackend` 接口 + `RenderBackend` 枚举 |
| `OtterWindowFactory.h` | <1KB | `WindowCreateInfo` + `create_platform_window` |
| `OtterPortableOpenGLRenderer.h` | 8KB | 便携 OpenGL 后端（无 Win32 依赖） |
| `OtterRaylibRenderer.h` | 26KB | Raylib 渲染后端 |

---

## 2. 一帧的生命周期（数据流向）

```
main()
 │
 └─► win.run()  ← 阻塞消息循环
      │
      while (running_):
      ├─ 1. PeekMessage + DispatchMessage（处理所有 Windows 消息）
      │     ├─ WM_MOUSEMOVE → Layer::dispatch_mouse_move
      │     ├─ WM_LBUTTONDOWN → Layer::dispatch_mouse_down
      │     ├─ WM_KEYDOWN → keyboard_key_cb_ → char 输入送到 TextArea
      │     └─ WM_SIZE → renderer().resize
      │
      ├─ 2. anim_manager_.tick(dt)
      │     └─ 遍历所有活跃 Animation → apply_easing → apply_to_layer
      │
      ├─ 3. canvas_.tick(dt)
      │     ├─ 执行所有 on_update 回调（dt 秒）
      │     ├─ 执行所有 on_render 回调（写入 dynamic_chain_）
      │     └─ 递归 tick 所有子层
      │
      └─ 4. render_one_frame()
            ├─ renderer().begin_frame(clear_color)  — 清屏 + 重置变换
            ├─ canvas_.flush(renderer)
            │     └─ 递归：
            │         ├─ push_style / push_transform
            │         ├─ 绘制 CSS 效果（背景/边框/裁切/模糊/羽化/shader）
            │         ├─ static_chain_.execute()  — 静态 PaintChain
            │         ├─ dynamic_chain_.execute() — 动态 PaintChain
            │         ├─ 递归子层 flush
            │         └─ pop_transform / pop_style
            └─ renderer().end_frame() — EndDraw/SwapBuffers
              └─ 如果 D2DERR_RECREATE_TARGET → 自动重建（v1.0.7）
```

---

## 3. 渲染管线

### D2D 路径
```
cmd_move_to/line_to/arc/bezier_to
  → GeometrySink（D2D 路径积累）
  → cmd_fill/cmd_stroke → commit_geometry()
  → FillGeometry / DrawGeometry（一次性提交到渲染目标）
```

**关键点**：D2D 路径是不可变几何体，必须 Build → Close → Draw → Release。
`pending_shadow_` 在每次 fill/stroke 后重置。

### OpenGL 路径
```
cmd_move_to → path_.clear() + push_path_point
cmd_line_to/arc/bezier_to → push_path_point（细分插值）
cmd_fill → normalize_polygon → GL_TRIANGLE_FAN + 逐顶点颜色
cmd_stroke → GL_LINE_LOOP/STRIP + glLineWidth
```

**关键点**：顶点上限 4096（`push_path_point` 中的硬限制）。超出会被静默丢弃。

---

## 4. 事件分发链路

### 鼠标移动（WM_MOUSEMOVE）
```
WndProc(WM_MOUSEMOVE)
  → 构建 MouseEvent（x/y/delta_x/delta_y/wheel_delta/按键状态）
  → canvas_.dispatch_mouse_move(e)
    → 子层逆序遍历（rbegin→rend，视觉上层优先）
    → 转换为局部坐标（le.x -= world_tx_）
    → is_in_hit_area(le) → mouse_enter/leave/hover/move 回调
    → 任一回调返回 true → 停止传播
```

### 键盘输入（WM_KEYDOWN + WM_CHAR）
```
WM_KEYDOWN → keyboard_key_cb_(wp, lp)
  如果 TextArea 有焦点：handle_key(VK_BACK/VK_DELETE/VK_LEFT/...)
  Ctrl+Z/Y → undo/redo

WM_CHAR → keyboard_char_cb_(ch)
  如果 TextArea 有焦点：handle_char('\b' 退格 / '\r' 回车 / 可打印字符)
```

---

## 5. 图层树管理

### creat() 幂等机制
```cpp
Layer* creat(string_view name) {
    auto it = name_map_.find(name);     // 用 heterogeneous lookup（C++20）
    if (it != name_map_.end())
        return it->second;              // 同名已存在 → 直接返回
    auto child = make_unique<Layer>(name);
    child->parent_ = this;
    child->set_anim_manager(anim_manager_);  // 继承动画管理器
    name_map_.emplace(child->name_, raw_ptr);
    children_.push_back(move(child));
    return raw_ptr;
}
```

### find() 递归查找
```cpp
Layer* find(string_view name) const {
    if (name_ == name) return const_cast<Layer*>(this);  // 自己匹配
    if (auto* p = get_child(name)) return p;              // 直接子层
    for (auto& child : children_)
        if (auto* p = child->find(name)) return p;        // 深度递归
    return nullptr;
}
```

### flush() 递归
```
Layer::flush(ctx, parent_layout, parent_wtx, parent_wty):
  1. 解析变换：LayoutPos → 网格坐标计算 或 Manual → transform_ 值
  2. 累加世界偏移：world_tx_ = eff_tx + parent_wtx
  3. 解析边界：LayoutPos → 格子矩形 或 bounds_ → resolved_bounds_
  4. 推入变换 + 样式
  5. 绘制 CSS 效果（背景/边框/裁切/模糊/shader）
  6. static_chain_.execute() + dynamic_chain_.execute()
  7. 递归子层 flush（child_layout 从本层 layout_config_ 获取）
  8. 弹出变换 + 样式
```

---

## 6. OpenGL vs D2D 后端差异

| 特性 | D2D | OpenGL |
|------|-----|--------|
| 文字渲染 | DirectWrite（子像素 ClearType） | GDI DrawTextW → 纹理（灰度） |
| 路径抗锯齿 | Per-Primitive 亚像素 | GL_LINE_SMOOTH（兼容性有限） |
| 渐变 | 原生 Linear/Radial brush | 逐顶点着色 + GL_TRIANGLE_FAN |
| 阴影 | 多层 offset FillGeometry | 多层 offset GL_TRIANGLE_FAN（v1.0.7 后 stroke 也支持） |
| 裁切 | PushAxisAlignedClip / PushLayer | 模板缓冲（rebuild_stencil_clip） |
| 透明度 | PushLayer + 继承栈 | 逐顶点 glColor4f |
| 后处理 | 无（D2D 1.0 限制） | GLSL shader（create_shader + begin_layer_shader） |
| 设备丢失恢复 | D2DERR_RECREATE_TARGET → render_one_frame 自动重建 | SEH 保护 SwapBuffers |
| 缓存策略 | TextFormat 缓存 + Brush LRU + Bitmap 缓存 | Text/Bitmap 纹理 LRU + TextKey hash |

---

## 7. 关键常量和宏

```cpp
// OtterLayer.h
#define OTTER_PI 3.14159265358979323846f    // π（float 精度）

// OtterWindow.h
#define OTTER_WND_CLASS L"OtterWindowClass"  // 窗口类名
#define WS_EX_NOREDIRECTIONBITMAP 0x00000020 // Win8+ D2D 优化

// OtterRenderer.h
#define D2D1_ANTIALIAS_MODE_PER_PRIMITIVE    // 矢量亚像素抗锯齿
D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE          // 文字子像素渲染

// OtterOpenGLRenderer.h
#define GL_CLAMP_TO_EDGE 0x812F              // 纹理寻址模式
#define GL_FRAMEBUFFER 0x8D40               // FBO
path_ 容量限制: 4096 顶点（push_path_point 中硬编码）
```

---

## 8. 常见源码级排查路径

### "文字不显示"
1. `OtterRenderer.h` → `cmd_text`：检查 `dwrite_factory_` 是否为空、`content.empty()`
2. `OtterOpenGLRenderer.h` → `cmd_text`：检查 `draw_text_texture` → `load_text_texture` → GDI 光栅化是否成功
3. 检查 `TextStyle::color.a` 是否为 0（全透明）
4. 检查 `effective_opacity()` 是否为 0

### "图层没有渲染"
1. `OtterLayer.h` → `flush()`：检查 `style_.visible` 是否为 false
2. 检查 `resolved_bounds_` 的 width/height 是否为 0
3. `on_render` 回调是否返回了 `true`（false = 只执行一帧就注销）

### "点击不响应"
1. `OtterLayer.h` → `dispatch_mouse_down`：检查 `style_.visible`、`hit_area_`、`hit_area_mode_`
2. 检查坐标转换：`le.x -= world_tx_` — world_tx_ 是否正确？
3. 检查是否有其他层在 z 序上盖住并消费了事件

### "动画不执行"
1. `OtterLayer.h` → `AnimManager::tick`：检查 `animations_` 是否为空
2. 检查 `set_anim_manager` 是否在构造时被调用（子层继承父层的 anim_manager）
3. `animate_*` 方法需要 `anim_manager_` 非空才生效

### "内存/GPU 泄漏"
1. `OtterRenderer.h` → `release_all`：检查 COM 对象的 Release 顺序
2. `OtterOpenGLRenderer.h` → `release_all`：检查 glDeleteTextures/glDeleteProgram
3. Brush/TextFormat/Bitmap/Text 缓存是否在 `release_render_resources` 中清空
4. `OtterLayer.h` → 子层的 `unique_ptr` 所有权是否正确

### "崩溃在 XXX"
1. 先看调用栈最后停在哪个 `Otter*.h` 的哪一行
2. 看是不是空指针解引用（`render_target_`、`factory_`、`dwrite_factory_`）
3. 看是不是 COM 对象被提前 Release 后又使用
4. 看是不是跨线程访问（OtterOnline worker 线程直接改 UI）
5. 用 `Logger::instance().set_file("debug.log")` + `OTTER_LOG_ERROR` 打日志定位
