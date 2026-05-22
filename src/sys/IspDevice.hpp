#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "runtime/Result.hpp"
#include "sample_comm_isp.h"

namespace rmg::sys {

    struct IspConfig {
        int cam_id = 0;
        std::string iq_path = "/etc/iqfiles";
        rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
        bool multi_cam = false;
    };

    class IspDevice {
    public:
        IspDevice() = default;
        explicit IspDevice(IspConfig config) : config_(std::move(config)) {}
        ~IspDevice();

        IspDevice(const IspDevice &) = delete;
        IspDevice &operator=(const IspDevice &) = delete;
        IspDevice(IspDevice &&other) noexcept;
        IspDevice &operator=(IspDevice &&other) noexcept;

        [[nodiscard]] Result<void> open();
        [[nodiscard]] Result<void> set_frame_rate(uint32_t fps);
        [[nodiscard]] Result<void> set_mirror_flip(bool mirror, bool flip);
        void close();

        [[nodiscard]] bool opened() const { return opened_; }
        [[nodiscard]] const IspConfig &config() const { return config_; }

    private:
        IspConfig config_;
        bool opened_ = false;
    };

} // namespace rmg::sys
