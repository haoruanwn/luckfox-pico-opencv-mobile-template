/**
 * @file MediaFrame.cpp
 * @brief 媒体帧抽象 - 实现文件
 *
 * @author 好软，好温暖
 * @date 2026-01-29
 */

#include "MediaFrame.hpp"

#include <spdlog/spdlog.h>

namespace rmg {

    // ============================================================================
    // YuvFrame 实现
    // ============================================================================

    YuvFrame::YuvFrame(const VIDEO_FRAME_INFO_S &frame_info, ReleaseCallback release_cb) :
        frame_info_(frame_info), release_cb_(std::move(release_cb)) {
        is_valid_ = (frame_info_.stVFrame.pMbBlk != nullptr);
    }

    YuvFrame::~YuvFrame() {
        if (is_valid_ && release_cb_ && frame_info_.stVFrame.pMbBlk != nullptr) {
            release_cb_(&frame_info_);
        }
    }

    YuvFrame::YuvFrame(YuvFrame &&other) noexcept :
        frame_info_(other.frame_info_), release_cb_(std::move(other.release_cb_)), is_valid_(other.is_valid_) {
        other.is_valid_ = false;
        other.frame_info_.stVFrame.pMbBlk = nullptr;
        other.release_cb_ = nullptr;
    }

    YuvFrame &YuvFrame::operator=(YuvFrame &&other) noexcept {
        if (this != &other) {
            // 释放当前帧
            if (is_valid_ && release_cb_ && frame_info_.stVFrame.pMbBlk != nullptr) {
                release_cb_(&frame_info_);
            }

            // 转移所有权
            frame_info_ = other.frame_info_;
            release_cb_ = std::move(other.release_cb_);
            is_valid_ = other.is_valid_;

            other.is_valid_ = false;
            other.frame_info_.stVFrame.pMbBlk = nullptr;
            other.release_cb_ = nullptr;
        }
        return *this;
    }

    void *YuvFrame::GetVirAddr() const {
        if (!is_valid_ || frame_info_.stVFrame.pMbBlk == nullptr) {
            return nullptr;
        }

        void *vir_addr = frame_info_.stVFrame.pVirAddr[0];
        if (vir_addr == nullptr) {
            vir_addr = RK_MPI_MB_Handle2VirAddr(frame_info_.stVFrame.pMbBlk);
        }
        return vir_addr;
    }

    uint64_t YuvFrame::GetPhyAddr() const {
        if (!is_valid_ || frame_info_.stVFrame.pMbBlk == nullptr) {
            return 0;
        }
        return RK_MPI_MB_Handle2PhysAddr(frame_info_.stVFrame.pMbBlk);
    }

    size_t YuvFrame::GetDataSize() const {
        if (!is_valid_ || frame_info_.stVFrame.pMbBlk == nullptr) {
            return 0;
        }
        return static_cast<size_t>(RK_MPI_MB_GetSize(frame_info_.stVFrame.pMbBlk));
    }

} // namespace rmg
