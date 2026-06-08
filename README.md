# OtterSkill

教 AI 使用 **Otter**（header-only C++20 GUI 框架）的技能包（Claude Code Skill）。

Otter 的核心范式：**窗口（otterwindow）→ 图层树（Layer）→ 绘画链（PaintChain）**。
你给图层挂回调（`on_render` / `on_update` / `on_click` …），框架每帧驱动渲染与交互。

## 仓库内容

```
skills/otter-framework/      ← Claude Code 技能（主体）
  SKILL.md                   ← 入口：第一性原理、最小程序、API 速查、陷阱
  references/
    01-window.md             ← 窗口、玻璃材质、性能接口
    02-layer-paint.md        ← 图层树、PaintChain 全量绘制 API、坐标系
    03-input.md              ← 鼠标事件、键盘输入、命中测试
    04-layout-anim.md        ← 网格布局、动画
    05-online.md             ← P2P 网络与线程安全
    06-browser-cef.md        ← 嵌入浏览器（CEF/WebView2）、视频编解码器、GPU 路径陷阱
examples/                    ← 可编译示例（每个文件头部含编译命令）
  counter.cpp                ← 入门：按钮/点击/悬停 + 共享状态
  gradient_showcase.cpp      ← 渐变、贝塞尔曲线、圆弧动画
  text_input.cpp             ← 键盘输入、文本缓冲、闪烁光标
  grid_gallery.cpp           ← 网格布局 Layout + LayoutPos
  dashboard.cpp              ← 综合实战：实时仪表盘
  glass_window.cpp           ← 玻璃/亚克力窗口
  glass_webview2.cpp         ← 嵌入 WebView2
  cef_bilibili.cpp           ← 嵌入 CEF 浏览器（OSR），打开 bilibili
tests/                       ← 测试与一键脚本
  test_core_headless.cpp     ← 核心层无渲染单元测试（跨平台，g++ 可跑）
  run_tests.sh               ← 编译/测试一键脚本（core/headers/examples）
```

## 作为 Claude Code 技能安装

把 `skills/otter-framework/` 复制到你项目的技能目录：

```bash
# 项目级
cp -r skills/otter-framework <your-project>/.claude/skills/
# 或用户级（全局可用）
cp -r skills/otter-framework ~/.claude/skills/
```

之后在 Claude Code 里相关任务会自动触发该技能，或用 `/otter-framework` 调用。

## 快速上手（最小程序）

```cpp
#include "OtterWindow.h"
int main()
{
    Otter::otterwindow win(800, 600, L"Hello", Otter::RenderBackend::OpenGL);
    win.borderless().draggable().rounded_corners()
       .set_clear_color(Otter::Color::from_rgb_hex(0x101418));
    Otter::Layer* root = win.get["__otter_canvas__"];
    root->on_render([&win](Otter::PaintChain& c, float){
        c.fill_round_rect(20, 20, win.width()-40, win.height()-40, 16,
                          Otter::Color::from_rgb_hex(0x1E2530));
        return true;
    });
    win.run();
    return 0;
}
```

更完整内容见 `skills/otter-framework/SKILL.md`。

## 编译要点

- 纯框架（无浏览器）：g++ 15+ 或 MSVC，C++20。
- 跨平台核心测试无需 GPU：`g++ -std=c++20 -I. tests/test_core_headless.cpp -o t && ./t`
- Windows GUI 链接库：`d2d1 dwrite windowscodecs gdi32 user32 ole32 dwmapi opengl32 winhttp imm32`
- 含 CEF 的目标**必须用 MSVC**（libcef 为 MSVC ABI）。详见 `references/06-browser-cef.md`。

> 本仓库仅含技能文档与示例源码，不含 Otter 框架头文件本体与 CEF 运行时；
> 请在实际 Otter 工程中配合使用。
