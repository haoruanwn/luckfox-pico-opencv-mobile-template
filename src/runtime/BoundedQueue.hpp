#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <stdexcept>

#include "runtime/Cancellation.hpp"
#include "runtime/Result.hpp"

namespace rmg::runtime {

    enum class QueuePolicy {
        kBlock,
        kDropNewest,
        kDropOldest,
        kLatestOnly,
    };

    struct QueueStats {
        uint64_t pushed = 0;
        uint64_t popped = 0;
        uint64_t dropped = 0;
        size_t current_depth = 0;
        size_t capacity = 0;
    };

    template<typename T>
    class BoundedQueue {
    public:
        explicit BoundedQueue(size_t capacity, QueuePolicy policy = QueuePolicy::kBlock) :
            capacity_(capacity), policy_(policy) {
            if (capacity_ == 0) {
                throw std::invalid_argument("BoundedQueue capacity must be greater than zero");
            }
        }

        BoundedQueue(const BoundedQueue &) = delete;
        BoundedQueue &operator=(const BoundedQueue &) = delete;

        [[nodiscard]] Result<void> push(T item, CancellationToken token = CancellationToken()) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (closed_) {
                return Result<void>::failure(Error::queue_closed("BoundedQueue", "push", "queue is closed"));
            }

            if (policy_ == QueuePolicy::kBlock) {
                while (!closed_ && queue_.size() >= capacity_) {
                    if (token.cancelled()) {
                        return Result<void>::failure(Error::cancelled("BoundedQueue", "push", "push was cancelled"));
                    }
                    not_full_.wait_for(lock, kCancelPollInterval);
                }

                if (closed_) {
                    return Result<void>::failure(Error::queue_closed("BoundedQueue", "push", "queue is closed"));
                }
                if (token.cancelled()) {
                    return Result<void>::failure(Error::cancelled("BoundedQueue", "push", "push was cancelled"));
                }
            } else if (queue_.size() >= capacity_) {
                if (policy_ == QueuePolicy::kDropNewest) {
                    ++stats_.dropped;
                    return Result<void>::failure(
                            Error::dropped("BoundedQueue", "push", "queue is full; newest item was dropped"));
                }

                if (policy_ == QueuePolicy::kDropOldest) {
                    queue_.pop_front();
                    ++stats_.dropped;
                }
            }

            if (policy_ == QueuePolicy::kLatestOnly && !queue_.empty()) {
                stats_.dropped += queue_.size();
                queue_.clear();
            }

            queue_.push_back(std::move(item));
            ++stats_.pushed;
            not_empty_.notify_one();
            return Result<void>::success();
        }

        [[nodiscard]] Result<T> pop(CancellationToken token = CancellationToken()) {
            std::unique_lock<std::mutex> lock(mutex_);
            while (!closed_ && queue_.empty()) {
                if (token.cancelled()) {
                    return Result<T>::failure(Error::cancelled("BoundedQueue", "pop", "pop was cancelled"));
                }
                not_empty_.wait_for(lock, kCancelPollInterval);
            }

            if (closed_ || queue_.empty()) {
                return Result<T>::failure(Error::queue_closed("BoundedQueue", "pop", "queue is closed"));
            }

            T item = std::move(queue_.front());
            queue_.pop_front();
            ++stats_.popped;
            not_full_.notify_one();
            return Result<T>::success(std::move(item));
        }

        void close() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                closed_ = true;
            }
            not_empty_.notify_all();
            not_full_.notify_all();
        }

        [[nodiscard]] bool closed() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return closed_;
        }

        [[nodiscard]] QueueStats stats() const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto stats = stats_;
            stats.current_depth = queue_.size();
            stats.capacity = capacity_;
            return stats;
        }

        [[nodiscard]] size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

        [[nodiscard]] size_t capacity() const { return capacity_; }

    private:
        static constexpr auto kCancelPollInterval = std::chrono::milliseconds(1);

        const size_t capacity_;
        const QueuePolicy policy_;
        mutable std::mutex mutex_;
        std::condition_variable not_empty_;
        std::condition_variable not_full_;
        std::deque<T> queue_;
        bool closed_ = false;
        QueueStats stats_;
    };

} // namespace rmg::runtime
