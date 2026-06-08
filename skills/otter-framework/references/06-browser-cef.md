# 06 — 嵌入浏览器（CEF / WebView2）

框架可在图层里嵌入完整浏览器。两套后端：
- **CEF**（`app/OtterChromeNode.*`）：离屏渲染（OSR），贴图进图层，能与图层树/透明混合。
- **WebView2**（`app/OtterWebView2Node.*`）：原生子窗口合成，最流畅，但是独立窗口层。

## CEF 测试程序骨架（OSR）

完整可运行示例见 `examples/cef_bilibili.cpp`。要点：

```cpp
#include "../OtterWindow.h"
#include "../app/OtterChromeNode.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int)
{
    // ① 多进程引导（必须最先）：本进程若是 CEF 子进程，处理完即退出。
    int sub = Otter::OtterChromeNode::execute_subprocess(inst);
    if (sub >= 0) return sub;

    // ② 初始化 CEF（仅主进程到此）
    if (!Otter::OtterChromeNode::initialize(inst)) return 1;
    Otter::OtterChromeNode::use_shared_texture(false);  // 见下方「GPU 路径」警告

    Otter::otterwindow win(1280, 800, L"CEF", Otter::RenderBackend::Direct2D);
    win.draggable().rounded_corners();
    Otter::Layer* root = win.get["__otter_canvas__"];

    // ③ 挂浏览器层，铺满客户区
    auto* browser = Otter::OtterChromeLayer::attach(root, win.width(), win.height(), win.native_handle());
    browser->transparent_hit_test(false);  // 不透明页面：整层可点
    browser->load_url(L"https://www.bilibili.com");
    browser->focus(true);

    // ④ 键盘转发：窗口键盘目标 → 浏览器层（否则网页输入框打不了字）
    win.set_keyboard_target(
        [browser](WCHAR ch){ browser->send_key_char((WPARAM)ch, 0); return true; },
        [browser](WPARAM k, LPARAM n){ browser->send_key_down(k, n); return true; },
        [browser](WPARAM k, LPARAM n){ browser->send_key_up(k, n); return true; });

    win.run();
    Otter::OtterChromeNode::shutdown();
    return 0;
}
```

## 致命陷阱（本节是血泪经验，务必读）

### 1. 必须 `execute_subprocess` 在最前
CEF 是多进程模型，exe 会被自己重新拉起为渲染/GPU 子进程。入口第一件事就是
`execute_subprocess()`，返回 `>=0` 立即退出。漏了会窗口自我繁殖 / 崩溃。

### 2. 必须用 `wWinMain` + `/SUBSYSTEM:WINDOWS`
子进程以 GUI 方式拉起，需要 `HINSTANCE`，且避免子进程弹控制台。

### 3. 必须用 MSVC（cl）编译，不能用 g++
CEF 的 `libcef.lib` / `libcef_dll_wrapper.lib` 是 VS2022（MSVC ABI）。MinGW g++
ABI 不兼容、链接不了。用 `VsDevCmd.bat + cl`，参考 `build_cef_bilibili.py`。
（纯框架部分仍可 g++；只有带 CEF 的目标必须 MSVC。）

### 4. 运行时文件必须配套同目录
exe 同目录要有 `libcef.dll`、`icudtl.dat`、`resources.pak`、`chrome_*.pak`、
`v8_context_snapshot.bin`、`locales/`、`libEGL.dll`、`libGLESv2.dll` 等，且
**版本必须与 libcef.lib 完全一致**，混版本会崩。

### 5. ⚠️ 视频「不支持 HTML5 播放器」= libcef 没编入专有编解码器
B 站等站点的 H5 播放器走 **H.264/MP4 + MSE**。若 `libcef.dll` 不含专有编解码器，
`video.canPlayType('video/mp4; codecs="avc1..."')` 返回空、`MediaSource.isTypeSupported(...)`
为 false，页面直接报「不支持 HTML5 播放器」。

**这无法靠改命令行开关修复**——开关只能开启「已编入二进制」的功能。
- 官方 cef-builds.spotifycdn.com 的 **Standard Distribution 默认带 H.264/AAC**
  （`proprietary_codecs=true ffmpeg_branding=Chrome`）。
- 诊断方法：在 `OnLoadEnd` 注入探测 JS，看 console（已落到 `otter_cef_trace.log`）：
  ```js
  var v=document.createElement('video');
  console.log('h264=' + v.canPlayType('video/mp4; codecs="avc1.42E01E, mp4a.40.2"') +
              ', mse_h264=' + (!!window.MediaSource &&
                MediaSource.isTypeSupported('video/mp4; codecs="avc1.42E01E, mp4a.40.2"')));
  ```
  `h264=probably, mse_h264=true` 才算 OK；若为空/false → 换官方带编解码器的 libcef 运行时。
- 许可：H.264/AAC 是专利编解码器，分发启用专有编解码器的构建可能需 MPEG LA / Via LA 授权。

### 6. ⚠️ GPU 共享纹理路径可能不产帧 → 页面空白
`use_shared_texture(true)` 时走 `OnAcceleratedPaint`（GPU 共享纹理）。在某些
GPU + `multi_threaded_message_loop` 组合下该回调一次都不触发，页面全白。
排查：临时 `use_shared_texture(false)` 强制走软件 `OnPaint`，若页面恢复即可定位。
trace 里 grep `OnPaint` / `OnAcceleratedPaint` 看哪条在回调。

### 7. 命令行开关：带值开关必须用 `AppendSwitchWithValue`
`AppendSwitch("autoplay-policy=...")` 把整串当成开关名（含 '=' 无效），等于没设。
正确：`AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required")`。

## CEF 帧率说明
OSR 路径每帧把 GPU 纹理读回 CPU 再传回 GPU，天然比 WebView2 原生合成慢。
框架已做有界优化：`windowless_frame_rate` 跟随显示器刷新率（消除拍频）、
`update_frame` 去掉多余的整帧拷贝。彻底零拷贝需把 D2D 升级到 1.1 或走
OpenGL `WGL_NV_DX_interop2`，是大改动（D2D 1.0 的 `ID2D1HwndRenderTarget`
无法采样 D3D11 纹理）。

## WebView2（更流畅，独立子窗口）
见 `examples/glass_webview2.cpp`：`OtterWebView2Node::initialize` →
`OtterWebView2Layer::attach(root, w, h, win.native_handle())` → `load_url/load_html`。
适合「就要满帧流畅、不需要与图层透明混合」的场景。
