// ============================================================
//  dashboard.cpp —— Otter 框架综合实战示例
//  实时系统监控仪表盘（System Monitor Dashboard）
//
//  演示能力：
//    · 无边框 + 可拖动 + 圆角 + 玻璃材质窗口
//    · 多卡片布局 + 实时波形折线图（CPU / 内存 模拟数据）
//    · 动画环形进度仪表（缓动插值）
//    · 可点击侧边栏导航（hit_area 命中 + 状态切换）
//    · on_update 驱动数据更新，on_render 驱动绘制
//
//  编译（OpenGL 后端，跨平台路径）：
//    g++ -std=c++20 -I. examples/dashboard.cpp -lopengl32 -lgdi32 ... -o dashboard
//
//  本示例所有坐标均为窗口客户区物理像素。
// ============================================================

#include "../OtterWindow.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace
{
    using Otter::Color;

    // ── 主题配色 ─────────────────────────────────────────────
    const Color kBg        = Color::from_rgb_hex(0x0E1420, 0.86f); // 主背景
    const Color kCard      = Color::from_rgb_hex(0x182234, 0.92f); // 卡片背景
    const Color kCardEdge  = Color::from_rgb_hex(0xFFFFFF, 0.10f); // 卡片描边
    const Color kAccent    = Color::from_rgb_hex(0x39D0FF, 1.0f);  // 主强调色（青）
    const Color kAccent2   = Color::from_rgb_hex(0x64FFDA, 1.0f);  // 次强调色（绿）
    const Color kWarn      = Color::from_rgb_hex(0xFFB84D, 1.0f);  // 警告色（橙）
    const Color kText      = Color::from_rgb_hex(0xEAF3FF, 0.95f); // 主文字
    const Color kTextDim   = Color::from_rgb_hex(0xAEC2DC, 0.65f); // 次文字
    const Color kSidebar   = Color::from_rgb_hex(0x121A28, 0.95f); // 侧边栏背景
    const Color kNavActive = Color::from_rgb_hex(0x39D0FF, 0.18f); // 选中导航底色

    // ── 简易确定性伪随机（不依赖 rand，逐帧平滑波动）──────────
    struct Wobble
    {
        float phase = 0.f;
        float value = 0.5f;
        float speed = 1.f;
        float seed  = 0.f;

        // 用多个不同频率正弦叠加，产生类似真实负载的起伏曲线
        float step(float dt)
        {
            phase += dt * speed;
            float v = 0.5f
                    + 0.22f * std::sin(phase * 1.0f + seed)
                    + 0.14f * std::sin(phase * 2.3f + seed * 1.7f)
                    + 0.08f * std::sin(phase * 5.1f + seed * 0.3f);
            value = std::clamp(v, 0.02f, 0.98f);
            return value;
        }
    };

    // ── 滚动波形缓冲（固定容量的历史采样）─────────────────────
    struct History
    {
        std::deque<float> samples;
        size_t capacity = 80;

        void push(float v)
        {
            samples.push_back(v);
            while (samples.size() > capacity)
                samples.pop_front();
        }
    };

    // ── 缓动：当前值朝目标值逼近（指数平滑）──────────────────
    inline float ease_to(float current, float target, float dt, float rate = 6.f)
    {
        float t = 1.f - std::exp(-rate * dt);
        return current + (target - current) * t;
    }

    // ── 折线波形图绘制 ────────────────────────────────────────
    void draw_waveform(Otter::PaintChain& c,
                       const History& h,
                       float x, float y, float w, float ht,
                       Color line_col)
    {
        if (h.samples.size() < 2) return;

        const size_t n = h.samples.size();
        const float step_x = w / static_cast<float>(h.capacity - 1);

        // 填充区（折线下方半透明）：用闭合路径
        c.move_to(x, y + ht);
        for (size_t i = 0; i < n; ++i)
        {
            float px = x + step_x * static_cast<float>(i);
            float py = y + ht - h.samples[i] * ht;
            c.line_to(px, py);
        }
        c.line_to(x + step_x * static_cast<float>(n - 1), y + ht);
        c.close();
        Color fill_col = line_col;
        fill_col.a = 0.16f;
        c.fill(fill_col);

        // 折线本体
        c.move_to(x, y + ht - h.samples[0] * ht);
        for (size_t i = 1; i < n; ++i)
        {
            float px = x + step_x * static_cast<float>(i);
            float py = y + ht - h.samples[i] * ht;
            c.line_to(px, py);
        }
        c.stroke(line_col, 2.0f);
    }

    // ── 环形进度仪表（0~1）────────────────────────────────────
    void draw_gauge(Otter::PaintChain& c,
                    float cx, float cy, float radius,
                    float value, Color col, const std::wstring& label)
    {
        const float PI = 3.14159265358979f;
        const float start = PI * 0.75f;          // 起始角（左下）
        const float sweep = PI * 1.5f;           // 总扫过 270°

        // 轨道（底环）
        c.move_to(cx + std::cos(start) * radius, cy + std::sin(start) * radius);
        c.arc(cx, cy, radius, start, start + sweep);
        c.stroke(Color::from_rgb_hex(0xFFFFFF, 0.10f), 9.f);

        // 进度弧
        float end = start + sweep * std::clamp(value, 0.f, 1.f);
        c.move_to(cx + std::cos(start) * radius, cy + std::sin(start) * radius);
        c.arc(cx, cy, radius, start, end);
        c.stroke(col, 9.f);

        // 中心百分比文字
        Otter::TextStyle big;
        big.font_family = L"Segoe UI";
        big.font_size = 30;
        big.weight = Otter::TextStyle::Weight::Bold;
        big.color = kText;
        big.h_align = Otter::TextStyle::HAlign::Center;
        big.v_align = Otter::TextStyle::VAlign::Middle;
        wchar_t buf[16];
        std::swprintf(buf, 16, L"%d%%", static_cast<int>(value * 100.f + 0.5f));
        c.text(buf, cx - 60, cy - 24, 120, 40, big);

        Otter::TextStyle sub;
        sub.font_family = L"Segoe UI";
        sub.font_size = 13;
        sub.color = kTextDim;
        sub.h_align = Otter::TextStyle::HAlign::Center;
        c.text(label, cx - 60, cy + 16, 120, 20, sub);
    }
}

