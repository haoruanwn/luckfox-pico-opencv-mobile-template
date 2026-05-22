/**
 * @file MediaModule.hpp
 * @brief Minimal module lifecycle base
 *
 * 这是 reset 后保留下来的临时基类。后续 runtime 会用更明确的
 * NodeState / open / start / stop / close 语义替换它。
 */

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <string_view>

#include "MediaFrame.hpp"

namespace rmg {

    /**
     * @brief 模块状态枚举
     */
    enum class ModuleState {
        kUninitialized, ///< 未初始化
        kInitialized, ///< 已初始化
        kRunning, ///< 运行中
        kStopped, ///< 已停止
        kError, ///< 错误状态
    };

    /**
     * @brief YUV 帧回调类型（值语义，支持移动）
     */
    using YuvFrameCallback = std::function<void(YuvFrame)>;

    /**
     * @brief 媒体模块基类
     *
     * 当前只为 VideoCapture 保留最小生命周期接口。
     */
    class MediaModule {
    public:
        /**
         * @brief 构造函数
         * @param name 模块名称（用于日志和调试）
         */
        explicit MediaModule(std::string_view name) : name_(name) {}

        virtual ~MediaModule() = default;

        // 禁用拷贝
        MediaModule(const MediaModule &) = delete;
        MediaModule &operator=(const MediaModule &) = delete;

        MediaModule(MediaModule &&) = delete;
        MediaModule &operator=(MediaModule &&) = delete;

        /**
         * @brief 初始化模块
         * @return true 初始化成功
         * @return false 初始化失败
         */
        [[nodiscard]] virtual bool Initialize() = 0;

        /**
         * @brief 启动模块
         * @return true 启动成功
         */
        [[nodiscard]] virtual bool Start() = 0;

        /**
         * @brief 停止模块
         */
        virtual void Stop() = 0;

        /**
         * @brief 获取模块名称
         */
        [[nodiscard]] std::string_view GetName() const { return name_; }

        /**
         * @brief 获取模块状态
         */
        [[nodiscard]] ModuleState GetState() const { return state_.load(); }

        /**
         * @brief 检查模块是否正在运行
         */
        [[nodiscard]] bool IsRunning() const { return state_.load() == ModuleState::kRunning; }

    protected:
        /**
         * @brief 设置模块状态
         */
        void SetState(ModuleState state) { state_.store(state); }

        std::string name_;
        std::atomic<ModuleState> state_{ModuleState::kUninitialized};
    };

} // namespace rmg
