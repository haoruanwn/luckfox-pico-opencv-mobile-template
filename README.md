# RV1106 视觉 runtime playground

这个仓库是之前 `RV1106 MediaGraph` 实验的重置版。

旧版本一次性封装了摄像头采集、VENC、JPEG、文件保存、RTSP 和 pipeline
绑定，范围太大，runtime 模型还没想清楚就开始堆功能。现在仓库只保留最小
相机采集底座，把它作为边缘视觉 runtime 的第一个里程碑。

## 当前范围

当前代码：

```text
src/
  Frame           move-only 的 YuvFrame RAII 封装
  runtime/        Result、Error、Node、NodeState、NodeStats
  sys/            RK MPI / RKAIQ / VI 的薄 RAII 封装
  nodes/          CameraNode

examples/camera_capture/
  CameraCapture   打开 CameraNode，采集一帧 NV12/YUV 并保存为 .yuv 文件
```

当前保留的第三方依赖：

```text
3rdparty/luckfox_pico_rkmpi_example
3rdparty/spdlog
```

暂时删除的旧能力：

```text
VideoEncoder
RtspServer
FileSaver
Pipeline
JPEG / H.264 / RTSP examples
libdatachannel submodule
```

这些能力后面应该作为 runtime node 和 edge 回来，而不是继续以零散 wrapper
的形式堆在一起。

## 路线图

runtime 方向见：

```text
docs/runtime-roadmap.md
```

短版结构：

```text
rmg-sys       厂商 API 的薄 RAII 封装
rmg-runtime   graph、node、edge、queue、cancellation、error、metrics
rmg-nodes     camera、encoder、file、RTSP、RGA、NPU 等具体节点
```

## 构建

先初始化子模块：

```bash
git submodule update --init --recursive
```

交叉编译工具链路径在这里配置：

```text
toolchain-luckfox-pico.cmake
```

构建：

```bash
cmake --preset Debug
cmake --build build/Debug
```

采集示例输出位置：

```text
build/Debug/examples/camera_capture/CameraCapture
```

## 在设备上运行

复制二进制到 RV1106 设备后运行：

```bash
killall rkipc
./CameraCapture /tmp/frame_1920x1080_nv12.yuv
```

`rkipc` 是系统默认开机自启的相机服务，会占用 RKAIQ/VI 资源。运行本仓库的
相机示例前需要先停掉它，否则 ISP 初始化可能会阻塞在 `/tmp/aiq0.lock`。

默认摄像头假设：

```text
IQ files: /etc/iqfiles
VI device: /dev/video11
format: NV12
resolution: 1920x1080
```

查看保存的帧：

```bash
ffplay -video_size 1920x1080 -pixel_format nv12 frame_1920x1080.yuv
```