int main()
{
    // 1280x760 无边框玻璃窗口，OpenGL 后端（跨平台路径，默认 VSync 限速）
    Otter::otterwindow win(1280, 760, L"Otter 系统监控仪表盘",
                           Otter::RenderBackend::OpenGL);

    win.borderless()
       .draggable()
       .rounded_corners()
       .render_mode(Otter::RenderMode::GPU)  // 显式 GPU（v1.0.2 新接口）
       .vsync(true)                          // VSync 限速（v1.0.2 新接口）
       .set_clear_color(kBg);

    // ── 应用状态 ─────────────────────────────────────────────
    struct State
    {
        Wobble  cpu_w{ .speed = 1.4f, .seed = 0.3f };
        Wobble  mem_w{ .speed = 0.7f, .seed = 2.1f };
        Wobble  net_w{ .speed = 2.6f, .seed = 4.4f };
        History cpu_hist, mem_hist, net_hist;

        float cpu_disp = 0.f;   // 平滑显示值
        float mem_disp = 0.f;
        float net_disp = 0.f;
        float gauge_disp = 0.f; // 仪表平滑值

        float sample_acc = 0.f; // 采样计时器
        int   nav_index = 0;    // 当前选中导航项
        float uptime = 0.f;     // 运行时长（秒）
    };
    auto st = std::make_shared<State>();

    const std::array<std::wstring, 4> nav_items = {
        L"概览", L"处理器", L"内存", L"网络"
    };

    auto* root = win.get["__otter_canvas__"];
    if (!root) return 1;

    // ── 数据更新（每帧）──────────────────────────────────────
    root->on_update([st](float dt) -> bool
    {
        st->uptime += dt;
        st->sample_acc += dt;

        // 实时目标值
        float cpu_t = st->cpu_w.step(dt);
        float mem_t = st->mem_w.step(dt);
        float net_t = st->net_w.step(dt);

        // 平滑显示
        st->cpu_disp = ease_to(st->cpu_disp, cpu_t, dt);
        st->mem_disp = ease_to(st->mem_disp, mem_t, dt);
        st->net_disp = ease_to(st->net_disp, net_t, dt);
        st->gauge_disp = ease_to(st->gauge_disp, cpu_t, dt, 4.f);

        // 每 ~80ms 采一个波形点
        if (st->sample_acc >= 0.08f)
        {
            st->sample_acc = 0.f;
            st->cpu_hist.push(st->cpu_disp);
            st->mem_hist.push(st->mem_disp);
            st->net_hist.push(st->net_disp);
        }
        return true;
    });

    // ── 侧边栏导航按钮（可点击子图层）────────────────────────
    // 每个按钮一个子图层，用 hit_area 做命中，点击切换 nav_index
    for (int i = 0; i < static_cast<int>(nav_items.size()); ++i)
    {
        float by = 120.f + i * 56.f;
        Otter::Layer* btn = root->creat(std::string("nav_") + std::to_string(i));
        btn->hit_area(16.f, by, 188.f, 46.f);
        btn->on_click([st, i](const Otter::MouseEvent&) -> bool
        {
            st->nav_index = i;
            return true;  // 消费事件
        });
    }

    // ── 主渲染 ───────────────────────────────────────────────
    root->on_render([st, &win, nav_items](Otter::PaintChain& c, float) -> bool
    {
        const float W = static_cast<float>(win.width());
        const float H = static_cast<float>(win.height());

        // 外层圆角背景板
        c.fill_round_rect(0, 0, W, H, 16, kBg);

        // ── 侧边栏 ───────────────────────────────────────────
        const float SB = 220.f;
        c.fill_round_rect(0, 0, SB, H, 16, kSidebar);
        c.fill_rect(SB - 16, 0, 16, H, kSidebar);  // 补右侧直角

        Otter::TextStyle logo;
        logo.font_family = L"Segoe UI";
        logo.font_size = 22;
        logo.weight = Otter::TextStyle::Weight::Bold;
        logo.color = kAccent;
        c.text(L"🦦 Otter Monitor", 24, 36, logo);

        // 导航项
        for (int i = 0; i < static_cast<int>(nav_items.size()); ++i)
        {
            float by = 120.f + i * 56.f;
            bool active = (st->nav_index == i);
            if (active)
            {
                c.fill_round_rect(16, by, 188, 46, 10, kNavActive);
                c.fill_round_rect(16, by, 4, 46, 2, kAccent); // 左侧高亮条
            }
            Otter::TextStyle nav;
            nav.font_family = L"Segoe UI";
            nav.font_size = 16;
            nav.weight = active ? Otter::TextStyle::Weight::Bold
                                : Otter::TextStyle::Weight::Regular;
            nav.color = active ? kText : kTextDim;
            nav.v_align = Otter::TextStyle::VAlign::Middle;
            c.text(nav_items[i], 40, by, 150, 46, nav);
        }

        // 运行时长（侧边栏底部）
        Otter::TextStyle up;
        up.font_family = L"Consolas";
        up.font_size = 13;
        up.color = kTextDim;
        wchar_t ubuf[48];
        int total = static_cast<int>(st->uptime);
        std::swprintf(ubuf, 48, L"运行 %02d:%02d:%02d",
                      total / 3600, (total / 60) % 60, total % 60);
        c.text(ubuf, 24, H - 44, up);

        // ── 顶栏标题 ─────────────────────────────────────────
        const float CX = SB + 32.f;  // 内容区左边距
        Otter::TextStyle h1;
        h1.font_family = L"Segoe UI";
        h1.font_size = 28;
        h1.weight = Otter::TextStyle::Weight::Bold;
        h1.color = kText;
        c.text(nav_items[st->nav_index] + L" · 实时监控", CX, 32, h1);

        Otter::TextStyle h2;
        h2.font_family = L"Segoe UI";
        h2.font_size = 14;
        h2.color = kTextDim;
        c.text(L"数据为演示用模拟值 · 拖动窗口移动 · ESC 退出", CX, 72, h2);

        // 关闭按钮（右上角）
        c.fill_circle(W - 36, 40, 13, Color::from_rgb_hex(0xFF5F57, 0.9f));

        // ── 顶部三张统计卡片 ─────────────────────────────────
        struct CardDef { const wchar_t* name; float val; Color col; const wchar_t* unit; };
        CardDef cards[3] = {
            { L"CPU 使用率", st->cpu_disp, kAccent,  L"%" },
            { L"内存占用",   st->mem_disp, kAccent2, L"%" },
            { L"网络吞吐",   st->net_disp, kWarn,    L" MB/s" },
        };

        const float card_y = 104.f;
        const float card_h = 120.f;
        const float gap = 20.f;
        const float card_w = (W - CX - 32.f - gap * 2.f) / 3.f;

        for (int i = 0; i < 3; ++i)
        {
            float cx = CX + i * (card_w + gap);
            c.fill_round_rect(cx, card_y, card_w, card_h, 14, kCard);
            c.stroke_round_rect(cx, card_y, card_w, card_h, 14, kCardEdge, 1.f);

            Otter::TextStyle lbl;
            lbl.font_family = L"Segoe UI";
            lbl.font_size = 14;
            lbl.color = kTextDim;
            c.text(cards[i].name, cx + 20, card_y + 18, lbl);

            // 数值（网络卡片显示 *100 当作 MB/s 模拟）
            Otter::TextStyle num;
            num.font_family = L"Segoe UI";
            num.font_size = 38;
            num.weight = Otter::TextStyle::Weight::Bold;
            num.color = cards[i].col;
            wchar_t nbuf[32];
            if (i == 2)
                std::swprintf(nbuf, 32, L"%.1f%ls", cards[i].val * 100.f, cards[i].unit);
            else
                std::swprintf(nbuf, 32, L"%d%ls", static_cast<int>(cards[i].val * 100.f + 0.5f), cards[i].unit);
            c.text(nbuf, cx + 20, card_y + 46, num);

            // 卡片底部迷你进度条
            c.fill_round_rect(cx + 20, card_y + card_h - 22, card_w - 40, 6, 3,
                              Color::from_rgb_hex(0xFFFFFF, 0.08f));
            c.fill_round_rect(cx + 20, card_y + card_h - 22,
                              (card_w - 40) * std::clamp(cards[i].val, 0.f, 1.f), 6, 3,
                              cards[i].col);
        }

        // ── 大波形图卡片 ─────────────────────────────────────
        const float chart_y = card_y + card_h + gap;
        const float chart_h = 280.f;
        const float chart_w = (W - CX - 32.f) * 0.62f;
        c.fill_round_rect(CX, chart_y, chart_w, chart_h, 14, kCard);
        c.stroke_round_rect(CX, chart_y, chart_w, chart_h, 14, kCardEdge, 1.f);

        Otter::TextStyle ct;
        ct.font_family = L"Segoe UI";
        ct.font_size = 16;
        ct.weight = Otter::TextStyle::Weight::Bold;
        ct.color = kText;
        c.text(L"负载趋势（近 80 次采样）", CX + 22, chart_y + 18, ct);

        // 图例
        struct Leg { const wchar_t* n; Color col; };
        Leg legs[3] = { { L"CPU", kAccent }, { L"内存", kAccent2 }, { L"网络", kWarn } };
        for (int i = 0; i < 3; ++i)
        {
            float lx = CX + chart_w - 220 + i * 72.f;
            c.fill_circle(lx, chart_y + 26, 5, legs[i].col);
            Otter::TextStyle ls;
            ls.font_family = L"Segoe UI"; ls.font_size = 13; ls.color = kTextDim;
            c.text(legs[i].n, lx + 12, chart_y + 17, ls);
        }

        // 网格线
        const float plot_x = CX + 22;
        const float plot_y = chart_y + 54;
        const float plot_w = chart_w - 44;
        const float plot_h = chart_h - 80;
        for (int g = 0; g <= 4; ++g)
        {
            float gy = plot_y + plot_h * g / 4.f;
            c.stroke_line(plot_x, gy, plot_x + plot_w, gy,
                          1.f, Color::from_rgb_hex(0xFFFFFF, 0.05f));
        }

        draw_waveform(c, st->mem_hist, plot_x, plot_y, plot_w, plot_h, kAccent2);
        draw_waveform(c, st->net_hist, plot_x, plot_y, plot_w, plot_h, kWarn);
        draw_waveform(c, st->cpu_hist, plot_x, plot_y, plot_w, plot_h, kAccent);

        // ── 右侧环形仪表卡片 ─────────────────────────────────
        const float gx = CX + chart_w + gap;
        const float gw = W - gx - 32.f;
        c.fill_round_rect(gx, chart_y, gw, chart_h, 14, kCard);
        c.stroke_round_rect(gx, chart_y, gw, chart_h, 14, kCardEdge, 1.f);

        Otter::TextStyle gt;
        gt.font_family = L"Segoe UI"; gt.font_size = 16;
        gt.weight = Otter::TextStyle::Weight::Bold; gt.color = kText;
        c.text(L"核心负载", gx + 22, chart_y + 18, gt);

        Color gauge_col = st->gauge_disp > 0.8f ? Color::from_rgb_hex(0xFF5F57)
                        : st->gauge_disp > 0.5f ? kWarn : kAccent;
        draw_gauge(c, gx + gw / 2.f, chart_y + chart_h / 2.f + 6.f, 78.f,
                   st->gauge_disp, gauge_col, L"CPU 综合");

        return true;
    });

    win.run();
    return 0;
}
