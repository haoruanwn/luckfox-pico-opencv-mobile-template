/**
 * @file MediaFrame.hpp
 * @brief YUV frame RAII wrapper
 *
 * 当前 reset 只保留相机采集所需的 YuvFrame。EncodedFrame 和统一
 * MediaFrame variant 会在 VENC runtime node 回来时重新设计。
 */

#pragma once

#include <cstdint>
#include <functional>
#include <optional>

// Rockchip MPI headers
#include "rk_comm_mb.h"
#include "rk_comm_video.h"
#include "rk_mpi_mb.h"

namespace rmg { // RV1106 MediaGraph

    /**
     * @brief 帧类型枚举
     */
    enum class FrameType {
        kYuv, ///< YUV 原始帧
    };

    // ============================================================================
    // YuvFrame - YUV 原始帧
    // ============================================================================

    /**
     * @brief YUV 帧类型
     *
     * 封装 VIDEO_FRAME_INFO_S，支持从 VI/VPSS 获取的 YUV 数据。
     * 使用 RAII 确保帧资源在对象析构时正确释放。
     */
    class YuvFrame final {
    public:
        /**
         * @brief 帧释放回调类型
         *
         * 用于自定义帧的释放方式（VI/VPSS/RGA 等不同来源的帧释放方式不同）
         */
        using ReleaseCallback = std::function<void(VIDEO_FRAME_INFO_S *)>;

        /**
         * @brief 默认构造函数（创建无效帧）
         */
        YuvFrame() = default;

        /**
         * @brief 构造函数
         * @param frame_info 帧信息结构体
         * @param release_cb 释放回调函数
         */
        YuvFrame(const VIDEO_FRAME_INFO_S &frame_info, ReleaseCallback release_cb);

        /**
         * @brief 析构函数 - 自动释放帧资源
         */
        ~YuvFrame();

        // 禁用拷贝
        YuvFrame(const YuvFrame &) = delete;
        YuvFrame &operator=(const YuvFrame &) = delete;

        // 移动语义
        YuvFrame(YuvFrame &&other) noexcept;
        YuvFrame &operator=(YuvFrame &&other) noexcept;

        /**
         * @brief 获取帧类型
         */
        [[nodiscard]] static constexpr FrameType GetType() { return FrameType::kYuv; }

        /**
         * @brief 获取帧的虚拟地址（CPU 可访问）
         * @return 虚拟地址指针，失败返回 nullptr
         */
        [[nodiscard]] void *GetVirAddr() const;

        /**
         * @brief 获取帧的物理地址（硬件加速器使用）
         * @return 物理地址
         */
        [[nodiscard]] uint64_t GetPhyAddr() const;

        /**
         * @brief 获取帧数据大小
         * @return 数据大小（字节）
         */
        [[nodiscard]] size_t GetDataSize() const;

        /**
         * @brief 获取时间戳
         */
        [[nodiscard]] uint64_t GetPts() const { return frame_info_.stVFrame.u64PTS; }

        /**
         * @brief 检查帧是否有效
         */
        [[nodiscard]] bool IsValid() const { return is_valid_; }

        /**
         * @brief 获取帧宽度
         */
        [[nodiscard]] uint32_t GetWidth() const { return frame_info_.stVFrame.u32Width; }

        /**
         * @brief 获取帧高度
         */
        [[nodiscard]] uint32_t GetHeight() const { return frame_info_.stVFrame.u32Height; }

        /**
         * @brief 获取虚拟宽度（对齐后的宽度）
         */
        [[nodiscard]] uint32_t GetVirWidth() const { return frame_info_.stVFrame.u32VirWidth; }

        /**
         * @brief 获取虚拟高度（对齐后的高度）
         */
        [[nodiscard]] uint32_t GetVirHeight() const { return frame_info_.stVFrame.u32VirHeight; }

        /**
         * @brief 获取像素格式
         */
        [[nodiscard]] PIXEL_FORMAT_E GetPixelFormat() const { return frame_info_.stVFrame.enPixelFormat; }

        /**
         * @brief 获取原始帧信息结构体引用（用于硬件绑定等场景）
         */
        [[nodiscard]] const VIDEO_FRAME_INFO_S &GetFrameInfo() const { return frame_info_; }

    private:
        VIDEO_FRAME_INFO_S frame_info_{};
        ReleaseCallback release_cb_;
        bool is_valid_ = false;
    };

    /**
     * @brief 可选 YUV 帧
     */
    using OptionalYuvFrame = std::optional<YuvFrame>;

} // namespace rmg
