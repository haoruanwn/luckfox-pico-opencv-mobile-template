#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace rmg {

    enum class ErrorCode {
        kInvalidState,
        kInvalidArgument,
        kRkApiFailed,
        kTimeout,
        kUnavailable,
        kCancelled,
        kQueueClosed,
        kQueueFull,
        kDropped,
    };

    struct Error {
        ErrorCode code = ErrorCode::kUnavailable;
        std::string message;
        int32_t rk_ret = 0;
        std::string component;
        std::string operation;

        [[nodiscard]] static Error invalid_state(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kInvalidState, std::move(message), 0, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error invalid_argument(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kInvalidArgument, std::move(message), 0, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error rk_failure(std::string component, std::string operation, int32_t rk_ret,
                                              std::string message) {
            return {ErrorCode::kRkApiFailed, std::move(message), rk_ret, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error timeout(std::string component, std::string operation, int32_t rk_ret,
                                           std::string message) {
            return {ErrorCode::kTimeout, std::move(message), rk_ret, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error unavailable(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kUnavailable, std::move(message), 0, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error cancelled(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kCancelled, std::move(message), 0, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error queue_closed(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kQueueClosed, std::move(message), 0, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error queue_full(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kQueueFull, std::move(message), 0, std::move(component), std::move(operation)};
        }

        [[nodiscard]] static Error dropped(std::string component, std::string operation, std::string message) {
            return {ErrorCode::kDropped, std::move(message), 0, std::move(component), std::move(operation)};
        }
    };

} // namespace rmg
