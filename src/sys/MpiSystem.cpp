#include "sys/MpiSystem.hpp"

#include <spdlog/spdlog.h>

#include "rk_mpi_sys.h"

namespace rmg::sys {

    std::mutex MpiSystem::mutex_;
    std::atomic<int> MpiSystem::ref_count_{0};
    bool MpiSystem::initialized_ = false;

    MpiSystem::Handle::~Handle() { release(); }

    MpiSystem::Handle::Handle(Handle &&other) noexcept : valid_(other.valid_) { other.valid_ = false; }

    MpiSystem::Handle &MpiSystem::Handle::operator=(Handle &&other) noexcept {
        if (this != &other) {
            release();
            valid_ = other.valid_;
            other.valid_ = false;
        }
        return *this;
    }

    void MpiSystem::Handle::release() {
        if (valid_) {
            MpiSystem::release();
            valid_ = false;
        }
    }

    Result<MpiSystem::Handle> MpiSystem::acquire() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (ref_count_.load() == 0) {
            SPDLOG_INFO("Initializing MPI system");
            RK_S32 ret = RK_MPI_SYS_Init();
            if (ret != RK_SUCCESS) {
                return Result<Handle>::failure(Error::rk_failure("MpiSystem", "RK_MPI_SYS_Init", ret,
                                                                 "failed to initialize RK MPI system"));
            }
            initialized_ = true;
        }

        ref_count_.fetch_add(1);
        return Result<Handle>::success(Handle(true));
    }

    bool MpiSystem::initialized() {
        std::lock_guard<std::mutex> lock(mutex_);
        return initialized_;
    }

    int MpiSystem::ref_count() { return ref_count_.load(); }

    void MpiSystem::release() {
        std::lock_guard<std::mutex> lock(mutex_);

        int previous = ref_count_.load();
        if (previous <= 0) {
            ref_count_.store(0);
            initialized_ = false;
            SPDLOG_WARN("MpiSystem release called with no active references");
            return;
        }

        ref_count_.store(previous - 1);
        if (previous == 1 && initialized_) {
            SPDLOG_INFO("Deinitializing MPI system");
            RK_MPI_SYS_Exit();
            initialized_ = false;
        }
    }

} // namespace rmg::sys
