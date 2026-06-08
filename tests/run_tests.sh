#!/usr/bin/env bash
# ============================================================
#  tests/run_tests.sh —— Otter 框架编译 / 测试一键脚本
#
#  覆盖三层验证（与 CI 对齐）：
#    A. 核心层无渲染单元测试（跨平台，编译并运行）
#    B. 全部头文件 MinGW 语法检查（-fsyntax-only）
#    C. Windows GUI 示例真实链接（产出 .exe）
#
#  用法：
#    bash tests/run_tests.sh            # 跑全部
#    bash tests/run_tests.sh core       # 仅核心单元测试
#    bash tests/run_tests.sh headers    # 仅头文件语法检查
#    bash tests/run_tests.sh examples   # 仅示例链接
#
#  依赖：g++（支持 C++20），Windows 上为 MSYS2 MinGW-w64。
# ============================================================
set -u

# 切到框架根目录（脚本位于 tests/ 下）
cd "$(dirname "$0")/.." || exit 2

CXX="${CXX:-g++}"
STD="-std=c++20"
OUT="${TMPDIR:-/tmp}/otter_test_out"
mkdir -p "$OUT"

# Windows GUI 链接所需系统库（D2D + DWrite + WIC + OpenGL + WinHTTP + IME）
WIN_LIBS="-ld2d1 -ldwrite -lwindowscodecs -lgdi32 -luser32 -lole32 -ldwmapi -lopengl32 -lwinhttp -limm32"

FAIL=0

# ── A. 核心层无渲染单元测试 ──────────────────────────────────
run_core()
{
    echo "=== [A] 核心层无渲染单元测试 ==="
    if "$CXX" $STD -I. tests/test_core_headless.cpp -o "$OUT/test_core" 2>"$OUT/core_err.txt"; then
        if "$OUT/test_core"; then
            echo "  [OK] 核心测试通过"
        else
            echo "  [FAIL] 核心测试运行失败"; FAIL=1
        fi
    else
        echo "  [FAIL] 核心测试编译失败:"; cat "$OUT/core_err.txt"; FAIL=1
    fi
}

# ── B. 头文件语法检查 ────────────────────────────────────────
run_headers()
{
    echo "=== [B] 头文件 MinGW 语法检查 ==="
    local headers=(OtterLayer OtterText OtterRenderer OtterOpenGLRenderer \
                   OtterPortableOpenGLRenderer OtterWidget OtterWindow OtterOnline \
                   OtterInput OtterPlatform OtterWindowFactory OtterDebug OtterCreat)
    for h in "${headers[@]}"; do
        printf '#include "%s.h"\nint main(){return 0;}\n' "$h" > "$OUT/h.cpp"
        if "$CXX" $STD -fsyntax-only -I. -Iapp "$OUT/h.cpp" 2>"$OUT/h_err.txt"; then
            echo "  [OK] $h.h"
        else
            echo "  [FAIL] $h.h:"; head -5 "$OUT/h_err.txt"; FAIL=1
        fi
    done
}

# ── C. Windows GUI 示例链接 ──────────────────────────────────
run_examples()
{
    echo "=== [C] Windows GUI 示例链接 ==="
    for ex in examples/*.cpp; do
        name="$(basename "$ex" .cpp)"
        # 跳过依赖可选组件（WebView2 / Chrome-CEF）的示例：
        # 它们需要额外编译 app/*.cpp 翻译单元并链接 WebView2/CEF SDK，
        # 不在「纯框架」链接范围内（见 README_STANDALONE.md 可选组件说明）。
        case "$name" in
            glass_webview2|*chrome*|*cef*)
                echo "  [SKIP] $name（需可选组件 WebView2/CEF SDK，单独构建）"
                continue ;;
        esac
        if "$CXX" $STD -I. "$ex" -o "$OUT/$name.exe" $WIN_LIBS 2>"$OUT/ex_err.txt"; then
            echo "  [OK] $name  ->  $OUT/$name.exe"
        else
            echo "  [FAIL] $name:"; head -8 "$OUT/ex_err.txt"; FAIL=1
        fi
    done
}

case "${1:-all}" in
    core)     run_core ;;
    headers)  run_headers ;;
    examples) run_examples ;;
    all)      run_core; run_headers; run_examples ;;
    *)        echo "未知参数: $1（可选 core/headers/examples/all）"; exit 2 ;;
esac

echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo "全部通过 ✅"
else
    echo "存在失败 ❌"
fi
exit $FAIL
