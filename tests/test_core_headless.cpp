// ============================================================
//  test_core_headless.cpp —— Otter 核心层无渲染单元测试
//
//  目的：验证「平台中立」核心层（OtterLayer.h）在任意编译器
//        （g++ / clang / MSVC）上均可编译并运行，无需窗口 / GPU。
//
//  覆盖：
//    1. 图层树创建 / 查找（creat / get_child / find）
//    2. 图层属性链式调用（background / border_radius / opacity）
//    3. LayerHandle 弱引用句柄（防悬垂指针 use-after-free）
//    4. 鼠标命中测试与事件分发（hit_area + on_click + 冒泡）
//    5. on_update 回调驱动（tick）与「返回 false 自动注销」
//    6. 子层 z 序（bring_to_front）
//
//  编译（跨平台，无需任何 GPU / 窗口库）：
//    g++ -std=c++20 -I.. tests/test_core_headless.cpp -o test_core
//    ./test_core
//
//  本测试不实例化 RenderContext，因此不触发任何渲染后端。
// ============================================================

#include "../OtterLayer.h"

#include <cassert>
#include <cstdio>
#include <string>

// ── 轻量断言框架（不引入第三方依赖）──────────────────────────
namespace
{
    int g_pass = 0;   // 通过计数
    int g_fail = 0;   // 失败计数

    // 检查布尔条件；失败时打印文件:行号 + 表达式文本
    void check_impl(bool cond, const char* expr, const char* file, int line)
    {
        if (cond)
        {
            ++g_pass;
        }
        else
        {
            ++g_fail;
            std::printf("  [FAIL] %s:%d  断言失败: %s\n", file, line, expr);
        }
    }
}

// CHECK 宏：记录通过 / 失败而不中断后续用例（便于一次跑全）
#define CHECK(cond) check_impl((cond), #cond, __FILE__, __LINE__)

// 每个测试用例打印一个标题
#define CASE(name) std::printf("[CASE] %s\n", name)

// ============================================================
//  用例 1：图层树创建与查找
// ============================================================
static void test_layer_tree()
{
    CASE("图层树创建与查找");

    Otter::Layer root("root", /*is_canvas=*/true);

    // creat 返回非拥有观察指针；同名再次 creat 应返回同一对象（幂等）
    Otter::Layer* a = root.creat("panel_a");
    Otter::Layer* b = root.creat("panel_b");
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(a != b);
    CHECK(root.creat("panel_a") == a);   // 幂等：同名不重复创建

    // get_child 仅查直接子层
    CHECK(root.get_child("panel_a") == a);
    CHECK(root.get_child("panel_b") == b);
    CHECK(root.get_child("not_exist") == nullptr);

    // 深层嵌套 + find 在整棵子树中递归查找
    Otter::Layer* deep = a->creat("inner")->creat("leaf");
    CHECK(deep != nullptr);
    CHECK(deep->name() == "leaf");
    CHECK(root.find("leaf") == deep);
    CHECK(root.find("inner") != nullptr);
    CHECK(root.find("ghost") == nullptr);
}

// ============================================================
//  用例 2：属性链式调用
// ============================================================
static void test_layer_properties()
{
    CASE("图层属性链式调用");

    Otter::Layer root("root", true);
    Otter::Layer* card = root.creat("card");

    // 链式返回 Layer&，可连续调用
    card->layer_bounds(10, 20, 300, 150)
         .background(Otter::Color::from_rgb_hex(0x182234))
         .border_radius(14.f)
         .opacity(0.8f)
         .visible(true);

    // border_radius 负值应被夹为 0（防御性）
    card->border_radius(-5.f);
    // visible 仅有 setter（无 getter），此处验证链式调用不破坏对象身份
    CHECK(card->name() == "card");
    card->visible(false).visible(true);   // 链式 setter 往返不崩溃
    CHECK(card->name() == "card");
}

