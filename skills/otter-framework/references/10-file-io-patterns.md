# 10 — 文件读写 / 剪贴板 / 编码 / 持久化模式

Otter 是 GUI 框架，不内置文件系统封装。但在 Otter 应用中处理文件是高频需求。
本册提供**经过验证的跨平台安全模式**。

## 目录
1. UTF-8 文件读写
2. 剪贴板操作
3. Win32 文件对话框封装
4. 自动保存模式
5. 最近文件列表（MRU）
6. 日志文件

---

## 1. UTF-8 文件读写

Otter 内部使用 `std::wstring`（UTF-16），因为 DirectWrite 和 Win32 API 原生接受宽字符。
读写文件时，需做 UTF-8 ↔ UTF-16 转换。

### 读 UTF-8 文件
```cpp
std::wstring read_file_utf8(const std::wstring& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string raw((std::istreambuf_iterator<char>(f)), {});
    if (raw.empty()) return {};

    // 跳过 UTF-8 BOM（可选）
    size_t start = 0;
    if (raw.size() >= 3
        && (unsigned char)raw[0] == 0xEF
        && (unsigned char)raw[1] == 0xBB
        && (unsigned char)raw[2] == 0xBF)
        start = 3;

    std::wstring result;
    int n = MultiByteToWideChar(CP_UTF8, 0,
        raw.data() + start, (int)(raw.size() - start),
        nullptr, 0);
    if (n > 0) {
        result.resize(n);
        MultiByteToWideChar(CP_UTF8, 0,
            raw.data() + start, (int)(raw.size() - start),
            result.data(), n);
    }
    return result;
}
```

### 写 UTF-8 文件
```cpp
void write_file_utf8(const std::wstring& path, const std::wstring& content)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return;
    // UTF-8 BOM（可选但推荐，记事本能正确识别）
    f.write("\xEF\xBB\xBF", 3);
    int n = WideCharToMultiByte(CP_UTF8, 0,
        content.data(), (int)content.size(),
        nullptr, 0, nullptr, nullptr);
    if (n > 0) {
        std::string utf8(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0,
            content.data(), (int)content.size(),
            utf8.data(), n, nullptr, nullptr);
        f.write(utf8.data(), n);
    }
}
```

### 检测编码（简易）
如果没有 BOM 且内容含非 ASCII 字节，按以下优先级试：
1. UTF-8 BOM（`0xEF 0xBB 0xBF`）→ UTF-8
2. UTF-16 LE BOM（`0xFF 0xFE`）→ UTF-16 LE
3. UTF-16 BE BOM（`0xFE 0xFF`）→ UTF-16 BE
4. 无 BOM 但 `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)` 成功 → UTF-8
5. 否则 → 系统 ANSI 代码页（`CP_ACP`）或 GBK（`936`）

```cpp
bool is_valid_utf8(const std::string& s)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        s.data(), (int)s.size(), nullptr, 0) > 0;
}
```

---

## 2. 剪贴板操作

Otter 在 `OtterWidget.h` 中提供了 `clipboard_set()` / `clipboard_get()`：

```cpp
Otter::clipboard_set(L"复制这段文字");
std::wstring text = Otter::clipboard_get();
```

内部使用 `ClipboardGuard` RAII 类——构造时 `OpenClipboard()`，析构时自动 `CloseClipboard()`，异常安全。

### 粘贴时处理 `\r\n` → `\n`
从剪贴板取出的文本可能含 `\r\n`（Windows 惯例），写入编辑器前需统一为 `\n`：
```cpp
std::wstring clip = Otter::clipboard_get();
std::wstring safe;
for (wchar_t c : clip)
    if (c != L'\r') safe += c;
ta.set_text(safe);  // 或 buffer_.insert(...)
```

---

## 3. Win32 文件对话框封装

