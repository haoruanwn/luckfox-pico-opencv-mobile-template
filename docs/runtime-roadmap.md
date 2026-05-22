# RV1106 视觉 runtime 路线图

这个仓库正在从一个“大而全的 RK MPI 封装”重置成一个更小、更清晰的
边缘视觉 runtime 练习项目。真正要练的是 runtime；RK MPI 只是其中一个后端。

## 方向

目标是做一个现代一点的嵌入式视觉 pipeline runtime：

```text
设备生命周期
+ move-only frame 所有权
+ node 状态机
+ 有界软件队列
+ 硬件绑定边
+ 错误与指标观测
```

长期结构分三层：

```text
rmg-sys
  很薄的 RAII 封装，覆盖 RK MPI、RKAIQ、VENC、RGA、NPU、RTSP 等厂商 API。
  这一层不做 graph，不开后台线程，不做 callback 框架。

rmg-runtime
  Graph、Node、Port、Executor、Queue、Cancellation、Result、Metrics。
  这是这个项目最主要的练习对象。

rmg-nodes
  CameraNode、JpegEncoderNode、VencNode、RtspSink、FileSink、RgaNode、NpuNode。
  这一层把具体硬件/软件能力适配成 runtime 节点。
```

## 原则

- 所有权要明确。Frame 是 move-only 的 RAII 值对象。
- 控制面和数据面分离。
- 硬件绑定是第一类 edge，不要藏在 callback 里。
- 软件 edge 默认必须有界。
- 丢帧、超时、RK MPI 错误都要能观测。
- 先用 C++17 的线程、队列、状态机把模型跑稳，再考虑 C++20 coroutine。
- 优先做一条小而稳定的 pipeline，不急着铺一个完整框架。

## 核心抽象

### Frame

先从当前真正需要的 frame 类型开始：

```text
YuvFrame
  Camera / VI / VPSS / RGA 输出的原始图像帧。
  持有对应来源的 release 回调。

EncodedFrame
  H.264、H.265、JPEG、MJPEG 输出。
  等 VENC node 回来时重新引入。

TensorFrame
  NPU 输入/输出 buffer。
  等 RGA 和 VENC 稳定后再加。
```

规则：

- Frame 不可拷贝。
- 只有底层 RK 对象允许跨线程移动时，Frame 才能跨线程移动。
- CPU 可访问地址和硬件 handle 要分开暴露。
- Frame 只负责释放自己，不负责决定流向。

### Node

runtime 层的 node 应该有明确状态机：

```text
Created -> Configured -> Opened -> Started -> Stopping -> Stopped -> Closed
```

不要继续使用含糊的 `Initialize()` 语义。打开硬件、启动线程、停止线程、
关闭资源应该是不同阶段。

### Edge

edge 分两类：

```text
HardwareEdge
  VI -> VENC
  VI -> RGA
  RGA -> VENC
  通过 RK_MPI_SYS_Bind 或等价的设备侧绑定实现。

SoftwareEdge
  Camera -> JPEG
  VENC -> File
  VENC -> RTSP
  RGA -> NPU
  通过有界队列和明确 backpressure 策略实现。
```

软件队列策略：

```text
Block
DropNewest
DropOldest
LatestOnly
```

不要把无界队列作为默认选项。

### 错误

不要继续只返回 `bool`。runtime 应该有一个轻量结果类型：

```text
Result<T>
Error {
  code
  message
  rk_ret
  node_name
  operation
}
```

第一版可以很小，甚至 header-only。关键是保留 RK 返回码和失败操作。

### 指标

每个 node 和 edge 最终都应该暴露：

```text
state
input_fps
output_fps
queue_depth
dropped_frames
last_error
latency
bytes_per_second
```

只有日志不够。第一版有一个 `graph.dump_stats()` 就已经很有价值。

## 里程碑

### Milestone 1：最小采集底座

目标：

```text
Camera -> YUV file
```

只保留：

- `YuvFrame`
- `SystemManager`
- `VideoCapture`
- 一个采集示例

这是当前 reset 后的状态。

### Milestone 2：runtime 基础件

增加：

- `Result<T>`
- `CancellationToken`
- 有界 MPSC/SPSC queue
- `NodeState`
- 简单 worker thread wrapper
- stats counters

这一阶段还不做 RK MPI graph。

### Milestone 3：软件 graph

目标 API：

```cpp
auto graph = Graph::Builder()
    .node("camera", CameraNode::Config{...})
    .node("jpeg", JpegEncoderNode::Config{...})
    .node("file", FileSink::Config{...})
    .connect("camera.out", "jpeg.in", EdgePolicy::DropOldest{3})
    .connect("jpeg.out", "file.in", EdgePolicy::Block{1})
    .build();

graph.start();
graph.stop();
```

目标 pipeline：

```text
CameraNode -> JpegEncoderNode -> FileSink
```

### Milestone 4：硬件 edge

重新引入 VENC：

```text
CameraNode ==HardwareEdge==> VencNode
```

runtime 负责 bind / unbind 顺序，并把 VI channel depth 的规则显式化。

### Milestone 5：RTSP 推流

目标 pipeline：

```text
CameraNode ==HardwareEdge==> VencNode -> RtspSink
```

RTSP sink 是普通 sink node，不应该反过来驱动 graph。

### Milestone 6：优雅停止和健康状态

增加：

- 确定性的停止顺序
- 带超时的线程 join
- worker thread 错误传播
- 周期性 stats dump
- 每个 node 的 health state

### Milestone 7：RGA

增加 resize、crop、format conversion：

```text
CameraNode -> RgaNode -> JpegEncoderNode
CameraNode -> RgaNode -> NpuNode
```

### Milestone 8：NPU

增加：

- tensor buffer
- model 生命周期
- preprocess / postprocess node
- async inference scheduling

目标 pipeline：

```text
CameraNode -> RgaNode -> NpuNode -> PostProcessNode -> Sink
```

## 本次 reset 删除了什么

旧版本里有 VENC、RTSP、文件保存和类似 graph 的 `Pipeline` 类。这些代码先删掉，
因为它们在 runtime 模型清楚之前固化了太多决策。

这些能力后面应该以 runtime node 和 edge 的形式回来，而不是继续作为零散 wrapper。