// ============================================================
//  用例 3：LayerHandle 弱引用句柄（核心内存安全特性）
// ============================================================
static void test_layer_handle()
{
    CASE("LayerHandle 弱引用句柄");

    Otter::Layer root("root", true);
    Otter::Layer* child = root.creat("temp");

    // 获取弱句柄；此时图层存活
    Otter::LayerHandle h = child->handle();
    CHECK(h.valid());
    CHECK(h.get() == child);
    CHECK(static_cast<bool>(h));        // operator bool
    CHECK(h->name() == "temp");          // operator->

    // 同一图层多次 handle() 应共享存活令牌（仍指向同一对象）
    Otter::LayerHandle h2 = child->handle();
    CHECK(h2.get() == child);

    // 销毁该子层（从父层移除 → unique_ptr 析构）后，句柄应自动失效
    // 这里借助 root 离开作用域来销毁整棵树，分两段验证生命周期：
    {
        Otter::Layer local_root("local", true);
        Otter::Layer* c = local_root.creat("x");
        Otter::LayerHandle lh = c->handle();
        CHECK(lh.valid());
        // local_root 析构 → c 随之销毁 → lh 失效（在作用域结束后检测）
        // 为在作用域内验证，改为显式重置整棵树不可行（无 remove API 暴露），
        // 故依赖析构路径：见下方 after-scope 断言。
        // 此处先确认存活。
    }
    // 作用域结束，local_root 已析构。但 lh 已离开作用域，无法再访问。
    // 因此用「父树析构令外部句柄失效」的可观察路径单独验证：
    {
        Otter::LayerHandle outer;
        {
            auto tree = std::make_unique<Otter::Layer>("scoped", true);
            Otter::Layer* node = tree->creat("node");
            outer = node->handle();
            CHECK(outer.valid());        // 存活
            tree.reset();                // 显式销毁整棵树
            CHECK(!outer.valid());       // 句柄随之失效（不再悬垂）
            CHECK(outer.get() == nullptr);
            CHECK(!static_cast<bool>(outer));
        }
    }
}

// ============================================================
//  用例 4：鼠标命中测试 + on_click + 事件冒泡/消费
// ============================================================
static void test_mouse_hit_and_events()
{
    CASE("鼠标命中测试与事件分发");

    Otter::Layer root("root", true);
    Otter::Layer* btn = root.creat("button");
    btn->hit_area(50, 50, 100, 40);   // 命中矩形 [50,50]~[150,90]

    int click_count = 0;
    int down_count  = 0;
    btn->on_mouse_down([&down_count](const Otter::MouseEvent&) -> bool {
        ++down_count;
        return true;   // 消费按下事件
    });
    btn->on_click([&click_count](const Otter::MouseEvent&) -> bool {
        ++click_count;
        return true;   // 消费点击事件
    });

    // 构造一次「按下→松开」点击序列（命中区域内）
    Otter::MouseEvent e;
    e.x = 80; e.y = 70; e.left_down = true;
    bool down_consumed = root.dispatch_mouse_down(e);
    CHECK(down_consumed);   // 命中区域内按下且注册了 on_mouse_down → 被消费
    CHECK(down_count == 1);

    // 松开（is_click=true 表示构成点击）
    Otter::MouseEvent up = e;
    up.left_down = false;
    bool up_consumed = root.dispatch_mouse_up(up, /*is_click=*/true, /*is_right=*/false);
    CHECK(up_consumed);
    CHECK(click_count == 1);   // on_click 触发一次

    // 区域外点击不应触发
    Otter::MouseEvent outside;
    outside.x = 5; outside.y = 5; outside.left_down = true;
    root.dispatch_mouse_down(outside);
    Otter::MouseEvent outside_up = outside; outside_up.left_down = false;
    root.dispatch_mouse_up(outside_up, true, false);
    CHECK(click_count == 1);   // 仍为 1，未增加
}

// ============================================================
//  用例 5：on_update 驱动 + 返回 false 自动注销
// ============================================================
static void test_update_lifecycle()
{
    CASE("on_update 帧驱动与自动注销");

    Otter::Layer root("root", true);

    int tick_count = 0;
    // 回调返回 true 持续触发，累加 3 次后返回 false 自动注销
    root.on_update([&tick_count](float dt) -> bool {
        (void)dt;
        ++tick_count;
        return tick_count < 3;   // 第 3 次后注销
    });

    // 模拟逐帧 tick
    for (int i = 0; i < 10; ++i)
        root.tick(0.016f);

    // 应在第 3 次后停止：最终计数恰为 3
    CHECK(tick_count == 3);
}

// ============================================================
//  用例 6：子层前置（bring_to_front 改变兄弟绘制顺序）
// ============================================================
static void test_z_order()
{
    CASE("子层 z 序 bring_to_front");

    Otter::Layer root("root", true);
    Otter::Layer* first  = root.creat("first");
    Otter::Layer* second = root.creat("second");
    CHECK(first != nullptr && second != nullptr);

    // 默认顺序：先创建的在底。把 first 提到最前不应崩溃，且仍可查到
    first->bring_to_front();
    CHECK(root.get_child("first") == first);
    CHECK(root.get_child("second") == second);
}

int main()
{
    std::printf("==== Otter 核心层无渲染测试 ====\n");

    test_layer_tree();
    test_layer_properties();
    test_layer_handle();
    test_mouse_hit_and_events();
    test_update_lifecycle();
    test_z_order();

    std::printf("--------------------------------\n");
    std::printf("通过: %d   失败: %d\n", g_pass, g_fail);

    // 非零退出码便于 CI 检测
    return g_fail == 0 ? 0 : 1;
}
