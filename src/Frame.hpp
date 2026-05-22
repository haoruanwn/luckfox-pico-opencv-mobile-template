#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

#include "rk_comm_mb.h"
#include "rk_comm_video.h"
#include "rk_mpi_mb.h"

namespace rmg {

    class YuvFrame final {
    public:
        using ReleaseCallback = std::function<void(VIDEO_FRAME_INFO_S *)>;

        YuvFrame() = default;
        YuvFrame(const VIDEO_FRAME_INFO_S &frame_info, ReleaseCallback release_cb);
        ~YuvFrame();

        YuvFrame(const YuvFrame &) = delete;
        YuvFrame &operator=(const YuvFrame &) = delete;

        YuvFrame(YuvFrame &&other) noexcept;
        YuvFrame &operator=(YuvFrame &&other) noexcept;

        [[nodiscard]] bool valid() const { return is_valid_; }
        [[nodiscard]] void *data() const;
        [[nodiscard]] uint64_t physical_address() const;
        [[nodiscard]] size_t size() const;
        [[nodiscard]] uint64_t pts() const { return frame_info_.stVFrame.u64PTS; }
        [[nodiscard]] uint32_t width() const { return frame_info_.stVFrame.u32Width; }
        [[nodiscard]] uint32_t height() const { return frame_info_.stVFrame.u32Height; }
        [[nodiscard]] uint32_t virtual_width() const { return frame_info_.stVFrame.u32VirWidth; }
        [[nodiscard]] uint32_t virtual_height() const { return frame_info_.stVFrame.u32VirHeight; }
        [[nodiscard]] PIXEL_FORMAT_E pixel_format() const { return frame_info_.stVFrame.enPixelFormat; }
        [[nodiscard]] const VIDEO_FRAME_INFO_S &frame_info() const { return frame_info_; }

    private:
        void release();

        VIDEO_FRAME_INFO_S frame_info_{};
        ReleaseCallback release_cb_;
        bool is_valid_ = false;
    };

    using OptionalYuvFrame = std::optional<YuvFrame>;

} // namespace rmg
