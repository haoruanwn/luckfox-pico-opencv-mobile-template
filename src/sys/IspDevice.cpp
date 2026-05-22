#include "sys/IspDevice.hpp"

#include <utility>

#include <spdlog/spdlog.h>

namespace rmg::sys {

    IspDevice::~IspDevice() { close(); }

    IspDevice::IspDevice(IspDevice &&other) noexcept : config_(std::move(other.config_)), opened_(other.opened_) {
        other.opened_ = false;
    }

    IspDevice &IspDevice::operator=(IspDevice &&other) noexcept {
        if (this != &other) {
            close();
            config_ = std::move(other.config_);
            opened_ = other.opened_;
            other.opened_ = false;
        }
        return *this;
    }

    Result<void> IspDevice::open() {
        if (opened_) {
            return Result<void>::success();
        }

        SPDLOG_INFO("Initializing ISP (cam_id: {}, iq_path: {})", config_.cam_id, config_.iq_path);

        RK_S32 ret = SAMPLE_COMM_ISP_Init(config_.cam_id, config_.hdr_mode, config_.multi_cam ? RK_TRUE : RK_FALSE,
                                          config_.iq_path.c_str());
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("IspDevice", "SAMPLE_COMM_ISP_Init", ret, "failed to initialize ISP"));
        }

        ret = SAMPLE_COMM_ISP_Run(config_.cam_id);
        if (ret != RK_SUCCESS) {
            SAMPLE_COMM_ISP_Stop(config_.cam_id);
            return Result<void>::failure(
                Error::rk_failure("IspDevice", "SAMPLE_COMM_ISP_Run", ret, "failed to start ISP"));
        }

        opened_ = true;
        return Result<void>::success();
    }

    Result<void> IspDevice::set_frame_rate(uint32_t fps) {
        if (!opened_) {
            return Result<void>::failure(
                Error::invalid_state("IspDevice", "set_frame_rate", "ISP is not open"));
        }

        RK_S32 ret = SAMPLE_COMM_ISP_SetFrameRate(config_.cam_id, fps);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("IspDevice", "SAMPLE_COMM_ISP_SetFrameRate", ret, "failed to set frame rate"));
        }
        return Result<void>::success();
    }

    Result<void> IspDevice::set_mirror_flip(bool mirror, bool flip) {
        if (!opened_) {
            return Result<void>::failure(
                Error::invalid_state("IspDevice", "set_mirror_flip", "ISP is not open"));
        }

        RK_S32 ret = SAMPLE_COMM_ISP_SetMirrorFlip(config_.cam_id, mirror ? 1 : 0, flip ? 1 : 0);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("IspDevice", "SAMPLE_COMM_ISP_SetMirrorFlip", ret, "failed to set mirror/flip"));
        }
        return Result<void>::success();
    }

    void IspDevice::close() {
        if (!opened_) {
            return;
        }

        SPDLOG_INFO("Stopping ISP (cam_id: {})", config_.cam_id);
        SAMPLE_COMM_ISP_Stop(config_.cam_id);
        opened_ = false;
    }

} // namespace rmg::sys
