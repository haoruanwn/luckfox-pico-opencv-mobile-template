#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "Frame.hpp"
#include "runtime/Node.hpp"
#include "runtime/NodeStats.hpp"
#include "sys/MpiSystem.hpp"

namespace rmg::nodes {

    struct CropRect {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct RgaTransform {
        uint32_t dst_width = 0;
        uint32_t dst_height = 0;
        PIXEL_FORMAT_E dst_format = RK_FMT_YUV420SP;
        std::optional<CropRect> crop;
    };

    class RgaNode final : public runtime::Node {
    public:
        RgaNode() = default;
        ~RgaNode() override;

        RgaNode(const RgaNode &) = delete;
        RgaNode &operator=(const RgaNode &) = delete;
        RgaNode(RgaNode &&) = delete;
        RgaNode &operator=(RgaNode &&) = delete;

        [[nodiscard]] Result<void> open() override;
        [[nodiscard]] Result<void> start() override;
        [[nodiscard]] Result<void> stop() override;
        [[nodiscard]] Result<void> close() override;
        [[nodiscard]] runtime::NodeState state() const override { return state_.load(); }

        [[nodiscard]] Result<ImageFrame> resize_nv12(const YuvFrame &src, uint32_t dst_width, uint32_t dst_height);
        [[nodiscard]] Result<ImageFrame> to_rgb888(const YuvFrame &src, uint32_t dst_width, uint32_t dst_height);
        [[nodiscard]] Result<ImageFrame> transform(const YuvFrame &src, const RgaTransform &transform);

        [[nodiscard]] runtime::NodeStats stats() const { return stats_; }

        [[nodiscard]] static Result<void> validate_transform_parameters(uint32_t src_width, uint32_t src_height,
                                                                        PIXEL_FORMAT_E src_format,
                                                                        const RgaTransform &transform) {
            if (src_width == 0 || src_height == 0) {
                return Result<void>::failure(
                        Error::invalid_argument("RgaNode", "validate", "source dimensions must be non-zero"));
            }

            if (src_format != RK_FMT_YUV420SP) {
                return Result<void>::failure(
                        Error::invalid_argument("RgaNode", "validate", "only RK_FMT_YUV420SP input is supported"));
            }

            if (transform.dst_width == 0 || transform.dst_height == 0) {
                return Result<void>::failure(
                        Error::invalid_argument("RgaNode", "validate", "destination dimensions must be non-zero"));
            }

            if (transform.dst_width % 2 != 0 || transform.dst_height % 2 != 0) {
                return Result<void>::failure(Error::invalid_argument(
                        "RgaNode", "validate", "destination dimensions must be even for NV12-backed transforms"));
            }

            if (transform.dst_format != RK_FMT_YUV420SP && transform.dst_format != RK_FMT_RGB888) {
                return Result<void>::failure(Error::invalid_argument(
                        "RgaNode", "validate", "only RK_FMT_YUV420SP and RK_FMT_RGB888 output are supported"));
            }

            if (!transform.crop.has_value()) {
                return Result<void>::success();
            }

            const CropRect &crop = *transform.crop;
            if (crop.width == 0 || crop.height == 0) {
                return Result<void>::failure(
                        Error::invalid_argument("RgaNode", "validate", "crop dimensions must be non-zero"));
            }

            if (crop.x % 2 != 0 || crop.y % 2 != 0 || crop.width % 2 != 0 || crop.height % 2 != 0) {
                return Result<void>::failure(
                        Error::invalid_argument("RgaNode", "validate", "crop coordinates and dimensions must be even"));
            }

            const uint64_t crop_right = static_cast<uint64_t>(crop.x) + crop.width;
            const uint64_t crop_bottom = static_cast<uint64_t>(crop.y) + crop.height;
            if (crop_right > src_width || crop_bottom > src_height) {
                return Result<void>::failure(
                        Error::invalid_argument("RgaNode", "validate", "crop rectangle exceeds source dimensions"));
            }

            return Result<void>::success();
        }

    private:
        [[nodiscard]] Result<void> validate_ready(const char *operation) const;
        void record_frame_error(const Error &error);
        void record_open_error(const Error &error);

        sys::MpiSystem::Handle mpi_;
        std::atomic<runtime::NodeState> state_{runtime::NodeState::kCreated};
        runtime::NodeStats stats_;
    };

} // namespace rmg::nodes
