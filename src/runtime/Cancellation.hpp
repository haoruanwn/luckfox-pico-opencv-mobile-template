#pragma once

#include <atomic>
#include <memory>

namespace rmg::runtime {

    namespace detail {

        struct CancellationState {
            std::atomic<bool> cancelled{false};
        };

    } // namespace detail

    class CancellationToken {
    public:
        CancellationToken() = default;

        [[nodiscard]] bool cancelled() const {
            return state_ != nullptr && state_->cancelled.load(std::memory_order_acquire);
        }

    private:
        friend class CancellationSource;

        explicit CancellationToken(std::shared_ptr<detail::CancellationState> state) : state_(std::move(state)) {}

        std::shared_ptr<detail::CancellationState> state_;
    };

    class CancellationSource {
    public:
        CancellationSource() : state_(std::make_shared<detail::CancellationState>()) {}

        [[nodiscard]] CancellationToken token() const { return CancellationToken(state_); }

        [[nodiscard]] bool cancel() {
            bool expected = false;
            return state_->cancelled.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }

        [[nodiscard]] bool cancelled() const { return state_->cancelled.load(std::memory_order_acquire); }

    private:
        std::shared_ptr<detail::CancellationState> state_;
    };

} // namespace rmg::runtime
