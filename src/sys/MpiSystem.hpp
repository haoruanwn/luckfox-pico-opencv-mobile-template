#pragma once

#include <atomic>
#include <mutex>

#include "runtime/Result.hpp"

namespace rmg::sys {

    class MpiSystem {
    public:
        class Handle {
        public:
            Handle() = default;
            ~Handle();

            Handle(const Handle &) = delete;
            Handle &operator=(const Handle &) = delete;

            Handle(Handle &&other) noexcept;
            Handle &operator=(Handle &&other) noexcept;

            [[nodiscard]] bool valid() const { return valid_; }

        private:
            friend class MpiSystem;

            explicit Handle(bool valid) : valid_(valid) {}
            void release();

            bool valid_ = false;
        };

        [[nodiscard]] static Result<Handle> acquire();
        [[nodiscard]] static bool initialized();
        [[nodiscard]] static int ref_count();

    private:
        static void release();

        static std::mutex mutex_;
        static std::atomic<int> ref_count_;
        static bool initialized_;
    };

} // namespace rmg::sys
