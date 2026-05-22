#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "Frame.hpp"
#include "runtime/Result.hpp"
#include "rk_comm_vi.h"
#include "rk_mpi_vi.h"

namespace rmg::sys {

    struct ViChannelConfig {
        int cam_id = 0;
        uint32_t pipe_id = 0;
        uint32_t chn_id = 0;
        uint32_t width = 1920;
        uint32_t height = 1080;
        std::string dev_name = "/dev/video11";
        PIXEL_FORMAT_E pixel_format = RK_FMT_YUV420SP;
        uint32_t buf_count = 3;
        uint32_t depth = 2;
    };

    class ViChannel {
    public:
        ViChannel() = default;
        explicit ViChannel(ViChannelConfig config) : config_(std::move(config)) {}
        ~ViChannel();

        ViChannel(const ViChannel &) = delete;
        ViChannel &operator=(const ViChannel &) = delete;
        ViChannel(ViChannel &&other) noexcept;
        ViChannel &operator=(ViChannel &&other) noexcept;

        [[nodiscard]] Result<void> open();
        [[nodiscard]] Result<YuvFrame> read_frame(int timeout_ms);
        [[nodiscard]] Result<uint32_t> current_fps() const;
        void close();

        [[nodiscard]] bool opened() const { return opened_; }
        [[nodiscard]] const ViChannelConfig &config() const { return config_; }

    private:
        [[nodiscard]] Result<void> configure_device();
        [[nodiscard]] Result<void> configure_channel();

        ViChannelConfig config_;
        bool dev_enabled_ = false;
        bool chn_enabled_ = false;
        bool opened_ = false;
    };

} // namespace rmg::sys
