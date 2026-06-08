# 07 — OOP 与 自建 UI

## 目录
- 1. 大型 UI 的拆分原则
- 2. 推荐对象模型
- 3. 控件构造顺序
- 4. 常见 UI 组件映射
- 5. 状态与回调
- 6. 一个可复用的组件骨架

## 1. 大型 UI 的拆分原则

Otter 的正确用法是“图层树 + 状态对象 + 组件类”，不是把所有逻辑塞进一个 `on_render`。

先把界面切成三层：

- 应用状态：纯数据，尽量不直接持有 `Layer*`。
- 组件对象：负责创建图层、绑定事件、刷新显示。
- 页面容器：负责组合多个组件和切换页面。

原则：

- 结构初始化只做一次。
- 绘制回调只画，不造结构。
- 状态变化驱动界面变化，不反过来。
- 需要跨帧保存的图层引用，优先用 `LayerHandle`。

## 2. 推荐对象模型

适合 Otter 的写法通常是：

```cpp
struct AppState
{
    int page = 0;
    bool connected = false;
    std::wstring input;
};

class DashboardPage
{
public:
    DashboardPage(Otter::Layer* parent, std::shared_ptr<AppState> st);
    void refresh();

private:
    void build();
    void bind();

    Otter::Layer* root_ = nullptr;
    std::shared_ptr<AppState> st_;
};
```

不要把 `AppState` 和 UI 逻辑混成一个类的全部成员。状态类越纯，越容易复用和测试。

## 3. 控件构造顺序

1. 先创建容器层。
2. 再创建视觉层。
3. 再设置布局和命中区。
4. 再绑事件。
5. 最后写渲染回调。

如果控件很多，先按页面拆，再按区域拆，再按控件拆。一个面板类负责一块逻辑，不要跨页读写。

## 4. 常见 UI 组件映射

- 卡片：`layer_bounds` + `background` + `border_radius`
- 按钮：`hit_area` + `on_click`
- 悬停按钮：再加 `on_mouse_enter` / `on_mouse_leave`
- 输入框：`set_keyboard_target`
- 列表项：子层 + `LayerHandle`
- 网格：`Layout` + `LayoutPos`
- 浮层提示：`overlay()` 或单独的顶层子树

## 5. 状态与回调

回调里常见的正确模式：

```cpp
auto st = std::make_shared<AppState>();
button->on_click([st](const Otter::MouseEvent&) -> bool {
    st->page = 1;
    return true;
});
```

建议：

- 数据用 `shared_ptr`。
- 长期保存图层用 `LayerHandle`。
- 短期局部逻辑可以用普通局部变量。
- 不要捕获会失效的栈引用后又让回调活很久。

## 6. 一个可复用的组件骨架

```cpp
class ToggleCard
{
public:
    ToggleCard(Otter::Layer* parent, std::shared_ptr<AppState> st)
        : st_(std::move(st))
    {
        root_ = parent->creat("toggle_card");
        build();
        bind();
    }

    void refresh()
    {
        if (root_ == nullptr)
            return;
        root_->visible(true);
    }

private:
    void build()
    {
        root_->layer_bounds(20, 20, 240, 120);
    }

    void bind()
    {
        root_->on_click([st = st_](const Otter::MouseEvent&) -> bool {
            st->connected = !st->connected;
            return true;
        });
    }

    Otter::Layer* root_ = nullptr;
    std::shared_ptr<AppState> st_;
};
```

这个模式的重点是：

- 创建和绑定分开。
- 组件只依赖自己的状态和父层。
- 页面把多个组件拼起来即可。

