#pragma once
#include "../../tree/structure/blackboard.hpp"
#include "state.hpp"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace bonsai::state {

    // Transition result types
    enum class TransitionResult {
        VALID,         // Normal transition to a new state
        EVENT_IGNORED, // Event is ignored, no state change occurs
        CANNOT_HAPPEN  // Illegal transition - triggers error
    };

    class Transition {
      public:
        using Condition = std::function<bool(tree::Blackboard &)>;
        using Action = std::function<void(tree::Blackboard &)>;

        Transition(StatePtr from, StatePtr to, Condition condition, int priority = 0)
            : from_(std::move(from)), to_(std::move(to)), condition_(std::move(condition)),
              result_(TransitionResult::VALID), priority_(priority) {}

        // Special constructor for EVENT_IGNORED and CANNOT_HAPPEN
        Transition(StatePtr from, TransitionResult result)
            : from_(std::move(from)), to_(nullptr), condition_(nullptr), result_(result) {
            if (result == TransitionResult::VALID) {
                throw std::invalid_argument("Use other constructor for VALID transitions");
            }
        }

        // Check if transition should occur
        bool shouldTransition(tree::Blackboard &blackboard) const {
            if (result_ != TransitionResult::VALID) {
                return false;
            }
            return condition_ && condition_(blackboard);
        }

        // Check transition validity
        void validate() const {
            if (result_ == TransitionResult::CANNOT_HAPPEN) {
                throw std::runtime_error("Invalid state transition: CANNOT_HAPPEN condition triggered");
            }
        }

        // Getters
        const StatePtr &from() const { return from_; }
        const StatePtr &to() const { return to_; }
        TransitionResult result() const { return result_; }
        bool isIgnored() const { return result_ == TransitionResult::EVENT_IGNORED; }
        bool cannotHappen() const { return result_ == TransitionResult::CANNOT_HAPPEN; }

        // FIX: Add priority and action support
        void setAction(Action action) { action_ = std::move(action); }
        void executeAction(tree::Blackboard &blackboard) const {
            if (action_)
                action_(blackboard);
        }
        int getPriority() const { return priority_; }

      private:
        StatePtr from_;
        StatePtr to_;
        Condition condition_;
        TransitionResult result_;
        int priority_ = 0; // FIX: Add priority for transition ordering
        Action action_;    // FIX: Add transition actions
    };

    using TransitionPtr = std::shared_ptr<Transition>;

} // namespace bonsai::state
