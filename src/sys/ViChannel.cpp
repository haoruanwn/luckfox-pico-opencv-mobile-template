#include "sys/ViChannel.hpp"

#include <cstring>
#include <utility>

#include <spdlog/spdlog.h>

namespace rmg::sys {

    ViChannel::~ViChannel() { close(); }

    ViChannel::ViChannel(ViChannel &&other) noexcept :
        config_(std::move(other.config_)), dev_enabled_(other.dev_enabled_), chn_enabled_(other.chn_enabled_),
        opened_(other.opened_) {
        other.dev_enabled_ = false;
        other.chn_enabled_ = false;
        other.opened_ = false;
    }

    ViChannel &ViChannel::operator=(ViChannel &&other) noexcept {
        if (this != &other) {
            close();
            config_ = std::move(other.config_);
            dev_enabled_ = other.dev_enabled_;
            chn_enabled_ = other.chn_enabled_;
            opened_ = other.opened_;
            other.dev_enabled_ = false;
            other.chn_enabled_ = false;
            other.opened_ = false;
        }
        return *this;
    }

    Result<void> ViChannel::open() {
        if (opened_) {
            return Result<void>::success();
        }

        auto dev_result = configure_device();
        if (!dev_result) {
            close();
            return dev_result;
        }

        auto chn_result = configure_channel();
        if (!chn_result) {
            close();
            return chn_result;
        }

        opened_ = true;
        return Result<void>::success();
    }

    Result<YuvFrame> ViChannel::read_frame(int timeout_ms) {
        if (!opened_) {
            return Result<YuvFrame>::failure(
                Error::invalid_state("ViChannel", "RK_MPI_VI_GetChnFrame", "VI channel is not open"));
        }

        VIDEO_FRAME_INFO_S frame_info;
        std::memset(&frame_info, 0, sizeof(frame_info));

        RK_S32 ret = RK_MPI_VI_GetChnFrame(config_.pipe_id, config_.chn_id, &frame_info, timeout_ms);
        if (ret != RK_SUCCESS) {
            if (ret == RK_ERR_VI_BUF_EMPTY) {
                return Result<YuvFrame>::failure(Error::timeout("ViChannel", "RK_MPI_VI_GetChnFrame", ret,
                                                               "timed out waiting for VI frame"));
            }
            return Result<YuvFrame>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_GetChnFrame", ret, "failed to read VI frame"));
        }

        uint32_t pipe_id = config_.pipe_id;
        uint32_t chn_id = config_.chn_id;
        auto release_cb = [pipe_id, chn_id](VIDEO_FRAME_INFO_S *frame) {
            RK_S32 release_ret = RK_MPI_VI_ReleaseChnFrame(pipe_id, chn_id, frame);
            if (release_ret != RK_SUCCESS) {
                SPDLOG_WARN("RK_MPI_VI_ReleaseChnFrame failed: 0x{:08X}", release_ret);
            }
        };

        return Result<YuvFrame>::success(YuvFrame(frame_info, release_cb));
    }

    Result<uint32_t> ViChannel::current_fps() const {
        if (!opened_) {
            return Result<uint32_t>::failure(
                Error::invalid_state("ViChannel", "RK_MPI_VI_QueryChnStatus", "VI channel is not open"));
        }

        VI_CHN_STATUS_S chn_status;
        std::memset(&chn_status, 0, sizeof(chn_status));

        RK_S32 ret = RK_MPI_VI_QueryChnStatus(config_.pipe_id, config_.chn_id, &chn_status);
        if (ret != RK_SUCCESS) {
            return Result<uint32_t>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_QueryChnStatus", ret, "failed to query VI channel status"));
        }

        return Result<uint32_t>::success(chn_status.u32FrameRate);
    }

    void ViChannel::close() {
        if (chn_enabled_) {
            RK_MPI_VI_DisableChn(config_.pipe_id, config_.chn_id);
            chn_enabled_ = false;
        }

        if (dev_enabled_) {
            RK_MPI_VI_DisableDev(config_.cam_id);
            dev_enabled_ = false;
        }

        opened_ = false;
    }

    Result<void> ViChannel::configure_device() {
        VI_DEV_ATTR_S dev_attr;
        std::memset(&dev_attr, 0, sizeof(dev_attr));
        dev_attr.stMaxSize.u32Width = config_.width;
        dev_attr.stMaxSize.u32Height = config_.height;
        dev_attr.enPixFmt = config_.pixel_format;
        dev_attr.enBufType = VI_V4L2_MEMORY_TYPE_DMABUF;
        dev_attr.u32BufCount = config_.buf_count;

        RK_S32 ret = RK_MPI_VI_SetDevAttr(config_.cam_id, &dev_attr);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_SetDevAttr", ret, "failed to set VI device attributes"));
        }

        ret = RK_MPI_VI_EnableDev(config_.cam_id);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_EnableDev", ret, "failed to enable VI device"));
        }
        dev_enabled_ = true;

        VI_DEV_BIND_PIPE_S bind_pipe;
        std::memset(&bind_pipe, 0, sizeof(bind_pipe));
        bind_pipe.u32Num = 1;
        bind_pipe.PipeId[0] = config_.pipe_id;

        ret = RK_MPI_VI_SetDevBindPipe(config_.cam_id, &bind_pipe);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_SetDevBindPipe", ret, "failed to bind VI device to pipe"));
        }

        return Result<void>::success();
    }

    Result<void> ViChannel::configure_channel() {
        VI_CHN_ATTR_S chn_attr;
        std::memset(&chn_attr, 0, sizeof(chn_attr));

        chn_attr.stFrameRate.s32SrcFrameRate = -1;
        chn_attr.stFrameRate.s32DstFrameRate = -1;
        chn_attr.stSize.u32Width = config_.width;
        chn_attr.stSize.u32Height = config_.height;
        chn_attr.enPixelFormat = config_.pixel_format;
        chn_attr.u32Depth = config_.depth;
        chn_attr.stIspOpt.u32BufCount = config_.buf_count;
        chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        chn_attr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
        chn_attr.stIspOpt.stMaxSize.u32Width = config_.width;
        chn_attr.stIspOpt.stMaxSize.u32Height = config_.height;
        std::strncpy(reinterpret_cast<char *>(chn_attr.stIspOpt.aEntityName), config_.dev_name.c_str(),
                     MAX_VI_ENTITY_NAME_LEN - 1);

        RK_S32 ret = RK_MPI_VI_SetChnAttr(config_.pipe_id, config_.chn_id, &chn_attr);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_SetChnAttr", ret, "failed to set VI channel attributes"));
        }

        ret = RK_MPI_VI_EnableChn(config_.pipe_id, config_.chn_id);
        if (ret != RK_SUCCESS) {
            return Result<void>::failure(
                Error::rk_failure("ViChannel", "RK_MPI_VI_EnableChn", ret, "failed to enable VI channel"));
        }
        chn_enabled_ = true;

        return Result<void>::success();
    }

} // namespace rmg::sys
