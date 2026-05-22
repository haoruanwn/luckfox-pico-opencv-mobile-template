#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "rmg.hpp"

namespace {

    constexpr uint32_t kDstWidth = 640;
    constexpr uint32_t kDstHeight = 360;
    constexpr const char *kNv12Path = "/tmp/rga_resize_640x360_nv12.yuv";
    constexpr const char *kRgbPath = "/tmp/rga_rgb_640x360.rgb";

    struct RgaOutputs {
        rmg::ImageFrame resized;
        rmg::ImageFrame rgb;
    };

    void log_error(const rmg::Error &error) {
        SPDLOG_ERROR("{}::{} failed: {} (rk_ret: 0x{:08X})", error.component, error.operation, error.message,
                     static_cast<uint32_t>(error.rk_ret));
    }

    rmg::Result<void> log_and_propagate(rmg::Error &&error) {
        log_error(error);
        return rmg::Result<void>::failure(std::move(error));
    }

    rmg::Result<void> save_frame(const rmg::ImageFrame &frame, const std::string &path) {
        void *data = frame.data();
        if (data == nullptr) {
            return rmg::Result<void>::failure(
                    rmg::Error::invalid_argument("CameraRgaSmoke", "save_frame", "frame has no CPU-visible data"));
        }

        size_t size = frame.size();
        if (size == 0) {
            const size_t pixels = static_cast<size_t>(frame.virtual_width()) * frame.virtual_height();
            size = frame.pixel_format() == RK_FMT_RGB888 ? pixels * 3 : pixels * 3 / 2;
        }

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return rmg::Result<void>::failure(rmg::Error::unavailable(
                    "CameraRgaSmoke", "save_frame", "failed to open output file: " + path));
        }

        output.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        if (!output) {
            return rmg::Result<void>::failure(rmg::Error::unavailable(
                    "CameraRgaSmoke", "save_frame", "failed to write output file: " + path));
        }

        SPDLOG_INFO("saved {} bytes to {}", size, path);
        return rmg::Result<void>::success();
    }

    void log_captured_frame(const rmg::YuvFrame &frame) {
        SPDLOG_INFO("captured frame: {}x{} (virtual: {}x{}), format: {}, pts: {}, size: {}", frame.width(),
                    frame.height(), frame.virtual_width(), frame.virtual_height(), static_cast<int>(frame.pixel_format()),
                    frame.pts(), frame.size());
    }

    void log_outputs(const RgaOutputs &outputs, const rmg::nodes::RgaNode &rga) {
        auto stats = rga.stats();
        SPDLOG_INFO("nv12 output: {}x{}, format={}, size={}, expected={}", outputs.resized.width(),
                    outputs.resized.height(), static_cast<int>(outputs.resized.pixel_format()), outputs.resized.size(),
                    static_cast<size_t>(kDstWidth) * kDstHeight * 3 / 2);
        SPDLOG_INFO("rgb output: {}x{}, format={}, size={}, expected={}", outputs.rgb.width(), outputs.rgb.height(),
                    static_cast<int>(outputs.rgb.pixel_format()), outputs.rgb.size(),
                    static_cast<size_t>(kDstWidth) * kDstHeight * 3);
        SPDLOG_INFO("rga stats: frames_in={}, frames_out={}, dropped_frames={}, errors={}, last_error={}",
                    stats.frames_in, stats.frames_out, stats.dropped_frames, stats.errors,
                    stats.last_error.has_value() ? "set" : "none");
    }

    rmg::Result<RgaOutputs> transform_frame(rmg::nodes::RgaNode &rga, const rmg::YuvFrame &frame) {
        return rga.resize_nv12(frame, kDstWidth, kDstHeight)
                .and_then([&](rmg::ImageFrame resized) {
                    return rga.to_rgb888(frame, kDstWidth, kDstHeight)
                            .and_then([&](rmg::ImageFrame rgb) {
                                return rmg::Result<RgaOutputs>::success(
                                        RgaOutputs{std::move(resized), std::move(rgb)});
                            });
                });
    }

    rmg::Result<void> save_and_log_outputs(const RgaOutputs &outputs, const rmg::nodes::RgaNode &rga) {
        return save_frame(outputs.resized, kNv12Path)
                .and_then([&] { return save_frame(outputs.rgb, kRgbPath); })
                .and_then([&] {
                    log_outputs(outputs, rga);
                    return rmg::Result<void>::success();
                });
    }

    class RuntimeCleanup {
    public:
        RuntimeCleanup(rmg::nodes::CameraNode &camera, rmg::nodes::RgaNode &rga) : camera_(camera), rga_(rga) {}
        ~RuntimeCleanup() {
            (void) rga_.stop();
            (void) rga_.close();
            (void) camera_.stop();
            (void) camera_.close();
        }

        RuntimeCleanup(const RuntimeCleanup &) = delete;
        RuntimeCleanup &operator=(const RuntimeCleanup &) = delete;

    private:
        rmg::nodes::CameraNode &camera_;
        rmg::nodes::RgaNode &rga_;
    };

} // namespace

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    rmg::nodes::CameraConfig camera_config;
    camera_config.width = 1920;
    camera_config.height = 1080;
    camera_config.iq_path = "/etc/iqfiles";
    camera_config.dev_name = "/dev/video11";
    camera_config.pixel_format = RK_FMT_YUV420SP;
    camera_config.buf_count = 3;
    camera_config.depth = 2;

    rmg::nodes::CameraNode camera(camera_config);
    rmg::nodes::RgaNode rga;
    RuntimeCleanup cleanup(camera, rga);

    auto result = camera.open()
                          .and_then([&] { return camera.start(); })
                          .and_then([&] { return rga.open(); })
                          .and_then([&] { return rga.start(); })
                          .and_then([&] { return camera.read_frame(1000); })
                          .and_then([&](rmg::YuvFrame frame) {
                              log_captured_frame(frame);
                              return transform_frame(rga, frame);
                          })
                          .and_then([&](RgaOutputs outputs) { return save_and_log_outputs(outputs, rga); })
                          .or_else([](rmg::Error &&error) { return log_and_propagate(std::move(error)); });

    return result ? 0 : 1;
}
