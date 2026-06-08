// ============================================================
//  grid_gallery.cpp —— Otter 框架网格布局示例
//
//  演示能力：
//    · 窗口级网格布局 win.Layout(cols, rows, ...)
//    · 子图层用 LayoutPos(col, row) 自动定位到网格单元
//    · 每个子层独立 on_render，在自身 bounds 内绘制
//    · 点击单元格放大显示其编号（局部状态 + 共享选中态）
//
//  关键点：调用了 LayoutPos 的子层会由父层网格自动计算 bounds，
//  子层 on_render 的绘制坐标是「相对自身 bounds 的局部坐标」。
//
//  编译（Windows，MinGW g++）：
//    g++ -std=c++20 -I. examples/grid_gallery.cpp -o grid_gallery.exe \
//        -ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 \
//        -ldwmapi -lopengl32 -lwinhttp -limm32
// ============================================================

#include "../OtterWindow.h"
#include <memory>
#include <string>

namespace
{
    using Otter::Color;

    // 6 种单元格配色（循环使用）
    const Color kCellColors[6] = {
        Color::from_rgb_hex(0x39D0FF), Color::from_rgb_hex(0x64FFDA),
        Color::from_rgb_hex(0xFFB84D), Color::from_rgb_hex(0xFF6B9D),
        Color::from_rgb_hex(0x9B5CFF), Color::from_rgb_hex(0x5CE1FF),
    };
}

int main()
{
    Otter::otterwindow win(640, 480, L"Otter 网格画廊", Otter::RenderBackend::OpenGL);
    win.borderless().draggable().rounded_corners()
       .set_clear_color(Color::from_rgb_hex(0x0E1118));

    // 3x3 网格：边距 28，单元间距 16
    win.Layout(/*cols=*/3, /*rows=*/3,
               /*pad_x=*/28, /*pad_y=*/28,
               /*gap_x=*/16, /*gap_y=*/16);

    auto* root = win.get["__otter_canvas__"];
    if (!root) return 1;

    // 共享选中态（被点击的单元格索引，-1 表示无）
    auto selected = std::make_shared<int>(-1);

    // ── 创建 9 个网格单元 ────────────────────────────────────
    for (int i = 0; i < 9; ++i)
    {
        int col = i % 3;
        int row = i / 3;

        Otter::Layer* cell = root->creat("cell_" + std::to_string(i));
        cell->LayoutPos(col, row);              // 自动定位到网格 (col,row)
        cell->auto_hit_area(true);              // 命中区域 = 自身 bounds

        cell->on_click([selected, i](const Otter::MouseEvent&) -> bool {
            *selected = (*selected == i) ? -1 : i;  // 再次点击取消选中
            return true;
        });

        // 每个单元独立绘制：坐标为相对自身 bounds 的局部坐标
        cell->on_render([selected, i, cell](Otter::PaintChain& c, float) -> bool {
            // 取自身解析后的尺寸（局部绘制以 (0,0) 为左上角）
            const Otter::Rect* b = cell->resolved_bounds();
            if (!b) return true;
            float w = b->width, h = b->height;
            if (w <= 0 || h <= 0) return true;

            Color col = kCellColors[i % 6];
            bool sel = (*selected == i);
            col.a = sel ? 1.0f : 0.78f;

            c.fill_round_rect(0, 0, w, h, 14, col);
            if (sel)
                c.stroke_round_rect(0, 0, w, h, 14, Color::white(), 2.5f);

            // 单元编号
            Otter::TextStyle num;
            num.font_family = L"Segoe UI";
            num.font_size = sel ? 48 : 30;
            num.weight = Otter::TextStyle::Weight::Bold;
            num.color = Color::from_rgb_hex(0x0B0E14);
            num.h_align = Otter::TextStyle::HAlign::Center;
            num.v_align = Otter::TextStyle::VAlign::Middle;
            c.text(std::to_wstring(i + 1), 0, 0, w, h, num);
            return true;
        });
    }

    win.run();
    return 0;
}