```cpp
#include <commdlg.h>

// 打开文件对话框
std::wstring open_file_dialog(HWND owner)
{
    wchar_t buf[MAX_PATH * 2]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof(buf) / sizeof(wchar_t);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) return buf;
    return {};
}

// 保存文件对话框
std::wstring save_file_dialog(HWND owner)
{
    wchar_t buf[MAX_PATH * 2]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof(buf) / sizeof(wchar_t);
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&ofn)) return buf;
    return {};
}

// 获取 owner HWND：otterwindow 的 native_handle()
HWND hwnd = static_cast<HWND>(win.native_handle());
```

---

## 4. 自动保存模式

在 `on_update` 中累积计时，达到阈值且内容有变化时保存：

```cpp
auto st = std::make_shared<EditorState>();
st->file_path = L"";
st->dirty = false;
st->auto_save_timer = 0.f;
st->auto_save_interval = 30.f;  // 30 秒
st->auto_save_enabled = true;

// 在 TextArea 的 on_change 中标记脏数据
ta.on_change([st](const std::wstring&) { st->dirty = true; });

// 在根层的 on_update 中做自动保存
root->on_update([st, &ta](float dt) -> bool {
    if (st->auto_save_enabled && st->dirty && !st->file_path.empty()) {
        st->auto_save_timer += dt;
        if (st->auto_save_timer >= st->auto_save_interval) {
            write_file_utf8(st->file_path, ta.text());
            st->auto_save_timer = 0.f;
            st->dirty = false;
        }
    }
    return true;
});
```

**注意**：`write_file_utf8` 在主线程做。大文件（>1MB）建议用后台线程写，避免卡帧。

---

## 5. 最近文件列表（MRU — Most Recently Used）

用 Windows 注册表持久化最近文件列表：

```cpp
#include <shlobj.h>  // SHGetFolderPathW

// 读取 MRU
std::vector<std::wstring> load_mru()
{
    std::vector<std::wstring> result;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\MyApp\\MRU", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        for (int i = 0; i < 10; ++i) {
            wchar_t name[32], value[MAX_PATH];
            DWORD name_len = 32, value_len = sizeof(value), type = 0;
            swprintf_s(name, L"%d", i);
            if (RegQueryValueExW(hKey, name, nullptr, &type,
                (BYTE*)value, &value_len) == ERROR_SUCCESS && type == REG_SZ)
                result.push_back(value);
        }
        RegCloseKey(hKey);
    }
    return result;
}

// 保存 MRU（最多 10 条）
void save_mru(const std::wstring& path)
{
    std::vector<std::wstring> list = load_mru();
    // 去重 + 移到最前
    list.erase(std::remove(list.begin(), list.end(), path), list.end());
    list.insert(list.begin(), path);
    if (list.size() > 10) list.resize(10);

    HKEY hKey;
    RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\MyApp\\MRU", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    for (size_t i = 0; i < list.size(); ++i) {
        wchar_t name[32];
        swprintf_s(name, L"%zu", i);
        RegSetValueExW(hKey, name, 0, REG_SZ,
            (BYTE*)list[i].c_str(),
            (DWORD)((list[i].size() + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(hKey);
}
```

---

## 6. 日志文件

Otter 内置日志系统 `OtterDebug.h`：

```cpp
// 开启文件日志
Otter::Logger::instance().set_file("myapp_debug.log");
Otter::Logger::instance().set_min_level(Otter::LogLevel::Debug);

// 宏（自动附带文件名和行号）
OTTER_LOG_INFO("subsystem", "message");
OTTER_LOG_WARN("network", Otter::format_log("timeout: %d ms", ms));
OTTER_LOG_ERROR("renderer", "device lost");

// 作用域计时器
void do_heavy_work() {
    Otter::ScopeTimer timer("perf", "heavy_work");
    // ... 退出作用域时自动打印耗时 ...
}

// format_log: printf 风格格式化（支持 %d %s %f 等）
std::string msg = Otter::format_log("frame %d: %d layers, %.2f ms", frame, layers, ms);
```

日志自动输出到 **stderr + 文件 + `OutputDebugStringA`**（VS 调试器可见）。

### 日志级别
`Trace < Debug < Info < Warning < Error < Fatal`  
默认最低级别 = `Info`（Trace/Debug 不输出）。
