#pragma once
#include "../tree/structure/blackboard.hpp"
#include "structure/state.hpp"
#include "structure/transition.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace stateup {
    namespace core {
        class ThreadPool;
    }
} // namespace stateup

namespace stateup::state {

    // Forward declaration for friend class
    class CompositeState;

    // Debug event types
    enum class DebugEvent {
        STATE_ENTERED,
        STATE_UPDATED,
        STATE_EXITED,
        TRANSITION_EVALUATED,
        TRANSITION_TAKEN,
        TRANSITION_REJECTED
    };

    // Debug information structure
    struct DebugInfo {
        DebugEvent event;
        std::string fromState;
        std::string toState;
        std::string transitionInfo;
        std::chrono::steady_clock::time_point timestamp;
        bool guardPassed;
        int priority;
    };

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
        void setExecutor(stateup::core::ThreadPool *pool) { executor_ = pool; }

        // Debugging support
        using DebugCallback = std::function<void(const DebugInfo &)>;
        void setDebugCallback(DebugCallback callback) { debugCallback_ = std::move(callback); }
        void clearDebugCallback() { debugCallback_ = nullptr; }

        // Transition history
        struct TransitionRecord {
            std::string fromState;
            std::string toState;
            std::chrono::steady_clock::time_point timestamp;
            std::string reason; // e.g., "condition", "timed", "probabilistic"
        };

        const std::vector<TransitionRecord> &getTransitionHistory() const { return transitionHistory_; }
        void clearTransitionHistory() { transitionHistory_.clear(); }
        void enableTransitionHistory(bool enable = true) { trackTransitionHistory_ = enable; }
        bool isTransitionHistoryEnabled() const { return trackTransitionHistory_; }

      private:
        void transitionTo(const StatePtr &newState);
        void transitionTo(const StatePtr &newState, const std::string &reason);
        std::vector<TransitionPtr> getTransitionsFrom(const StatePtr &state) const;
        void startTimersForState(const StatePtr &state);
        void resetTimersForState(const StatePtr &state);
        void notifyDebug(const DebugInfo &info);

        StatePtr initialState_;
        StatePtr currentState_;
        StatePtr previousState_; // FIX: Track previous state
        std::unordered_map<std::string, StatePtr> states_;
        std::vector<TransitionPtr> transitions_;
        tree::Blackboard blackboard_;
        std::vector<std::string> stateHistory_;    // FIX: Track state history
        static constexpr size_t MAX_HISTORY = 100; // Limit history size
        stateup::core::ThreadPool *executor_ = nullptr;

        // Debugging support
        DebugCallback debugCallback_;
        std::vector<TransitionRecord> transitionHistory_;
        bool trackTransitionHistory_ = false;
        static constexpr size_t MAX_TRANSITION_HISTORY = 1000;
    };

} // namespace stateup::state
