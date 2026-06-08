// ============================================================
//  cef_bilibili.cpp —— Otter 框架 CEF 浏览器测试程序
//
//  目的：用 CEF（OtterChromeLayer，离屏渲染 OSR）打开 bilibili，
//        实测视频自动播放 / H.265 解码 / 帧率（对应本次 codec+帧率修复）。
//
//  关键点 1：CEF 是多进程模型。可执行文件会被 CEF 重新拉起为渲染/GPU 子进程，
//    所以入口必须先调 execute_subprocess()，若返回 >=0 说明本次是子进程，立即退出，
//    绝不能继续建窗口。这一步漏了会「窗口疯狂自我繁殖 / 崩溃」。
//
//  关键点 2：用 WinMain 而非 main —— CEF 子进程以 GUI 方式拉起，需要 HINSTANCE，
//    且避免子进程弹控制台。
//
//  关键点 3：OtterChromeLayer 是 OSR 层，键鼠由框架的图层事件转发；键盘需用
//    set_keyboard_target 把窗口键盘目标指向浏览器层（否则 B 站搜索框打不了字）。
//
//  编译（Windows / MSVC 推荐，CEF 官方为 MSVC ABI；OtterChromeNode.h 会自动
//    #pragma comment 链接 libcef）：
//    cl /std:c++20 /EHsc /I. /Iapp examples\cef_bilibili.cpp app\OtterChromeNode.cpp ^
//       /link /SUBSYSTEM:WINDOWS libcef.lib libcef_dll_wrapper.lib
//
//  运行前确保 libcef.dll / icudtl.dat / resources.pak / locales\ 等
//  CEF 运行时文件与 exe 同目录（本仓库根目录已具备）。
// ============================================================

#include "../OtterWindow.h"
#include "../app/OtterChromeNode.h"

namespace
{
    // B 站首页；如想直接测视频可换成某个 BV 视频页 URL。
    const wchar_t* kStartUrl = L"https://www.bilibili.com";
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    // ── 子进程引导（必须最先执行）─────────────────────────────
    // 本次进程若是 CEF 子进程，execute_subprocess 处理完返回 >=0，直接退出。
    int sub = Otter::OtterChromeNode::execute_subprocess(instance);
    if (sub >= 0)
        return sub;

    // ── 初始化 CEF（仅主进程到这里）───────────────────────────
    if (!Otter::OtterChromeNode::initialize(instance))
        return 1;

    // 优先用 GPU 共享纹理（OnAcceleratedPaint），视频/动画更省。
    // 注意：若页面不渲染（trace 里无 OnPaint/OnAcceleratedPaint），先改成 false
    // 强制走软件 OnPaint 路径排查是否 GPU 共享纹理路径在本机不产帧。
    Otter::OtterChromeNode::use_shared_texture(false);

    // ── 创建窗口（D2D 后端；浏览器层走 OSR 贴图）──────────────
    Otter::otterwindow win(1280, 800, L"Otter CEF · 哔哩哔哩",
                           Otter::RenderBackend::Direct2D);
    win.draggable()
       .rounded_corners()
       .set_clear_color(Otter::Color::from_rgb_hex(0x14161C))
       .target_fps(0);   // 0 = 不额外限速，交给 VSync / 浏览器帧率

    Otter::Layer* root = win.get["__otter_canvas__"];
    if (!root)
    {
        Otter::OtterChromeNode::shutdown();
        return 1;
    }

    // ── 挂载 CEF 浏览器层（铺满客户区）────────────────────────
    Otter::OtterChromeLayer* browser =
        Otter::OtterChromeLayer::attach(root, win.width(), win.height(), win.native_handle());
    if (!browser)
    {
        Otter::OtterChromeNode::shutdown();
        return 1;
    }

    // OSR 层默认开启透明命中测试；B 站是不透明页面，关掉以保证整层都可点。
    browser->transparent_hit_test(false);
    browser->load_url(kStartUrl);
    browser->focus(true);

    // ── 键盘转发：把窗口键盘目标指向浏览器层 ──────────────────
    // 字符回调 → send_key_char；按键回调 → send_key_down。
    // 这样 B 站搜索框、播放器快捷键（空格暂停、F 全屏等）才能工作。
    win.set_keyboard_target(
        // 字符输入（已经过输入法/Shift 处理的真实字符）
        [browser](WCHAR ch) -> bool {
            browser->send_key_char(static_cast<WPARAM>(ch), 0);
            return true;
        },
        // 物理按键按下（含方向键、空格、回车等）
        [browser](WPARAM key, LPARAM native) -> bool {
            browser->send_key_down(key, native);
            return true;
        },
        // 按键抬起
        [browser](WPARAM key, LPARAM native) -> bool {
            browser->send_key_up(key, native);
            return true;
        });

    // ── 窗口尺寸变化时同步浏览器层尺寸 ────────────────────────
    win.on_ready([browser, &win]() {
        browser->resize(win.width(), win.height());
        browser->focus(true);
    });

    win.run();

    Otter::OtterChromeNode::shutdown();
    return 0;
}
