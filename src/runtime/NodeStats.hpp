#pragma once

#include <cstdint>
#include <optional>

#include "runtime/Error.hpp"

namespace rmg::runtime {

    struct NodeStats {
        uint64_t frames_in = 0;
        uint64_t frames_out = 0;
        uint64_t dropped_frames = 0;
        std::optional<Error> last_error;

        uint64_t frames_read = 0;
        uint64_t timeouts = 0;
        uint64_t errors = 0;
    };

} // namespace rmg::runtime
