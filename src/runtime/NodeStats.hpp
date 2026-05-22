#pragma once

#include <cstdint>

namespace rmg::runtime {

    struct NodeStats {
        uint64_t frames_read = 0;
        uint64_t timeouts = 0;
        uint64_t errors = 0;
    };

} // namespace rmg::runtime
