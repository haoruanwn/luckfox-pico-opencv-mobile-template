#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>

#include "rmg.hpp"

namespace {

    void log_error(const rmg::Error &error) {
        SPDLOG_ERROR("{}::{} failed: {} (rk_ret: 0x{:08X})", error.component, error.operation, error.message,
                     static_cast<uint32_t>(error.rk_ret));
    }

    rmg::Result<void> save_frame(const rmg::YuvFrame &frame, const std::string &path) {
        void *data = frame.data();
        if (data == nullptr) {
            return rmg::Result<void>::failure(
                rmg::Error::invalid_argument("CameraCapture", "save_frame", "frame has no CPU-visible data"));
        }

        size_t size = frame.size();
        if (size == 0) {
            size = static_cast<size_t>(frame.virtual_width()) * frame.virtual_height() * 3 / 2;
        }

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return rmg::Result<void>::failure({rmg::ErrorCode::kUnavailable, "failed to open output file: " + path,
                                               0, "CameraCapture", "save_frame"});
        }

        output.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        SPDLOG_INFO("saved {} bytes to {}", size, path);
        return rmg::Result<void>::success();
    }

    class CameraCleanup {
    public:
        explicit CameraCleanup(rmg::nodes::CameraNode &camera) : camera_(camera) {}
        ~CameraCleanup() {
            (void) camera_.stop();
            (void) camera_.close();
        }

        CameraCleanup(const CameraCleanup &) = delete;
        CameraCleanup &operator=(const CameraCleanup &) = delete;

    private:
        rmg::nodes::CameraNode &camera_;
    };

    rmg::Result<void> log_and_save_frame(rmg::YuvFrame frame, const std::string &output_path) {
        SPDLOG_INFO("captured frame: {}x{} (virtual: {}x{}), format: {}, pts: {}, size: {}", frame.width(),
                    frame.height(), frame.virtual_width(), frame.virtual_height(),
                    static_cast<int>(frame.pixel_format()), frame.pts(), frame.size());

        return save_frame(frame, output_path);
    }

} // namespace

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    std::string output_path = "frame_1920x1080_nv12.yuv";
    if (argc > 1) {
        output_path = argv[1];
    }

    rmg::nodes::CameraConfig config;
    config.width = 1920;
    config.height = 1080;
    config.iq_path = "/etc/iqfiles";
    config.dev_name = "/dev/video11";
    config.pixel_format = RK_FMT_YUV420SP;
    config.buf_count = 3;
    config.depth = 2;

    rmg::nodes::CameraNode camera(config);
    CameraCleanup cleanup(camera);

    auto result = camera.open()
                      .and_then([&] { return camera.start(); })
                      .and_then([&] { return camera.read_frame(1000); })
                      .and_then([&](rmg::YuvFrame frame) { return log_and_save_frame(std::move(frame), output_path); });

    if (!result) {
        log_error(result.error());
        return 1;
    }

    return 0;
}
