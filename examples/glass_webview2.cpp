#include "../OtterWindow.h"
#include "../app/OtterWebView2Node.h"

int main()
{
    if (!Otter::OtterWebView2Node::initialize(GetModuleHandleW(nullptr)))
        return 1;

    Otter::otterwindow win(980, 640, L"Otter Glass WebView2", Otter::RenderBackend::Direct2D);
    win.draggable()
       .rounded_corners()
       .glass_mode(Otter::otterwindow::GlassMode::Acrylic)
       .glass_tint(Otter::Color::from_rgb_hex(0x15202B, 0.38f))
       .keep_glass_active_on_blur()
       .set_clear_color(Otter::Color::transparent())
       .run_during_drag(false)
       .target_fps(60);

    auto* root = win.get["__otter_canvas__"];
    if (!root)
        return 1;

    root->on_render([&win](Otter::PaintChain& c, float)
    {
        const float w = static_cast<float>(win.width());
        const float h = static_cast<float>(win.height());

        c.fill_round_rect(0, 0, w, h, 0, Otter::Color::transparent());
        return true;
    });

    auto* webview = Otter::OtterWebView2Layer::attach(root, win.width(), win.height(), win.native_handle());
    if (!webview)
        return 1;

    webview->load_html(
        LR"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
html, body {
  margin: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: transparent;
  color: #f4fbff;
  font-family: Segoe UI, sans-serif;
}
body {
  box-sizing: border-box;
  padding: 34px;
}
.title { font-size: 34px; font-weight: 700; margin: 0 0 16px 0; }
.sub { font-size: 18px; opacity: 0.92; line-height: 1.5; max-width: 720px; }
.row { display: flex; gap: 16px; margin-top: 26px; }
.card {
  flex: 1;
  min-height: 120px;
  border-radius: 18px;
  background: rgba(255,255,255,0.06);
  border: 1px solid rgba(255,255,255,0.18);
}
</style>
</head>
<body>
  <div class="title">Transparent WebView2</div>
  <div class="sub">This page is rendered on top of the window's acrylic glass with a transparent HTML background.</div>
  <div class="row">
    <div class="card"></div>
    <div class="card"></div>
    <div class="card"></div>
  </div>
</body>
</html>
)HTML",
        L"https://otter.local/glass");

    win.run();
    Otter::OtterWebView2Node::shutdown();
    return 0;
}
