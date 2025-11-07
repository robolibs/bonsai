#pragma once
#include "../../tree/structure/blackboard.hpp"
#include "state.hpp"
#include <functional>
#include <memory>
#include <string>

namespace bonsai::state {

    class Transition {
      public:
        using Condition = std::function<bool(tree::Blackboard &)>;

        Transition(StatePtr from, StatePtr to, Condition condition)
            : from_(std::move(from)), to_(std::move(to)), condition_(std::move(condition)) {}

        // Check if transition should occur
        bool shouldTransition(tree::Blackboard &blackboard) const { return condition_ && condition_(blackboard); }

        // Getters
        const StatePtr &from() const { return from_; }
        const StatePtr &to() const { return to_; }

      private:
        StatePtr from_;
        StatePtr to_;
        Condition condition_;
    };

    using TransitionPtr = std::shared_ptr<Transition>;

} // namespace bonsai::state
