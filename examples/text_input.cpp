// ============================================================
//  text_input.cpp —— Otter 框架键盘输入示例
//
//  演示能力：
//    · 用 set_keyboard_target 接收字符输入与按键（跨平台 Key 重载）
//    · 维护一个文本缓冲，Backspace 删除、Enter 提交、Esc 退出
//    · 闪烁光标（on_update 累积时间）
//    · 点击输入框「聚焦」/ 点击空白「失焦」
//
//  编译（Windows，MinGW g++）：
//    g++ -std=c++20 -I. examples/text_input.cpp -o text_input.exe \
//        -ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 \
//        -ldwmapi -lopengl32 -lwinhttp -limm32
// ============================================================

#include "../OtterWindow.h"
#include <memory>
#include <string>
#include <vector>

namespace
{
    using Otter::Color;
    const Color kBg     = Color::from_rgb_hex(0x10141C);
    const Color kField  = Color::from_rgb_hex(0x1C2430);
    const Color kFocus  = Color::from_rgb_hex(0x39D0FF);
    const Color kText   = Color::from_rgb_hex(0xEAF3FF);
    const Color kDim    = Color::from_rgb_hex(0x8DA0B8);

    // 输入框几何
    const float kFieldX = 40, kFieldY = 120, kFieldW = 540, kFieldH = 64;
}

int main()
{
    Otter::otterwindow win(620, 360, L"Otter 文本输入", Otter::RenderBackend::OpenGL);
    win.borderless().draggable().rounded_corners().set_clear_color(kBg);

    auto* root = win.get["__otter_canvas__"];
    if (!root) return 1;

    // ── 共享状态 ─────────────────────────────────────────────
    struct State
    {
        std::wstring text;                 // 当前输入内容
        std::vector<std::wstring> history; // 已提交的内容
        bool  focused   = false;           // 是否聚焦输入框
        float blink     = 0.f;             // 光标闪烁计时
    };
    auto st = std::make_shared<State>();

    // ── 聚焦：把键盘目标指向输入处理逻辑 ─────────────────────
    auto focus_input = [st, &win]() {
        st->focused = true;
        win.set_keyboard_target(
            // 字符输入（可见字符）：追加到缓冲
            [st](wchar_t ch) -> bool {
                if (ch >= 32 && ch != 127) { st->text.push_back(ch); return true; }
                return false;
            },
            // 功能键：Backspace / Enter / Esc
            [st, &win](Otter::Key key) -> bool {
                if (key == Otter::Key::Backspace) {
                    if (!st->text.empty()) st->text.pop_back();
                    return true;
                }
                if (key == Otter::Key::Enter) {
                    if (!st->text.empty()) { st->history.push_back(st->text); st->text.clear(); }
                    return true;
                }
                if (key == Otter::Key::Escape) { win.close(); return true; }
                return false;
            },
            // 失焦回调：状态复位
            [st]() { st->focused = false; });
    };

    // ── 输入框可点击区域（点击聚焦）──────────────────────────
    Otter::Layer* field = root->creat("field");
    field->hit_area(kFieldX, kFieldY, kFieldW, kFieldH);
    field->on_click([focus_input](const Otter::MouseEvent&) -> bool {
        focus_input();
        return true;
    });

    // ── 光标闪烁 ─────────────────────────────────────────────
    root->on_update([st](float dt) -> bool { st->blink += dt; return true; });

    // ── 渲染 ─────────────────────────────────────────────────
    root->on_render([st, &win](Otter::PaintChain& c, float) -> bool {
        const float W = static_cast<float>(win.width());

        c.fill_round_rect(20, 20, W - 40, 320, 16, Color::from_rgb_hex(0x161C26));

        Otter::TextStyle title;
        title.font_family = L"Segoe UI";
        title.font_size = 22;
        title.weight = Otter::TextStyle::Weight::Bold;
        title.color = kText;
        c.text(L"点击输入框后键入 · Enter 提交 · Esc 退出", 40, 52, title);

        // 输入框（聚焦时高亮描边）
        c.fill_round_rect(kFieldX, kFieldY, kFieldW, kFieldH, 12, kField);
        Color edge = st->focused ? kFocus : Color::from_rgb_hex(0xFFFFFF, 0.12f);
        c.stroke_round_rect(kFieldX, kFieldY, kFieldW, kFieldH, 12, edge, st->focused ? 2.f : 1.f);

        // 文本 / 占位符
        Otter::TextStyle body;
        body.font_family = L"Consolas";
        body.font_size = 22;
        body.v_align = Otter::TextStyle::VAlign::Middle;
        if (st->text.empty() && !st->focused) {
            body.color = kDim;
            c.text(L"在此输入…", kFieldX + 18, kFieldY, kFieldW - 36, kFieldH, body);
        } else {
            body.color = kText;
            // 聚焦时按节拍追加光标符号
            std::wstring shown = st->text;
            bool caret_on = (static_cast<int>(st->blink * 2.f) % 2) == 0;
            if (st->focused && caret_on) shown += L"|";
            c.text(shown, kFieldX + 18, kFieldY, kFieldW - 36, kFieldH, body);
        }

        // 历史记录（最近 3 条）
        Otter::TextStyle hist;
        hist.font_family = L"Segoe UI";
        hist.font_size = 16;
        hist.color = kDim;
        int n = static_cast<int>(st->history.size());
        for (int i = 0; i < 3 && i < n; ++i) {
            const std::wstring& line = st->history[n - 1 - i];
            c.text(L"› " + line, kFieldX, kFieldY + kFieldH + 24 + i * 28, hist);
        }

        return true;
    });

    win.run();
    return 0;
}
