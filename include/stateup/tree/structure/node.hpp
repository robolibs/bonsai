#pragma once
#include "blackboard.hpp"
#include "status.hpp"
#include <atomic>
#include <memory>

namespace stateup::tree {

    class Node {
      public:
        enum class State { Idle, Running, Halted };

        Node() = default;
        virtual ~Node() = default; // FIX: Ensure virtual destructor

        // FIX: Add proper copy/move semantics
        Node(const Node &) = delete;
        Node &operator=(const Node &) = delete;
        Node(Node &&other) noexcept : state_(other.state_.load()) { other.state_.store(State::Idle); }
        Node &operator=(Node &&other) noexcept {
            if (this != &other) {
                state_.store(other.state_.load());
                other.state_.store(State::Idle);
            }
            return *this;
        }

        virtual Status tick(Blackboard &blackboard) = 0;
        virtual void reset() { state_.store(State::Idle); }
        virtual void halt() { state_.store(State::Halted); }

        inline bool isHalted() const { return state_.load(std::memory_order_acquire) == State::Halted; }
        inline State state() const { return state_.load(std::memory_order_acquire); }

      protected:
        void setState(State newState) { state_.store(newState, std::memory_order_release); }

        // FIX: Make state atomic for thread safety
        std::atomic<State> state_{State::Idle};
    };

    using NodePtr = std::shared_ptr<Node>;

} // namespace stateup::tree
