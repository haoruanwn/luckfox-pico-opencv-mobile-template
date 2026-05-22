#include "nodes/RgaNode.hpp"

#include <cstring>
#include <string>
#include <utility>

#include "librga/im2d.h"

namespace rmg::nodes {
    namespace {

        constexpr const char *kComponent = "RgaNode";

        [[nodiscard]] bool im_status_ok(IM_STATUS status) {
            return status == IM_STATUS_SUCCESS || status == IM_STATUS_NOERROR;
        }

        [[nodiscard]] std::string im_status_message(IM_STATUS status, const char *fallback) {
            const char *message = imStrError_t(status);
            if (message == nullptr || message[0] == '\0') {
                return fallback;
            }
            return message;
        }

        [[nodiscard]] Result<int> rga_format(PIXEL_FORMAT_E format) {
            switch (format) {
            case RK_FMT_YUV420SP:
                return Result<int>::success(RK_FORMAT_YCbCr_420_SP);
            case RK_FMT_RGB888:
                return Result<int>::success(RK_FORMAT_RGB_888);
            default:
                return Result<int>::failure(Error::invalid_argument(kComponent, "rga_format", "unsupported format"));
            }
        }

        [[nodiscard]] Result<size_t> image_size(uint32_t width, uint32_t height, PIXEL_FORMAT_E format) {
            const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
            switch (format) {
            case RK_FMT_YUV420SP:
                return Result<size_t>::success(pixels * 3 / 2);
            case RK_FMT_RGB888:
                return Result<size_t>::success(pixels * 3);
            default:
                return Result<size_t>::failure(Error::invalid_argument(kComponent, "image_size", "unsupported format"));
            }
        }

        [[nodiscard]] Result<ImageFrame> allocate_image(uint32_t width, uint32_t height, PIXEL_FORMAT_E format,
                                                        uint64_t pts) {
            auto size_result = image_size(width, height, format);
            if (!size_result) {
                return Result<ImageFrame>::failure(std::move(size_result).error());
            }

            MB_POOL_CONFIG_S pool_config;
            std::memset(&pool_config, 0, sizeof(pool_config));
            pool_config.u64MBSize = size_result.value();
            pool_config.u32MBCnt = 1;
            pool_config.enAllocType = MB_ALLOC_TYPE_DMA;

            MB_POOL pool = RK_MPI_MB_CreatePool(&pool_config);
            if (pool == MB_INVALID_POOLID) {
                return Result<ImageFrame>::failure(
                        Error::rk_failure(kComponent, "RK_MPI_MB_CreatePool", -1, "failed to create output MB pool"));
            }

            MB_BLK mb = RK_MPI_MB_GetMB(pool, size_result.value(), RK_TRUE);
            if (mb == nullptr) {
                RK_MPI_MB_DestroyPool(pool);
                return Result<ImageFrame>::failure(
                        Error::rk_failure(kComponent, "RK_MPI_MB_GetMB", -1, "failed to allocate output MB block"));
            }

            auto release_cb = [pool](MB_BLK block) {
                if (block != nullptr) {
                    RK_MPI_MB_ReleaseMB(block);
                }
                RK_MPI_MB_DestroyPool(pool);
            };

            return Result<ImageFrame>::success(ImageFrame(mb, width, height, width, height, format, pts, release_cb));
        }

        [[nodiscard]] uint32_t stride_or_width(uint32_t stride, uint32_t width) {
            return stride == 0 ? width : stride;
        }

        [[nodiscard]] Result<rga_buffer_t> wrap_source(const YuvFrame &frame) {
            auto format_result = rga_format(frame.pixel_format());
            if (!format_result) {
                return Result<rga_buffer_t>::failure(std::move(format_result).error());
            }

            MB_BLK mb = frame.frame_info().stVFrame.pMbBlk;
            int fd = static_cast<int>(RK_MPI_MB_Handle2Fd(mb));
            if (fd < 0) {
                return Result<rga_buffer_t>::failure(
                        Error::rk_failure(kComponent, "RK_MPI_MB_Handle2Fd", fd, "failed to get source DMA fd"));
            }

            return Result<rga_buffer_t>::success(wrapbuffer_fd_t(
                    fd, static_cast<int>(frame.width()), static_cast<int>(frame.height()),
                    static_cast<int>(stride_or_width(frame.virtual_width(), frame.width())),
                    static_cast<int>(stride_or_width(frame.virtual_height(), frame.height())), format_result.value()));
        }

        [[nodiscard]] Result<rga_buffer_t> wrap_image(const ImageFrame &frame) {
            auto format_result = rga_format(frame.pixel_format());
            if (!format_result) {
                return Result<rga_buffer_t>::failure(std::move(format_result).error());
            }

            int fd = frame.fd();
            if (fd < 0) {
                return Result<rga_buffer_t>::failure(
                        Error::rk_failure(kComponent, "RK_MPI_MB_Handle2Fd", fd, "failed to get image DMA fd"));
            }

            return Result<rga_buffer_t>::success(wrapbuffer_fd_t(
                    fd, static_cast<int>(frame.width()), static_cast<int>(frame.height()),
                    static_cast<int>(stride_or_width(frame.virtual_width(), frame.width())),
                    static_cast<int>(stride_or_width(frame.virtual_height(), frame.height())), format_result.value()));
        }

