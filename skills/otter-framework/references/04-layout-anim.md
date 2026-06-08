# 04 — 布局 与 动画

## 网格布局

两步：父层（或窗口）声明网格 → 子层用 `LayoutPos` 占位。框架自动算出每个子层的
bounds，子层 `on_render` 内用**局部坐标**绘制。

### 窗口级网格
```cpp
win.Layout(/*cols=*/3, /*rows=*/3,
           /*pad_x=*/28, /*pad_y=*/28,   // 外边距
           /*gap_x=*/16, /*gap_y=*/16);  // 单元间距
// 参考尺寸自动取窗口宽高
```

### 子层占位
```cpp
Otter::Layer* cell = root->creat("cell_0");
cell->LayoutPos(/*col=*/0, /*row=*/0, /*col_span=*/1, /*row_span=*/1);
cell->auto_hit_area(true);   // 命中区 = 网格单元
cell->on_render([cell](Otter::PaintChain& c, float){
    const Otter::Rect* b = cell->resolved_bounds();
    if (!b) return true;
    c.fill_round_rect(0, 0, b->width, b->height, 12, color);   // 局部坐标
    return true;
});
```

### 图层级网格（容器子层再分网格）
```cpp
panel->Layout(cols, rows, ref_w, ref_h, origin_x, origin_y, pad_x, pad_y, gap_x, gap_y);
```

> `LayoutPos` 与显式 `layer_bounds` 互斥——**最后调用的生效**（PosSource 标志控制）。

### 查询某格矩形（不创建子层）
```cpp
Otter::Rect r = win.layout_rect(col, row, col_span, row_span);
```

## 动画

图层自带补间动画（由窗口的 AnimManager 驱动，需窗口已 attach）。

```cpp
layer->animate_opacity(/*to=*/0.f, /*dur=*/0.4f,
                       Otter::Easing::EaseOutCubic,
                       /*on_complete=*/[]{ /* 完成回调 */ });

layer->animate_rotate(/*to_radians=*/3.14159f, 0.6f, Otter::Easing::EaseInOutSine);

// 组合多轨
layer->animate({ /* std::vector<AnimTrack> */ }, []{});

layer->cancel_animations();   // 取消该层全部动画
```

### Easing 枚举
`Linear`、`EaseIn/Out/InOut` × `Quad Cubic Quart Sine Expo Back`。
（如 `Easing::EaseOutBack` 有回弹感，适合弹出动画。）

## 手动逐帧动画（更灵活）

很多场景不用补间，直接在 `on_update` 累积时间、在 `on_render` 按时间画：
```cpp
auto phase = std::make_shared<float>(0.f);
root->on_update([phase](float dt){ *phase += dt; return true; });
root->on_render([phase](Otter::PaintChain& c, float){
    float y = 100 + std::sin(*phase * 2.f) * 30.f;   // 正弦上下浮动
    c.fill_circle(200, y, 20, color);
    return true;
});
```

> 指数平滑（朝目标逼近）很实用：
> `cur += (target - cur) * (1 - exp(-rate * dt));`
