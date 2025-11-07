#pragma once
#include "blackboard.hpp"
#include "status.hpp"
#include <memory>

namespace bonsai::tree {

    class Node {
      public:
        enum class State { Idle, Running, Halted };

        virtual ~Node() = default;
        virtual Status tick(Blackboard &blackboard) = 0;
        virtual void reset() { state_ = State::Idle; }
        virtual void halt() { state_ = State::Halted; }

        inline bool isHalted() const { return state_ == State::Halted; }
        inline State state() const { return state_; }

      protected:
        void setState(State newState) { state_ = newState; }

        State state_ = State::Idle;
    };

    using NodePtr = std::shared_ptr<Node>;

} // namespace bonsai::tree
