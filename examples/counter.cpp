// ============================================================
//  counter.cpp —— Otter 框架交互入门示例：可点击计数器
//
//  演示能力（最小可用的「按钮 + 状态」范式）：
//    · 用子图层 + hit_area 充当按钮
//    · on_click 修改共享状态，on_mouse_enter / leave 实现悬停高亮
//    · on_render 每帧把状态画成 UI（数值 + 两个按钮）
//    · 状态用 shared_ptr 在多个回调间共享（避免悬垂引用）
//
//  编译（Windows，MinGW g++）：
//    g++ -std=c++20 -I. examples/counter.cpp -o counter.exe \
//        -ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 \
//        -ldwmapi -lopengl32 -lwinhttp -limm32
// ============================================================

#include "../OtterWindow.h"
#include <memory>

namespace
{
    using Otter::Color;

    // 主题色
    const Color kBg      = Color::from_rgb_hex(0x12161F);
    const Color kCard    = Color::from_rgb_hex(0x1E2530);
    const Color kPlus    = Color::from_rgb_hex(0x39D0FF);
    const Color kMinus   = Color::from_rgb_hex(0xFF6B6B);
    const Color kText    = Color::from_rgb_hex(0xEAF3FF);
    const Color kTextDim = Color::from_rgb_hex(0x9FB2C8);

    // 按钮几何（两个按钮 + 居中数值）
    struct Btn { float x, y, w, h; };
    const Btn kMinusBtn{ 80,  220, 120, 90 };
    const Btn kPlusBtn { 300, 220, 120, 90 };

    bool inside(const Btn& b, float px, float py)
    {
        return px >= b.x && px <= b.x + b.w && py >= b.y && py <= b.y + b.h;
    }
}

int main()
{
    Otter::otterwindow win(500, 380, L"Otter 计数器", Otter::RenderBackend::OpenGL);
    win.borderless().draggable().rounded_corners().set_clear_color(kBg);

    // ── 共享状态（在所有回调间安全共享）──────────────────────
    struct State
    {
        int   value     = 0;      // 当前计数
        int   hover      = 0;      // 0=无, 1=减, 2=加（用于悬停高亮）
        float pop        = 0.f;    // 数值变化时的弹跳动画进度
    };
    auto st = std::make_shared<State>();

    auto* root = win.get["__otter_canvas__"];
    if (!root) return 1;

    // ── 弹跳动画衰减 ─────────────────────────────────────────
    root->on_update([st](float dt) -> bool {
        if (st->pop > 0.f)
            st->pop = st->pop - dt * 4.f < 0.f ? 0.f : st->pop - dt * 4.f;
        return true;
    });

    // ── 减号按钮 ─────────────────────────────────────────────
    Otter::Layer* minus = root->creat("btn_minus");
    minus->hit_area(kMinusBtn.x, kMinusBtn.y, kMinusBtn.w, kMinusBtn.h);
    minus->on_click([st](const Otter::MouseEvent&) -> bool {
        --st->value;
        st->pop = 1.f;
        return true;
    });
    minus->on_mouse_enter([st](const Otter::MouseEvent&) -> bool { st->hover = 1; return false; });
    minus->on_mouse_leave([st](const Otter::MouseEvent&) -> bool { st->hover = 0; return false; });

    // ── 加号按钮 ─────────────────────────────────────────────
    Otter::Layer* plus = root->creat("btn_plus");
    plus->hit_area(kPlusBtn.x, kPlusBtn.y, kPlusBtn.w, kPlusBtn.h);
    plus->on_click([st](const Otter::MouseEvent&) -> bool {
        ++st->value;
        st->pop = 1.f;
        return true;
    });
    plus->on_mouse_enter([st](const Otter::MouseEvent&) -> bool { st->hover = 2; return false; });
    plus->on_mouse_leave([st](const Otter::MouseEvent&) -> bool { st->hover = 0; return false; });

    // ── 渲染 ─────────────────────────────────────────────────
    root->on_render([st, &win](Otter::PaintChain& c, float) -> bool {
        const float W = static_cast<float>(win.width());

        // 卡片背景
        c.fill_round_rect(24, 24, W - 48, 332, 18, kCard);

        // 标题
        Otter::TextStyle title;
        title.font_family = L"Segoe UI";
        title.font_size = 20;
        title.weight = Otter::TextStyle::Weight::Bold;
        title.color = kText;
        title.h_align = Otter::TextStyle::HAlign::Center;
        c.text(L"点击 +/- 改变计数", 24, 56, W - 48, 30, title);

        // 当前数值（带轻微弹跳缩放）
        float scale = 1.f + st->pop * 0.25f;
        Otter::TextStyle num;
        num.font_family = L"Segoe UI";
        num.font_size = static_cast<int>(72 * scale);
        num.weight = Otter::TextStyle::Weight::Bold;
        num.color = st->value < 0 ? kMinus : (st->value > 0 ? kPlus : kText);
        num.h_align = Otter::TextStyle::HAlign::Center;
        num.v_align = Otter::TextStyle::VAlign::Middle;
        wchar_t buf[32];
        std::swprintf(buf, 32, L"%d", st->value);
        c.text(buf, 24, 110, W - 48, 90, num);

        // 减号按钮
        Color minus_col = kMinus;
        minus_col.a = (st->hover == 1) ? 1.f : 0.85f;
        c.fill_round_rect(kMinusBtn.x, kMinusBtn.y, kMinusBtn.w, kMinusBtn.h, 14, minus_col);

        // 加号按钮
        Color plus_col = kPlus;
        plus_col.a = (st->hover == 2) ? 1.f : 0.85f;
        c.fill_round_rect(kPlusBtn.x, kPlusBtn.y, kPlusBtn.w, kPlusBtn.h, 14, plus_col);

        // 按钮符号
        Otter::TextStyle sym;
        sym.font_family = L"Segoe UI";
        sym.font_size = 44;
        sym.weight = Otter::TextStyle::Weight::Bold;
        sym.color = Color::from_rgb_hex(0x0E1116);
        sym.h_align = Otter::TextStyle::HAlign::Center;
        sym.v_align = Otter::TextStyle::VAlign::Middle;
        c.text(L"−", kMinusBtn.x, kMinusBtn.y, kMinusBtn.w, kMinusBtn.h, sym);
        c.text(L"+", kPlusBtn.x,  kPlusBtn.y,  kPlusBtn.w,  kPlusBtn.h,  sym);

        return true;
    });

    win.run();
    return 0;
}
