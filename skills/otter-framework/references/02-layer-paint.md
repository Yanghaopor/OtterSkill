# 02 — 图层树 与 绘画链

## 图层树（Layer）

图层构成树。根是窗口的 `__otter_canvas__`。父层用 `unique_ptr` 持有子层；
你拿到的 `Layer*` 是**观察指针，不要 delete**。

```cpp
Otter::Layer* root  = win.get["__otter_canvas__"];
Otter::Layer* panel = root->creat("panel");        // 创建/复用同名子层（幂等）
Otter::Layer* leaf  = panel->creat("a")->creat("b"); // 链式建嵌套
Otter::Layer* found = root->find("b");             // 递归查整棵子树
Otter::Layer* kid   = root->get_child("panel");    // 只查直接子层
```

### 属性（链式，返回 Layer&）

```cpp
panel->layer_bounds(20, 20, 300, 160)   // 显式边界（x,y,w,h）
     .background(Otter::Color::from_rgb_hex(0x1E2530))
     .border_radius(14.f)
     .opacity(0.9f)
     .clip_content(true)                // 裁切子内容到自身边界
     .visible(true);
```

> `background` / `border_radius` 这类「图层级效果」由框架自动绘制；
> `on_render` 里的 PaintChain 绘制叠加在其上。两者可混用。

### z 序

```cpp
panel->bring_to_front();   // 在兄弟中提到最前（最后绘制 = 视觉最上）
```

## 坐标系（重要）

- **根画布的 `on_render`**：窗口客户区像素，(0,0) 在窗口左上。
- **设了 `layer_bounds` 或 `LayoutPos` 的子层的 `on_render`**：局部坐标，
  (0,0) 在该层 bounds 左上。用 `resolved_bounds()` 取自身宽高：

```cpp
cell->on_render([cell](Otter::PaintChain& c, float){
    const Otter::Rect* b = cell->resolved_bounds();
    if (!b) return true;
    c.fill_round_rect(0, 0, b->width, b->height, 12, color);  // 局部坐标
    return true;
});
```

## 绘画链（PaintChain）全量 API

所有方法返回 `PaintChain&`，可链式。

### 路径（手动构造形状）
```cpp
c.move_to(x,y).line_to(x2,y2)
 .bezier_to(cx1,cy1, cx2,cy2, ex,ey)   // 三次贝塞尔
 .arc(cx,cy, radius, start_rad, end_rad)
 .close()
 .fill(color);          // 或 .stroke(color, width)
```

### 填充与描边
```cpp
c.fill(Otter::Color::from_rgb_hex(0x39D0FF));
c.stroke(color, 2.0f);
```

### 渐变
```cpp
Otter::LinearGradient lg(x1,y1, x2,y2, startColor, endColor);
c.fill_round_rect(x,y,w,h, r, lg);

Otter::RadialGradient rg;
rg.cx=cx; rg.cy=cy; rg.rx=60; rg.ry=60;
rg.stops.emplace_back(0.f, Otter::Color::from_rgb_hex(0xFFB84D, 1.f));
rg.stops.emplace_back(1.f, Otter::Color::from_rgb_hex(0xFFB84D, 0.f));
c.fill_circle(cx, cy, 60, rg);
```

### 快捷形状
```cpp
c.fill_rect(x,y,w,h, color);
c.fill_round_rect(x,y,w,h, radius, color);
c.stroke_round_rect(x,y,w,h, radius, color, width);
c.fill_circle(cx,cy, r, color);
c.stroke_circle(cx,cy, r, width, color);
c.stroke_line(x1,y1, x2,y2, width, color);
```

### 文字
```cpp
Otter::TextStyle ts;
ts.font_family = L"Segoe UI";
ts.font_size   = 24;
ts.weight      = Otter::TextStyle::Weight::Bold;
ts.color       = Otter::Color::white();
ts.h_align     = Otter::TextStyle::HAlign::Center;   // Left/Center/Right/Justified
ts.v_align     = Otter::TextStyle::VAlign::Middle;   // Top/Middle/Bottom
c.text(L"单行", x, y, ts);
c.text(L"带框换行", x, y, max_w, max_h, ts);   // 文本框，自动换行 + 对齐
// 也接受 UTF-8 std::string
```

### 图片 / 裁切
```cpp
c.img(L"path/to.png", x, y, w, h, /*opacity=*/1.f, /*radius=*/8.f);
c.push_clip_rect(x,y,w,h);  /* ...绘制... */  c.pop_clip();
c.push_round_clip(x,y,w,h, radius); /* ... */ c.pop_clip();
```

## Color 速查
```cpp
Otter::Color::from_rgb_hex(0x39D0FF);          // 不透明
Otter::Color::from_rgb_hex(0x39D0FF, 0.5f);    // 带 alpha
Otter::Color::from_rgba_hex(0x39D0FF80);        // RGBA 一体
Otter::Color::white();  Otter::Color::black();
Otter::Color c{r,g,b,a};  c.a = 0.3f;          // 直接改分量（0~1）
```
