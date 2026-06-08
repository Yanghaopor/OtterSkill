#include "../OtterWindow.h"

int main()
{
    Otter::otterwindow win(860, 520, L"Otter Glass Window", Otter::RenderBackend::OpenGL);

    win.borderless()
       .draggable()
       .rounded_corners()
       .glass_mode(Otter::otterwindow::GlassMode::Acrylic)
       .glass_tint(Otter::Color::from_rgb_hex(0x1D2430, 0.42f))
       .target_fps(60);

    auto* root = win.get["__otter_canvas__"];
    if (!root)
        return 1;

    root->on_render([&win](Otter::PaintChain& c, float)
    {
        const float w = static_cast<float>(win.width());
        const float h = static_cast<float>(win.height());

        c.fill_round_rect(28, 28, w - 56, h - 56, 18,
            Otter::Color::from_rgb_hex(0x101722, 0.52f));
        c.stroke_round_rect(28, 28, w - 56, h - 56, 18,
            Otter::Color::from_rgb_hex(0xFFFFFF, 0.28f), 1.2f);

        Otter::TextStyle title;
        title.font_family = L"Segoe UI";
        title.font_size = 36;
        title.weight = Otter::TextStyle::Weight::Bold;
        title.color = Otter::Color::white();
        c.text(L"Otter Glass", 56, 58, title);

        Otter::TextStyle body;
        body.font_family = L"Segoe UI";
        body.font_size = 18;
        body.color = Otter::Color::from_rgb_hex(0xEAF3FF, 0.88f);
        c.text(L"Move the window over different desktop backgrounds to test acrylic blur.", 58, 112, body);

        c.fill_round_rect(58, 170, 250, 84, 12,
            Otter::Color::from_rgb_hex(0xFFFFFF, 0.14f));
        c.stroke_round_rect(58, 170, 250, 84, 12,
            Otter::Color::from_rgb_hex(0xFFFFFF, 0.28f), 1);

        c.fill_round_rect(336, 170, 250, 84, 12,
            Otter::Color::from_rgb_hex(0x39D0FF, 0.18f));
        c.stroke_round_rect(336, 170, 250, 84, 12,
            Otter::Color::from_rgb_hex(0x39D0FF, 0.40f), 1);

        c.fill_circle(w - 150, h - 120, 72, Otter::Color::from_rgb_hex(0xFFB84D, 0.26f));
        c.fill_circle(w - 98, h - 166, 44, Otter::Color::from_rgb_hex(0x64FFDA, 0.24f));
        return true;
    });

    win.run();
    return 0;
}
