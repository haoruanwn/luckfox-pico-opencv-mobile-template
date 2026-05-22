#pragma once

namespace rmg::runtime {

    enum class NodeState {
        kClosed,
        kOpen,
        kRunning,
        kError,
    };

} // namespace rmg::runtime
