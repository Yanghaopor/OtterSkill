# 12 — 高级文本编辑 / 内联 IME / OpenAI Responses

在实现长文写作、中文输入、文档排版、文本文件保存或 AI 辅助写作时读取本分册。不要把
Project1 的小说数据、世界观、RAG 或具体 AI 面板当作框架 API；它们仍属于应用层。

## 1. 选择组件

| 需求 | 使用 |
|---|---|
| 常规表单或短文本 | `TextField` / `TextArea`，见 `09-widgets.md` |
| 长文、多行选择、查找替换、平滑滚动、中文输入法 | `TextDocument` + `DocumentEditor` |
| 自定义正文视图或独立排版器 | `TextDocument` + `TextLayout` |
| 保留文本编码和行尾、原生文件对话框 | `app/OtterTextFileIO.h` |
| OpenAI Responses、SSE、工具循环或结构化输出 | `OtterOpenAIResponses.h` |

所有文本组件位于 `Otter::Text`，AI 客户端位于 `Otter::AI`。它们均为可选的
header-only 组件，按需包含相应的 `app/` 头文件。

## 2. 创建文档编辑器

让文档对象和编辑器对象覆盖整个 `win.run()` 生命周期；`DocumentEditor` 的图层回调
会引用编辑器自身。不要在 `page` 图层仍挂载时销毁编辑器，动态切换页面时应先移除该
图层，或让编辑器对象与窗口保持相同生命周期。

```cpp
#include "OtterWindow.h"
#include "app/OtterDocumentEditor.h"

int main()
{
    Otter::otterwindow win(960, 640, L"Document");
    Otter::Text::TextDocument document;
    document.reset_text(L"第一章\n");

    Otter::Layer* root = win.get["__otter_canvas__"];
    if (!root) return 1;

    Otter::Text::DocumentEditor editor(&win, root, &document);
    editor.set_bounds(0.f, 0.f, 960.f, 640.f);
    win.on_dpi_changed([&editor](float scale) {
        Otter::Text::Theme::set_scale(scale);
        editor.apply_metrics();
    });
    editor.focus();

    win.run();
    return 0;
}
```

`focus()` 会注册键盘目标、候选窗光标位置和内联 IME 回调。切换到其他输入控件、隐藏
编辑器或销毁编辑器前，调用 `defocus()`。同一窗口不要同时为一个已聚焦的
`DocumentEditor` 再注册另一组 `on_ime_*` 回调。

不要在 `on_ime_composition` 中修改 `TextDocument`：组合串尚未上屏。只在
`on_ime_result` 提交文本；`DocumentEditor` 已自动按此规则处理。

### 文档模型与排版

`TextDocument` 独立于窗口，可用于无界面测试。使用 `reset_text()` 载入全文，使用
`text()` 导出，使用 `insert_text()`、`replace_range()`、`undo()`、`redo()` 和
`find_next()` / `replace_all()` 做编辑操作。连续输入的撤销合并依赖时间推进；由
`DocumentEditor` 驱动时无需自行处理。

优先让 `DocumentEditor` 持有并驱动 `TextLayout`。只有在实现不同视觉或交互模型时才
直接使用 `TextLayout`；它的绘制路径针对 Windows 桌面后端，核心文档逻辑仍可无窗口测试。

## 3. 文件读写

`app/OtterTextFileIO.h` 自动识别 UTF-8、UTF-16 和 GBK，并保留输入文件的行尾信息。保存时
使用 `save_encoding_for()`，不要手写一套编码判断或直接覆盖源文件。

```cpp
#include "app/OtterTextFileIO.h"

Otter::Text::LoadResult loaded = Otter::Text::load_text_file(path);
if (loaded.ok) {
    document.reset_text(loaded.text);

    std::wstring error;
    bool saved = Otter::Text::save_text_file(
        path,
        document.text(),
        Otter::Text::save_encoding_for(loaded.encoding),
        loaded.lineEnding,
        error);
    if (saved) document.mark_saved();
}
```

`open_file_dialog()` 和 `save_file_dialog()` 需要 Windows 的 `comdlg32.lib`。文件对话框、
网络请求和大文件读写都不得放进 `on_render`；耗时操作放到工作线程，再由主线程更新 UI 状态。

## 4. 调用 Responses API

`OtterOpenAIResponses.h` 是 Windows/WinHTTP 客户端。显式提供模型和 API Key；只把 Key
保留在内存或受控的运行时配置中，绝不写进源码、日志、缓存键或项目文件。

```cpp
#include "app/OtterOpenAIResponses.h"
#include <cstdlib>

const char* apiKey = std::getenv("OPENAI_API_KEY");
if (apiKey) {
    Otter::AI::ClientConfig config;
    config.apiKey = apiKey;

    Otter::AI::ResponsesClient client(config);
    Otter::AI::ResponsesRequest request;
    request.model = "your-enabled-model";  // 必须显式选择可用模型
    request.instructions = "Return a concise revision.";
    request.inputText = "Revise this paragraph.";

    Otter::AI::ResponsesResult result = client.run(request);
    if (result.ok) {
        std::string text = result.outputText;
        // 在主线程把 text 转换并写入应用状态或 TextDocument。
    }
}
```

`run()` 仅处理非流式请求。需要 SSE 时调用 `run_streaming()` 并处理 `StreamEvent`；需要
本地函数工具时传入 `ToolRegistry`，需要 JSON Schema 时设置
`ResponsesRequest::structuredOutput`。优先阅读该头文件和实际框架仓库中
`tests/test_openai_responses.cpp` 的已验证样例，而不是手工拼接 JSON 或 HTTP 请求。

网络调用会阻塞，不能直接在 `on_click`、`on_update` 或 `on_render` 中执行。由工作线程运行
客户端，并让主线程只读取已完成结果、更新共享状态和刷新图层。默认传输为
`WinHttpTransport`，链接 `winhttp.lib`；测试可注入 `IHttpTransport`。

## 5. 回归验证

在包含 v1.0.8 头文件的实际 Otter 框架根目录运行：

```bash
bash tests/run_tests.sh extensions
```

该组覆盖文档模型、中文输入、排版、编辑器、Responses 和 IME API。运行完整框架回归时使用
`bash tests/run_tests.sh`。
