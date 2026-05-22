#pragma once

#include "runtime/NodeState.hpp"
#include "runtime/Result.hpp"

namespace rmg::runtime {

    class Node {
    public:
        virtual ~Node() = default;

        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;
        Node(Node &&) = delete;
        Node &operator=(Node &&) = delete;

        [[nodiscard]] virtual Result<void> open() = 0;
        [[nodiscard]] virtual Result<void> start() = 0;
        [[nodiscard]] virtual Result<void> stop() = 0;
        [[nodiscard]] virtual Result<void> close() = 0;
        [[nodiscard]] virtual NodeState state() const = 0;

    protected:
        Node() = default;
    };

} // namespace rmg::runtime
