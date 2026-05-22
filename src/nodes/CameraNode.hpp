#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "Frame.hpp"
#include "runtime/Node.hpp"
#include "runtime/NodeStats.hpp"
#include "sys/IspDevice.hpp"
#include "sys/MpiSystem.hpp"
#include "sys/ViChannel.hpp"

namespace rmg::nodes {

    struct CameraConfig {
        int cam_id = 0;
        uint32_t pipe_id = 0;
        uint32_t chn_id = 0;
        uint32_t width = 1920;
        uint32_t height = 1080;
        std::string iq_path = "/etc/iqfiles";
        std::string dev_name = "/dev/video11";
        PIXEL_FORMAT_E pixel_format = RK_FMT_YUV420SP;
        uint32_t buf_count = 3;
        uint32_t depth = 2;
        rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
        bool multi_cam = false;
    };

    class CameraNode final : public runtime::Node {
    public:
        explicit CameraNode(CameraConfig config);
        ~CameraNode() override;

        CameraNode(const CameraNode &) = delete;
        CameraNode &operator=(const CameraNode &) = delete;
        CameraNode(CameraNode &&) = delete;
        CameraNode &operator=(CameraNode &&) = delete;

        [[nodiscard]] Result<void> open() override;
        [[nodiscard]] Result<void> start() override;
        [[nodiscard]] Result<void> stop() override;
        [[nodiscard]] Result<void> close() override;
        [[nodiscard]] runtime::NodeState state() const override { return state_.load(); }

        [[nodiscard]] Result<YuvFrame> read_frame(int timeout_ms = 1000);
        [[nodiscard]] Result<uint32_t> current_fps() const;
        [[nodiscard]] Result<void> set_frame_rate(uint32_t fps);
        [[nodiscard]] Result<void> set_mirror_flip(bool mirror, bool flip);

        [[nodiscard]] const CameraConfig &config() const { return config_; }
        [[nodiscard]] runtime::NodeStats stats() const { return stats_; }

    private:
        [[nodiscard]] sys::IspConfig isp_config() const;
        [[nodiscard]] sys::ViChannelConfig vi_config() const;
        void record_error(const Error &error);

        CameraConfig config_;
        sys::MpiSystem::Handle mpi_;
        sys::IspDevice isp_;
        sys::ViChannel vi_;
        std::atomic<runtime::NodeState> state_{runtime::NodeState::kCreated};
        runtime::NodeStats stats_;
    };

} // namespace rmg::nodes
