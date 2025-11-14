#pragma once
#include "../tree/structure/blackboard.hpp"
#include "structure/state.hpp"
#include "structure/transition.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace bonsai {
    namespace core {
        class ThreadPool;
    }
} // namespace bonsai

namespace bonsai::state {

    // Forward declaration for friend class
    class CompositeState;

    class StateMachine {
        friend class CompositeState; // Allow CompositeState to access private members
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

        // FIX: Add state history
        const std::vector<std::string> &getStateHistory() const { return stateHistory_; }
        void clearHistory() { stateHistory_.clear(); }
        StatePtr getPreviousState() const { return previousState_; }
        void transitionToPrevious();

        // Optional: pluggable executor
        void setExecutor(bonsai::core::ThreadPool *pool) { executor_ = pool; }

      private:
        void transitionTo(const StatePtr &newState);
        std::vector<TransitionPtr> getTransitionsFrom(const StatePtr &state) const;

        StatePtr initialState_;
        StatePtr currentState_;
        StatePtr previousState_; // FIX: Track previous state
        std::unordered_map<std::string, StatePtr> states_;
        std::vector<TransitionPtr> transitions_;
        tree::Blackboard blackboard_;
        std::vector<std::string> stateHistory_;    // FIX: Track state history
        static constexpr size_t MAX_HISTORY = 100; // Limit history size
        bonsai::core::ThreadPool *executor_ = nullptr;
    };

} // namespace bonsai::state
