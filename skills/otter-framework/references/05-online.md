# 05 — P2P 网络（OtterOnline）与线程安全

`OtterOnline.h` 提供 UDP P2P：通过 rendezvous 服务器 + STUN 做 NAT 打洞，
打洞成功后直连，失败则经服务器中继（Relay）。

## 关键：线程安全模型（v1.0.4 必读）

`Peer` 内部有 worker 线程收发网络。**绝不能在 worker 线程直接操作图层树/UI**。
框架的设计是：worker 线程把收到的事件**入队**，主线程每帧调用 `poll_events()`
在**主线程**触发 `on_connected` / `on_data` 回调。

```cpp
Otter::Online::Peer::Config cfg;
cfg.rendezvous_host = "your.server.com";
cfg.rendezvous_port = 37640;
// cfg.session / cfg.peer_id 默认随机；同一会话两端 session 要一致

Otter::Online::Peer peer(cfg);

peer.on_connected([](Otter::Online::Endpoint ep){
    // 主线程安全：可以改 UI
});
peer.on_data([st](std::vector<uint8_t> data, uint64_t from){
    // 主线程安全：收到对端数据
});

peer.start();   // 启动 worker 线程（开始打洞）

// ── 在你的主循环 / on_update 里，每帧消费事件 ──
root->on_update([&peer](float){
    peer.poll_events();    // ⚠️ 必须每帧调用，否则回调永不触发
    return true;
});

// 发送（任意线程安全）
const char* msg = "hello";
peer.send(msg, 5);

bool ok = peer.connected();   // 是否已直连
peer.stop();                  // 析构也会自动 stop
```

## Config 字段
```
uint64_t session;           // 会话 ID（两端需一致才能配对）
uint64_t peer_id;           // 本端 ID
std::string rendezvous_host; uint16_t rendezvous_port;  // 集合服务器
uint16_t local_port = 0;    // 0 = 系统分配
bool use_stun = true;       // 用 STUN 探测公网地址
std::string stun_host;      uint16_t stun_port;
uint64_t direct_timeout_ms; // 直连超时（超时回退中继）
```

## Rendezvous 服务器
`OtterOnline.h` 内含 `RendezvousServer`，可单独编一个进程跑在公网，
负责撮合同 session 的两端、交换彼此的公网 endpoint。

## 陷阱
- ❌ 忘记调 `poll_events()` → `on_data` 永远不触发（事件堆在队列里）。
- ❌ 在 `on_data` 里做重活/阻塞 → 卡住主线程渲染。重活应转后台并把结果再投递回来。
- ✅ 共享状态（如收到的消息列表）也用 `shared_ptr`，回调 `[st]` 捕获。
