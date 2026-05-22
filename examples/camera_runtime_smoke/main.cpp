#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

#include "rmg.hpp"

namespace {

    struct SmokeConfig {
        std::string output_dir = "/tmp";
        uint32_t frame_count = 3;
        int timeout_ms = 1000;
    };

    void log_error(const rmg::Error &error) {
        SPDLOG_ERROR("{}::{} failed: {} (rk_ret: 0x{:08X})", error.component, error.operation, error.message,
                     static_cast<uint32_t>(error.rk_ret));
    }

    rmg::Result<void> save_frame(const rmg::YuvFrame &frame, const std::string &path) {
        void *data = frame.data();
        if (data == nullptr) {
            return rmg::Result<void>::failure(
                    rmg::Error::invalid_argument("CameraRuntimeSmoke", "save_frame", "frame has no CPU-visible data"));
        }

        size_t size = frame.size();
        if (size == 0) {
            size = static_cast<size_t>(frame.virtual_width()) * frame.virtual_height() * 3 / 2;
        }

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return rmg::Result<void>::failure(
                    rmg::Error::unavailable("CameraRuntimeSmoke", "save_frame", "failed to open output file: " + path));
        }

        output.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        if (!output) {
            return rmg::Result<void>::failure(rmg::Error::unavailable("CameraRuntimeSmoke", "save_frame",
                                                                      "failed to write output file: " + path));
        }

        SPDLOG_INFO("saved {} bytes to {}", size, path);
        return rmg::Result<void>::success();
    }

    std::string frame_path(const SmokeConfig &config, uint32_t index) {
        std::ostringstream path;
        path << config.output_dir << "/runtime_smoke_" << index << "_1920x1080_nv12.yuv";
        return path.str();
    }

    SmokeConfig parse_args(int argc, char **argv) {
        SmokeConfig config;
        if (argc > 1) {
            config.output_dir = argv[1];
        }
        if (argc > 2) {
            int parsed = std::atoi(argv[2]);
            if (parsed > 0) {
                config.frame_count = static_cast<uint32_t>(parsed);
            }
        }
        if (argc > 3) {
            int parsed = std::atoi(argv[3]);
            if (parsed > 0) {
                config.timeout_ms = parsed;
            }
        }
        return config;
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

} // namespace

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    SmokeConfig smoke = parse_args(argc, argv);

    rmg::nodes::CameraConfig camera_config;
    camera_config.width = 1920;
    camera_config.height = 1080;
    camera_config.iq_path = "/etc/iqfiles";
    camera_config.dev_name = "/dev/video11";
    camera_config.pixel_format = RK_FMT_YUV420SP;
    camera_config.buf_count = 3;
    camera_config.depth = 2;

    rmg::nodes::CameraNode camera(camera_config);
    CameraCleanup cleanup(camera);

    rmg::runtime::BoundedQueue<rmg::YuvFrame> queue(2, rmg::runtime::QueuePolicy::kBlock);
    rmg::runtime::WorkerThread capture_worker;
    rmg::runtime::WorkerThread writer_worker;
    std::atomic<uint32_t> captured{0};
    std::atomic<uint32_t> saved{0};

    auto result = camera.open().and_then([&] { return camera.start(); });
    if (!result) {
        log_error(result.error());
        return 1;
    }

    result = writer_worker.start("runtime-smoke-writer", [&](rmg::runtime::CancellationToken token) {
        for (uint32_t i = 0; i < smoke.frame_count; ++i) {
            auto frame = queue.pop(token);
            if (!frame) {
                return rmg::Result<void>::failure(std::move(frame).error());
            }

            auto save_result = save_frame(frame.value(), frame_path(smoke, i));
            if (!save_result) {
                return save_result;
            }
            saved.fetch_add(1);
        }
        return rmg::Result<void>::success();
    });
    if (!result) {
        log_error(result.error());
        return 1;
    }

    result = capture_worker.start("runtime-smoke-capture", [&](rmg::runtime::CancellationToken token) {
        for (uint32_t i = 0; i < smoke.frame_count; ++i) {
            if (token.cancelled()) {
                return rmg::Result<void>::failure(
                        rmg::Error::cancelled("CameraRuntimeSmoke", "capture", "capture was cancelled"));
            }

            auto frame = camera.read_frame(smoke.timeout_ms);
            if (!frame) {
                return rmg::Result<void>::failure(std::move(frame).error());
            }

            SPDLOG_INFO("captured frame {}: {}x{} pts={} size={}", i, frame.value().width(), frame.value().height(),
                        frame.value().pts(), frame.value().size());

            auto push_result = queue.push(std::move(frame).value(), token);
            if (!push_result) {
                return push_result;
            }
            captured.fetch_add(1);
        }
        return rmg::Result<void>::success();
    });
    if (!result) {
        writer_worker.request_stop();
        queue.close();
        (void) writer_worker.join(2000);
        log_error(result.error());
        return 1;
    }

    auto capture_join = capture_worker.join(static_cast<uint32_t>(smoke.frame_count * smoke.timeout_ms + 3000));
    if (!capture_join || capture_worker.last_error().has_value()) {
        writer_worker.request_stop();
        queue.close();
        (void) writer_worker.join(2000);
        if (!capture_join) {
            log_error(capture_join.error());
        } else {
            log_error(*capture_worker.last_error());
        }
        return 1;
    }

    auto writer_join = writer_worker.join(5000);
    if (!writer_join || writer_worker.last_error().has_value()) {
        queue.close();
        if (!writer_join) {
            log_error(writer_join.error());
        } else {
            log_error(*writer_worker.last_error());
        }
        return 1;
    }

    auto queue_stats = queue.stats();
    auto camera_stats = camera.stats();
    SPDLOG_INFO("runtime smoke complete: captured={}, saved={}, queue pushed={}, popped={}, dropped={}, camera "
                "frames_out={}",
                captured.load(), saved.load(), queue_stats.pushed, queue_stats.popped, queue_stats.dropped,
                camera_stats.frames_out);

    return 0;
}
