#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <thread>

#include "runtime/BoundedQueue.hpp"
#include "runtime/Cancellation.hpp"
#include "runtime/Node.hpp"
#include "runtime/WorkerThread.hpp"

namespace {

    using rmg::Error;
    using rmg::ErrorCode;
    using rmg::Result;
    using rmg::runtime::BoundedQueue;
    using rmg::runtime::CancellationSource;
    using rmg::runtime::Node;
    using rmg::runtime::NodeState;
    using rmg::runtime::QueuePolicy;
    using rmg::runtime::WorkerThread;

    void test_result_and_then() {
        auto result = Result<int>::success(2).and_then([](int value) { return Result<int>::success(value + 3); });
        assert(result);
        assert(result.value() == 5);

        const auto const_result = Result<int>::success(4);
        auto const_chain = const_result.and_then([](const int &value) { return Result<int>::success(value * 2); });
        assert(const_chain);
        assert(const_chain.value() == 8);

        bool called = false;
        auto failed = Result<int>::failure(Error::invalid_state("test", "first", "failed")).and_then([&](int) {
            called = true;
            return Result<int>::success(7);
        });
        assert(!failed);
        assert(!called);
        assert(failed.error().code == ErrorCode::kInvalidState);

        auto moved = Result<std::unique_ptr<int>>::success(std::make_unique<int>(9))
                             .and_then([](std::unique_ptr<int> value) { return Result<int>::success(*value); });
        assert(moved);
        assert(moved.value() == 9);
    }

    void test_result_error_helpers() {
        auto recovered =
                Result<int>::failure(Error::invalid_state("test", "recover", "failed")).or_else([](Error &&error) {
                    assert(error.code == ErrorCode::kInvalidState);
                    return Result<int>::success(11);
                });
        assert(recovered);
        assert(recovered.value() == 11);

        auto move_only = Result<std::unique_ptr<int>>::success(std::make_unique<int>(5));
        auto still_ok = std::move(move_only).or_else(
                [](Error &&) { return Result<std::unique_ptr<int>>::success(std::make_unique<int>(0)); });
        assert(still_ok);
        assert(*still_ok.value() == 5);

        bool called = false;
        auto ok_void = Result<void>::success();
        auto still_success = ok_void.or_else([&](Error &) {
            called = true;
            return Result<void>::failure(Error::unavailable("test", "or_else", "should not run"));
        });
        assert(still_success);
        assert(!called);

        auto recovered_void =
                Result<void>::failure(Error::cancelled("test", "void", "cancelled")).or_else([](Error &&error) {
                    assert(error.code == ErrorCode::kCancelled);
                    return Result<void>::success();
                });
        assert(recovered_void);

        auto mutable_error = Result<void>::failure(Error::unavailable("test", "error", "original"));
        mutable_error.error().message = "updated";
        assert(mutable_error.error().message == "updated");

        auto fallback = Error::unavailable("test", "fallback", "fallback");
        assert(Result<void>::success().error_or(fallback).message == "fallback");
        auto stored = Result<void>::failure(Error::queue_closed("test", "stored", "stored")).error_or(fallback);
        assert(stored.code == ErrorCode::kQueueClosed);
        assert(stored.message == "stored");
    }

    void test_cancellation_token() {
        CancellationSource source;
        auto token = source.token();
        assert(!token.cancelled());
        assert(source.cancel());
        assert(token.cancelled());
        assert(source.cancelled());
        assert(!source.cancel());
        assert(token.cancelled());
    }

    void test_queue_drop_newest() {
        BoundedQueue<int> queue(1, QueuePolicy::kDropNewest);
        assert(queue.push(1));
        auto dropped = queue.push(2);
        assert(!dropped);
        assert(dropped.error().code == ErrorCode::kDropped);

        auto item = queue.pop();
        assert(item);
        assert(item.value() == 1);
        auto stats = queue.stats();
        assert(stats.pushed == 1);
        assert(stats.popped == 1);
        assert(stats.dropped == 1);
        assert(stats.current_depth == 0);
        assert(stats.capacity == 1);
    }

    void test_queue_drop_oldest() {
        BoundedQueue<int> queue(2, QueuePolicy::kDropOldest);
        assert(queue.push(1));
        assert(queue.push(2));
        assert(queue.push(3));
        assert(queue.pop().value() == 2);
        assert(queue.pop().value() == 3);
        assert(queue.stats().dropped == 1);
    }

    void test_queue_latest_only() {
        BoundedQueue<int> queue(3, QueuePolicy::kLatestOnly);
        assert(queue.push(1));
        assert(queue.push(2));
        assert(queue.push(3));
        assert(queue.pop().value() == 3);
        assert(queue.stats().dropped == 2);
    }

    void test_queue_move_only() {
        BoundedQueue<std::unique_ptr<int>> queue(1);
        assert(queue.push(std::make_unique<int>(42)));
        auto item = queue.pop();
        assert(item);
        assert(*item.value() == 42);
    }

