#pragma once
#include "../../core/thread_safe.hpp"
#include "blackboard.hpp"
#include "status.hpp"
#include <atomic>
#include <memory>

namespace bonsai::tree {

    class Node {
      public:
        enum class State { Idle, Running, Halted };

        Node() = default;
        virtual ~Node() = default; // FIX: Ensure virtual destructor

        // Delete copy operations
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;

        // FIX: Add move semantics
        Node(Node &&) = default;
        Node &operator=(Node &&) = default;

        virtual Status tick(Blackboard &blackboard) = 0;
        virtual void reset() { state_.store(State::Idle, std::memory_order_release); }
        virtual void halt() { state_.store(State::Halted, std::memory_order_release); }

        inline bool isHalted() const { return state_.load(std::memory_order_acquire) == State::Halted; }
        inline State state() const { return state_.load(std::memory_order_acquire); }

      protected:
        void setState(State newState) { state_.store(newState, std::memory_order_release); }

        // FIX: Make state atomic for thread safety
        std::atomic<State> state_{State::Idle};

        // FIX: Add recursion guard
        static thread_local core::ExecutionGuard recursionGuard_;
    };

    using NodePtr = std::shared_ptr<Node>;

    // Helper for move construction
    template <typename T, typename... Args> NodePtr makeNode(Args &&...args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

} // namespace bonsai::tree