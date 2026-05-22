#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "runtime/Cancellation.hpp"
#include "runtime/Result.hpp"

namespace rmg::runtime {

    class WorkerThread {
    public:
        WorkerThread() = default;
        ~WorkerThread() {
            request_stop();
            if (thread_.joinable()) {
                thread_.join();
            }
        }

        WorkerThread(const WorkerThread &) = delete;
        WorkerThread &operator=(const WorkerThread &) = delete;
        WorkerThread(WorkerThread &&) = delete;
        WorkerThread &operator=(WorkerThread &&) = delete;

        template<typename F>
        [[nodiscard]] Result<void> start(std::string name, F &&fn) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (started_) {
                return Result<void>::failure(
                        Error::invalid_state("WorkerThread", "start", "worker has already been started"));
            }

            started_ = true;
            name_ = std::move(name);
            auto token = stop_source_.token();
            thread_ = std::thread([this, token, fn = std::forward<F>(fn)]() mutable { run(std::move(fn), token); });
            return Result<void>::success();
        }

        void request_stop() { (void) stop_source_.cancel(); }

        [[nodiscard]] CancellationToken token() const { return stop_source_.token(); }

        [[nodiscard]] Result<void> join(uint32_t timeout_ms) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (!started_) {
                    return Result<void>::failure(
                            Error::invalid_state("WorkerThread", "join", "worker has not been started"));
                }

                if (!finished_) {
                    auto timeout = std::chrono::milliseconds(timeout_ms);
                    if (!finished_cv_.wait_for(lock, timeout, [this] { return finished_; })) {
                        return Result<void>::failure(
                                Error::timeout("WorkerThread", "join", 0, "timed out waiting for worker to finish"));
                    }
                }
            }

            if (thread_.joinable()) {
                thread_.join();
            }
            return Result<void>::success();
        }

        [[nodiscard]] std::optional<Error> last_error() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return last_error_;
        }

        [[nodiscard]] bool finished() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return finished_;
        }

    private:
        template<typename F>
        void run(F &&fn, CancellationToken token) {
            try {
                using Ret = decltype(fn(token));
                if constexpr (std::is_same<Ret, Result<void>>::value) {
                    auto result = fn(token);
                    if (!result) {
                        set_last_error(std::move(result).error());
                    }
                } else {
                    static_assert(std::is_void<Ret>::value, "WorkerThread function must return void or Result<void>");
                    fn(token);
                }
            } catch (const std::exception &ex) {
                set_last_error(Error::unavailable(name(), "run", ex.what()));
            } catch (...) {
                set_last_error(Error::unavailable(name(), "run", "worker threw an unknown exception"));
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                finished_ = true;
            }
            finished_cv_.notify_all();
        }

        void set_last_error(Error error) {
            std::lock_guard<std::mutex> lock(mutex_);
            last_error_ = std::move(error);
        }

        [[nodiscard]] std::string name() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return name_;
        }

        CancellationSource stop_source_;
        mutable std::mutex mutex_;
        std::condition_variable finished_cv_;
        std::thread thread_;
        std::string name_ = "WorkerThread";
        bool started_ = false;
        bool finished_ = false;
        std::optional<Error> last_error_;
    };

} // namespace rmg::runtime