        [[nodiscard]] Result<void> run_imcrop(const rga_buffer_t &src, ImageFrame &dst, const CropRect &crop) {
            auto dst_buffer = wrap_image(dst);
            if (!dst_buffer) {
                return Result<void>::failure(std::move(dst_buffer).error());
            }

            im_rect rect{static_cast<int>(crop.x), static_cast<int>(crop.y), static_cast<int>(crop.width),
                         static_cast<int>(crop.height)};
            IM_STATUS status = imcrop_t(src, dst_buffer.value(), rect, 1);
            if (!im_status_ok(status)) {
                return Result<void>::failure(Error::rk_failure(kComponent, "imcrop", static_cast<int32_t>(status),
                                                               im_status_message(status, "RGA crop failed")));
            }
            return Result<void>::success();
        }

        [[nodiscard]] Result<void> run_imresize(const rga_buffer_t &src, ImageFrame &dst) {
            auto dst_buffer = wrap_image(dst);
            if (!dst_buffer) {
                return Result<void>::failure(std::move(dst_buffer).error());
            }

            IM_STATUS status = imresize_t(src, dst_buffer.value(), 0, 0, INTER_LINEAR, 1);
            if (!im_status_ok(status)) {
                return Result<void>::failure(Error::rk_failure(kComponent, "imresize", static_cast<int32_t>(status),
                                                               im_status_message(status, "RGA resize failed")));
            }
            return Result<void>::success();
        }

        [[nodiscard]] Result<void> run_imcvtcolor(const rga_buffer_t &src, ImageFrame &dst, int src_format,
                                                  int dst_format) {
            auto dst_buffer = wrap_image(dst);
            if (!dst_buffer) {
                return Result<void>::failure(std::move(dst_buffer).error());
            }

            IM_STATUS status = imcvtcolor_t(src, dst_buffer.value(), src_format, dst_format,
                                            IM_YUV_TO_RGB_BT601_LIMIT, 1);
            if (!im_status_ok(status)) {
                return Result<void>::failure(Error::rk_failure(kComponent, "imcvtcolor", static_cast<int32_t>(status),
                                                               im_status_message(status, "RGA color conversion failed")));
            }
            return Result<void>::success();
        }

        [[nodiscard]] Result<void> produce_nv12(const YuvFrame &src, const RgaTransform &transform, ImageFrame &dst) {
            auto src_buffer = wrap_source(src);
            if (!src_buffer) {
                return Result<void>::failure(std::move(src_buffer).error());
            }

            if (!transform.crop.has_value()) {
                return run_imresize(src_buffer.value(), dst);
            }

            const CropRect &crop = *transform.crop;
            if (crop.width == transform.dst_width && crop.height == transform.dst_height) {
                return run_imcrop(src_buffer.value(), dst, crop);
            }

            auto cropped = allocate_image(crop.width, crop.height, RK_FMT_YUV420SP, src.pts());
            if (!cropped) {
                return Result<void>::failure(std::move(cropped).error());
            }

            auto crop_result = run_imcrop(src_buffer.value(), cropped.value(), crop);
            if (!crop_result) {
                return crop_result;
            }

            auto cropped_buffer = wrap_image(cropped.value());
            if (!cropped_buffer) {
                return Result<void>::failure(std::move(cropped_buffer).error());
            }

            return run_imresize(cropped_buffer.value(), dst);
        }

    } // namespace

    RgaNode::~RgaNode() { (void) close(); }

    Result<void> RgaNode::open() {
        auto current = state_.load();
        if (current == runtime::NodeState::kOpened || current == runtime::NodeState::kStarted) {
            return Result<void>::success();
        }

        if (current == runtime::NodeState::kStopped) {
            state_.store(runtime::NodeState::kOpened);
            return Result<void>::success();
        }

        if (current == runtime::NodeState::kError) {
            (void) close();
        }

        IM_STATUS header_status = imcheckHeader();
        if (!im_status_ok(header_status)) {
            Error error = Error::rk_failure(kComponent, "imcheckHeader", static_cast<int32_t>(header_status),
                                            im_status_message(header_status, "RGA header check failed"));
            record_open_error(error);
            return Result<void>::failure(std::move(error));
        }

        auto mpi_result = sys::MpiSystem::acquire();
        if (!mpi_result) {
            record_open_error(mpi_result.error());
            return Result<void>::failure(std::move(mpi_result).error());
        }
        mpi_ = std::move(mpi_result).value();

        state_.store(runtime::NodeState::kOpened);
        return Result<void>::success();
    }

