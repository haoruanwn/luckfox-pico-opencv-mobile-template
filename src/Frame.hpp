#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

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

    class ImageFrame final {
    public:
        using ReleaseCallback = std::function<void(MB_BLK)>;

        ImageFrame() = default;
        ImageFrame(MB_BLK mb, uint32_t width, uint32_t height, uint32_t virtual_width, uint32_t virtual_height,
                   PIXEL_FORMAT_E pixel_format, uint64_t pts, ReleaseCallback release_cb) :
            mb_(mb), width_(width), height_(height), virtual_width_(virtual_width), virtual_height_(virtual_height),
            pixel_format_(pixel_format), pts_(pts), release_cb_(std::move(release_cb)), is_valid_(mb != nullptr) {}

        ~ImageFrame() { release(); }

        ImageFrame(const ImageFrame &) = delete;
        ImageFrame &operator=(const ImageFrame &) = delete;

        ImageFrame(ImageFrame &&other) noexcept :
            mb_(other.mb_), width_(other.width_), height_(other.height_), virtual_width_(other.virtual_width_),
            virtual_height_(other.virtual_height_), pixel_format_(other.pixel_format_), pts_(other.pts_),
            release_cb_(std::move(other.release_cb_)), is_valid_(other.is_valid_) {
            other.mb_ = nullptr;
            other.is_valid_ = false;
            other.release_cb_ = nullptr;
        }

        ImageFrame &operator=(ImageFrame &&other) noexcept {
            if (this != &other) {
                release();

                mb_ = other.mb_;
                width_ = other.width_;
                height_ = other.height_;
                virtual_width_ = other.virtual_width_;
                virtual_height_ = other.virtual_height_;
                pixel_format_ = other.pixel_format_;
                pts_ = other.pts_;
                release_cb_ = std::move(other.release_cb_);
                is_valid_ = other.is_valid_;

                other.mb_ = nullptr;
                other.is_valid_ = false;
                other.release_cb_ = nullptr;
            }
            return *this;
        }

        [[nodiscard]] bool valid() const { return is_valid_; }
        [[nodiscard]] void *data() const;
        [[nodiscard]] uint64_t physical_address() const;
        [[nodiscard]] int fd() const;
        [[nodiscard]] size_t size() const;
        [[nodiscard]] uint64_t pts() const { return pts_; }
        [[nodiscard]] uint32_t width() const { return width_; }
        [[nodiscard]] uint32_t height() const { return height_; }
        [[nodiscard]] uint32_t virtual_width() const { return virtual_width_; }
        [[nodiscard]] uint32_t virtual_height() const { return virtual_height_; }
        [[nodiscard]] PIXEL_FORMAT_E pixel_format() const { return pixel_format_; }
        [[nodiscard]] MB_BLK mb() const { return mb_; }

    private:
        void release() {
            if (is_valid_ && release_cb_ && mb_ != nullptr) {
                release_cb_(mb_);
            }
            mb_ = nullptr;
            is_valid_ = false;
            release_cb_ = nullptr;
        }

        MB_BLK mb_ = nullptr;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t virtual_width_ = 0;
        uint32_t virtual_height_ = 0;
        PIXEL_FORMAT_E pixel_format_ = RK_FMT_YUV420SP;
        uint64_t pts_ = 0;
        ReleaseCallback release_cb_;
        bool is_valid_ = false;
    };

} // namespace rmg
