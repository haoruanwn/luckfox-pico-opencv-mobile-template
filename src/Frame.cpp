#include "Frame.hpp"

#include <utility>

#include <spdlog/spdlog.h>

namespace rmg {

    YuvFrame::YuvFrame(const VIDEO_FRAME_INFO_S &frame_info, ReleaseCallback release_cb) :
        frame_info_(frame_info), release_cb_(std::move(release_cb)) {
        is_valid_ = (frame_info_.stVFrame.pMbBlk != nullptr);
    }

    YuvFrame::~YuvFrame() { release(); }

    YuvFrame::YuvFrame(YuvFrame &&other) noexcept :
        frame_info_(other.frame_info_), release_cb_(std::move(other.release_cb_)), is_valid_(other.is_valid_) {
        other.is_valid_ = false;
        other.frame_info_.stVFrame.pMbBlk = nullptr;
        other.release_cb_ = nullptr;
    }

    YuvFrame &YuvFrame::operator=(YuvFrame &&other) noexcept {
        if (this != &other) {
            release();

            frame_info_ = other.frame_info_;
            release_cb_ = std::move(other.release_cb_);
            is_valid_ = other.is_valid_;

            other.is_valid_ = false;
            other.frame_info_.stVFrame.pMbBlk = nullptr;
            other.release_cb_ = nullptr;
        }
        return *this;
    }

    void *YuvFrame::data() const {
        if (!is_valid_ || frame_info_.stVFrame.pMbBlk == nullptr) {
            return nullptr;
        }

        void *vir_addr = frame_info_.stVFrame.pVirAddr[0];
        if (vir_addr == nullptr) {
            vir_addr = RK_MPI_MB_Handle2VirAddr(frame_info_.stVFrame.pMbBlk);
        }
        return vir_addr;
    }

    uint64_t YuvFrame::physical_address() const {
        if (!is_valid_ || frame_info_.stVFrame.pMbBlk == nullptr) {
            return 0;
        }
        return RK_MPI_MB_Handle2PhysAddr(frame_info_.stVFrame.pMbBlk);
    }

    size_t YuvFrame::size() const {
        if (!is_valid_ || frame_info_.stVFrame.pMbBlk == nullptr) {
            return 0;
        }
        return static_cast<size_t>(RK_MPI_MB_GetSize(frame_info_.stVFrame.pMbBlk));
    }

    void YuvFrame::release() {
        if (is_valid_ && release_cb_ && frame_info_.stVFrame.pMbBlk != nullptr) {
            release_cb_(&frame_info_);
        }
        is_valid_ = false;
        frame_info_.stVFrame.pMbBlk = nullptr;
        release_cb_ = nullptr;
    }

    void *ImageFrame::data() const {
        if (!is_valid_ || mb_ == nullptr) {
            return nullptr;
        }
        return RK_MPI_MB_Handle2VirAddr(mb_);
    }

    uint64_t ImageFrame::physical_address() const {
        if (!is_valid_ || mb_ == nullptr) {
            return 0;
        }
        return RK_MPI_MB_Handle2PhysAddr(mb_);
    }

    int ImageFrame::fd() const {
        if (!is_valid_ || mb_ == nullptr) {
            return -1;
        }
        return static_cast<int>(RK_MPI_MB_Handle2Fd(mb_));
    }

    size_t ImageFrame::size() const {
        if (!is_valid_ || mb_ == nullptr) {
            return 0;
        }
        return static_cast<size_t>(RK_MPI_MB_GetSize(mb_));
    }

} // namespace rmg