    Result<void> RgaNode::start() {
        auto current = state_.load();
        if (current == runtime::NodeState::kStarted) {
            return Result<void>::success();
        }

        if (current != runtime::NodeState::kOpened && current != runtime::NodeState::kStopped) {
            return Result<void>::failure(
                    Error::invalid_state(kComponent, "start", "RGA node must be open before start"));
        }

        state_.store(runtime::NodeState::kStarted);
        return Result<void>::success();
    }

    Result<void> RgaNode::stop() {
        if (state_.load() == runtime::NodeState::kStarted) {
            state_.store(runtime::NodeState::kStopping);
            state_.store(runtime::NodeState::kStopped);
        }
        return Result<void>::success();
    }

    Result<void> RgaNode::close() {
        if (state_.load() == runtime::NodeState::kStarted) {
            (void) stop();
        }

        mpi_ = sys::MpiSystem::Handle();
        state_.store(runtime::NodeState::kClosed);
        return Result<void>::success();
    }

    Result<ImageFrame> RgaNode::resize_nv12(const YuvFrame &src, uint32_t dst_width, uint32_t dst_height) {
        RgaTransform transform;
        transform.dst_width = dst_width;
        transform.dst_height = dst_height;
        transform.dst_format = RK_FMT_YUV420SP;
        return this->transform(src, transform);
    }

    Result<ImageFrame> RgaNode::to_rgb888(const YuvFrame &src, uint32_t dst_width, uint32_t dst_height) {
        RgaTransform transform;
        transform.dst_width = dst_width;
        transform.dst_height = dst_height;
        transform.dst_format = RK_FMT_RGB888;
        return this->transform(src, transform);
    }

    Result<ImageFrame> RgaNode::transform(const YuvFrame &src, const RgaTransform &transform) {
        auto ready = validate_ready("transform");
        if (!ready) {
            return Result<ImageFrame>::failure(std::move(ready).error());
        }

        ++stats_.frames_in;

        if (!src.valid()) {
            Error error = Error::invalid_argument(kComponent, "transform", "source frame is not valid");
            record_frame_error(error);
            return Result<ImageFrame>::failure(std::move(error));
        }

        auto validate = validate_transform_parameters(src.width(), src.height(), src.pixel_format(), transform);
        if (!validate) {
            Error error = std::move(validate).error();
            record_frame_error(error);
            return Result<ImageFrame>::failure(std::move(error));
        }

        auto output = allocate_image(transform.dst_width, transform.dst_height, transform.dst_format, src.pts());
        if (!output) {
            Error error = std::move(output).error();
            record_frame_error(error);
            return Result<ImageFrame>::failure(std::move(error));
        }

        Result<void> operation = Result<void>::success();
        if (transform.dst_format == RK_FMT_YUV420SP) {
            operation = produce_nv12(src, transform, output.value());
        } else {
            const bool direct_convert = !transform.crop.has_value() && transform.dst_width == src.width() &&
                                        transform.dst_height == src.height();

            if (direct_convert) {
                auto src_buffer = wrap_source(src);
                if (!src_buffer) {
                    operation = Result<void>::failure(std::move(src_buffer).error());
                } else {
                    operation = run_imcvtcolor(src_buffer.value(), output.value(), RK_FORMAT_YCbCr_420_SP,
                                               RK_FORMAT_RGB_888);
                }
            } else {
                auto resized = allocate_image(transform.dst_width, transform.dst_height, RK_FMT_YUV420SP, src.pts());
                if (!resized) {
                    operation = Result<void>::failure(std::move(resized).error());
                } else {
                    RgaTransform resize_transform = transform;
                    resize_transform.dst_format = RK_FMT_YUV420SP;
                    operation = produce_nv12(src, resize_transform, resized.value());
                    if (operation) {
                        auto resized_buffer = wrap_image(resized.value());
                        if (!resized_buffer) {
                            operation = Result<void>::failure(std::move(resized_buffer).error());
                        } else {
                            operation = run_imcvtcolor(resized_buffer.value(), output.value(),
                                                       RK_FORMAT_YCbCr_420_SP, RK_FORMAT_RGB_888);
                        }
                    }
                }
            }
        }

        if (!operation) {
            Error error = std::move(operation).error();
            record_frame_error(error);
            return Result<ImageFrame>::failure(std::move(error));
        }

        ++stats_.frames_out;
        stats_.last_error.reset();
        return Result<ImageFrame>::success(std::move(output).value());
    }

    Result<void> RgaNode::validate_ready(const char *operation) const {
        if (state_.load() != runtime::NodeState::kStarted) {
            return Result<void>::failure(
                    Error::invalid_state(kComponent, operation, "RGA node must be running before transforms"));
        }
        return Result<void>::success();
    }

    void RgaNode::record_frame_error(const Error &error) {
        ++stats_.errors;
        ++stats_.dropped_frames;
        stats_.last_error = error;
    }

    void RgaNode::record_open_error(const Error &error) {
        ++stats_.errors;
        stats_.last_error = error;
        state_.store(runtime::NodeState::kError);
    }

} // namespace rmg::nodes
