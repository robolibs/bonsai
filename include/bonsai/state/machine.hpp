#pragma once
#include "../tree/structure/blackboard.hpp"
#include "structure/state.hpp"
#include "structure/transition.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace bonsai::state {

    class StateMachine {
      public:
        StateMachine() = default;
        explicit StateMachine(StatePtr initialState);

        // Add states and transitions
        void addState(StatePtr state);
        void addTransition(TransitionPtr transition);
        void addTransition(const StatePtr &from, const StatePtr &to, Transition::Condition condition);

        // Set initial state
        void setInitialState(StatePtr state);

        // Update the state machine
        void tick();
        void update() { tick(); } // Alias for tick

        // State management
        void reset();
        StatePtr getCurrentState() const { return currentState_; }
        const std::string &getCurrentStateName() const;

        // Blackboard access
        tree::Blackboard &blackboard() { return blackboard_; }
        const tree::Blackboard &blackboard() const { return blackboard_; }

      private:
        void transitionTo(const StatePtr &newState);
        std::vector<TransitionPtr> getTransitionsFrom(const StatePtr &state) const;

        StatePtr initialState_;
        StatePtr currentState_;
        std::unordered_map<std::string, StatePtr> states_;
        std::vector<TransitionPtr> transitions_;
        tree::Blackboard blackboard_;
    };

} // namespace bonsai::state