    void test_queue_close_wakes() {
        BoundedQueue<int> queue(1);
        std::atomic<bool> done{false};
        std::thread waiter([&] {
            auto result = queue.pop();
            assert(!result);
            assert(result.error().code == ErrorCode::kQueueClosed);
            done.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        queue.close();
        waiter.join();
        assert(done.load());
        assert(!queue.push(1));

        BoundedQueue<int> populated(1);
        assert(populated.push(1));
        populated.close();
        auto after_close = populated.pop();
        assert(!after_close);
        assert(after_close.error().code == ErrorCode::kQueueClosed);
    }

    void test_queue_cancel_wakes() {
        BoundedQueue<int> queue(1);
        CancellationSource source;
        std::atomic<bool> done{false};
        std::thread waiter([&] {
            auto result = queue.pop(source.token());
            assert(!result);
            assert(result.error().code == ErrorCode::kCancelled);
            done.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        (void) source.cancel();
        waiter.join();
        assert(done.load());
    }

    void test_queue_block_policy() {
        BoundedQueue<int> queue(1, QueuePolicy::kBlock);
        assert(queue.push(1));

        std::atomic<bool> pushed{false};
        std::thread producer([&] {
            auto result = queue.push(2);
            assert(result);
            pushed.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(!pushed.load());
        assert(queue.pop().value() == 1);
        producer.join();
        assert(pushed.load());
        assert(queue.pop().value() == 2);
    }

    void test_worker_thread_success() {
        WorkerThread worker;
        std::atomic<bool> ran{false};
        assert(worker.start("success", [&](auto) {
            ran.store(true);
            return Result<void>::success();
        }));
        assert(worker.join(1000));
        assert(ran.load());
        assert(!worker.last_error().has_value());
        assert(!worker.start("again", [](auto) {}));
    }

    void test_worker_thread_cancel() {
        WorkerThread worker;
        assert(worker.start("cancel", [](auto token) {
            while (!token.cancelled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }));
        worker.request_stop();
        assert(worker.join(1000));
        assert(!worker.last_error().has_value());
    }

    void test_worker_thread_error_and_timeout() {
        WorkerThread error_worker;
        assert(error_worker.start(
                "error", [](auto) { return Result<void>::failure(Error::cancelled("test", "worker", "cancelled")); }));
        assert(error_worker.join(1000));
        assert(error_worker.last_error().has_value());
        assert(error_worker.last_error()->code == ErrorCode::kCancelled);

        WorkerThread slow_worker;
        assert(slow_worker.start("slow", [](auto token) {
            while (!token.cancelled()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }));
        auto timed_out = slow_worker.join(1);
        assert(!timed_out);
        assert(timed_out.error().code == ErrorCode::kTimeout);
        slow_worker.request_stop();
        assert(slow_worker.join(1000));
    }

    class FakeNode final : public Node {
    public:
        Result<void> open() override {
            if (state_ == NodeState::kOpened || state_ == NodeState::kStarted) {
                return Result<void>::success();
            }
            state_ = NodeState::kOpened;
            return Result<void>::success();
        }

        Result<void> start() override {
            if (state_ == NodeState::kStarted) {
                return Result<void>::success();
            }
            if (state_ != NodeState::kOpened && state_ != NodeState::kStopped) {
                return Result<void>::failure(Error::invalid_state("FakeNode", "start", "not open"));
            }
            state_ = NodeState::kStarted;
            return Result<void>::success();
        }

        Result<void> stop() override {
            if (state_ == NodeState::kStarted) {
                state_ = NodeState::kStopping;
                state_ = NodeState::kStopped;
            }
            return Result<void>::success();
        }

        Result<void> close() override {
            if (state_ == NodeState::kStarted) {
                (void) stop();
            }
            state_ = NodeState::kClosed;
            return Result<void>::success();
        }

        NodeState state() const override { return state_; }

    private:
        NodeState state_ = NodeState::kCreated;
    };

    void test_fake_node_state_machine() {
        FakeNode node;
        assert(node.state() == NodeState::kCreated);
        assert(!node.start());
        assert(node.open());
        assert(node.state() == NodeState::kOpened);
        assert(node.open());
        assert(node.start());
        assert(node.state() == NodeState::kStarted);
        assert(node.start());
        assert(node.stop());
        assert(node.state() == NodeState::kStopped);
        assert(node.stop());
        assert(node.close());
        assert(node.state() == NodeState::kClosed);
        assert(node.close());
    }

} // namespace

int main() {
    test_result_and_then();
    test_result_error_helpers();
    test_cancellation_token();
    test_queue_drop_newest();
    test_queue_drop_oldest();
    test_queue_latest_only();
    test_queue_move_only();
    test_queue_close_wakes();
    test_queue_cancel_wakes();
    test_queue_block_policy();
    test_worker_thread_success();
    test_worker_thread_cancel();
    test_worker_thread_error_and_timeout();
    test_fake_node_state_machine();
    return 0;
}
