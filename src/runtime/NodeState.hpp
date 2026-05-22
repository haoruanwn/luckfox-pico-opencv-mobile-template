#pragma once

namespace rmg::runtime {

    enum class NodeState {
        kCreated,
        kOpened,
        kStarted,
        kStopping,
        kStopped,
        kClosed,
        kError,
    };

} // namespace rmg::runtime
