// ============================================================
//  gradient_showcase.cpp —— Otter 框架绘制能力展示
//
//  演示能力（纯绘制，无交互）：
//    · 线性渐变 / 径向渐变填充（LinearGradient / RadialGradient）
//    · 贝塞尔曲线路径（bezier_to）绘制平滑波浪
//    · 圆弧（arc）绘制旋转的进度环
//    · on_update 累积时间驱动动画相位
//
//  编译（Windows，MinGW g++）：
//    g++ -std=c++20 -I. examples/gradient_showcase.cpp -o gradient_showcase.exe \
//        -ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 \
//        -ldwmapi -lopengl32 -lwinhttp -limm32
// ============================================================

#include "../OtterWindow.h"
#include <cmath>
#include <memory>

namespace
{
    using Otter::Color;
    const float kPI = 3.14159265358979f;
}

int main()
{
    Otter::otterwindow win(720, 520, L"Otter 绘制展示", Otter::RenderBackend::OpenGL);
    win.borderless().draggable().rounded_corners()
       .set_clear_color(Color::from_rgb_hex(0x0B0E14));

    auto* root = win.get["__otter_canvas__"];
    if (!root) return 1;

    // 动画相位（随时间累积）
    auto phase = std::make_shared<float>(0.f);
    root->on_update([phase](float dt) -> bool { *phase += dt; return true; });

    root->on_render([phase, &win](Otter::PaintChain& c, float) -> bool {
        const float W = static_cast<float>(win.width());
        const float H = static_cast<float>(win.height());
        const float t = *phase;

        // ── 1. 全屏线性渐变背景板 ────────────────────────────
        Otter::LinearGradient bg(0, 0, W, H,
                                 Color::from_rgb_hex(0x141A2A),
                                 Color::from_rgb_hex(0x0B0E14));
        c.fill_round_rect(0, 0, W, H, 16, bg);

        // ── 2. 渐变标题卡片（左上）──────────────────────────
        Otter::LinearGradient card(40, 40, 40, 160,
                                   Color::from_rgb_hex(0x39D0FF, 0.30f),
                                   Color::from_rgb_hex(0x9B5CFF, 0.30f));
        c.fill_round_rect(40, 40, W - 80, 120, 16, card);
        c.stroke_round_rect(40, 40, W - 80, 120, 16,
                            Color::from_rgb_hex(0xFFFFFF, 0.18f), 1.f);

        Otter::TextStyle title;
        title.font_family = L"Segoe UI";
        title.font_size = 34;
        title.weight = Otter::TextStyle::Weight::Bold;
        title.color = Color::white();
        c.text(L"渐变 · 曲线 · 圆弧", 68, 64, title);

        Otter::TextStyle sub;
        sub.font_family = L"Segoe UI";
        sub.font_size = 15;
        sub.color = Color::from_rgb_hex(0xC8D6EC, 0.85f);
        c.text(L"LinearGradient / bezier_to / arc 实时动画", 68, 112, sub);

        // ── 3. 贝塞尔波浪（中部，随相位流动）─────────────────
        const float wave_y = 280.f;
        const float amp = 26.f;
        c.move_to(40, wave_y);
        for (int seg = 0; seg < 8; ++seg)
        {
            float x0 = 40 + (W - 80) * seg / 8.f;
            float x1 = 40 + (W - 80) * (seg + 1) / 8.f;
            float mid = (x0 + x1) * 0.5f;
            float dir = (seg % 2 == 0) ? 1.f : -1.f;
            float off = std::sin(t * 2.f + seg) * amp * dir;
            // 三次贝塞尔：两个控制点造出平滑起伏
            c.bezier_to(mid, wave_y + off, mid, wave_y + off, x1, wave_y);
        }
        c.stroke(Color::from_rgb_hex(0x64FFDA), 3.f);

        // ── 4. 径向渐变光球（随相位左右移动）─────────────────
        float orb_x = W * 0.5f + std::sin(t) * (W * 0.30f);
        float orb_y = 400.f;
        Otter::RadialGradient orb;
        orb.cx = orb_x; orb.cy = orb_y;
        orb.rx = 60.f;  orb.ry = 60.f;
        orb.stops.emplace_back(0.f, Color::from_rgb_hex(0xFFB84D, 0.95f));
        orb.stops.emplace_back(1.f, Color::from_rgb_hex(0xFFB84D, 0.0f));
        c.fill_circle(orb_x, orb_y, 60.f, orb);

        // ── 5. 旋转进度环（右下角，arc）──────────────────────
        float cx = W - 90.f, cy = 90.f, r = 30.f;
        float start = std::fmod(t * 1.5f, kPI * 2.f);
        // 底环
        c.move_to(cx + r, cy);
        c.arc(cx, cy, r, 0.f, kPI * 2.f);
        c.stroke(Color::from_rgb_hex(0xFFFFFF, 0.12f), 6.f);
        // 旋转弧段（270°）
        c.move_to(cx + std::cos(start) * r, cy + std::sin(start) * r);
        c.arc(cx, cy, r, start, start + kPI * 1.5f);
        c.stroke(Color::from_rgb_hex(0x39D0FF), 6.f);

        return true;
    });

    win.run();
    return 0;
}
