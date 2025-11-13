#include "bonsai/state/machine.hpp"
#include <algorithm>
#include <stdexcept>

namespace bonsai::state {

    StateMachine::StateMachine(StatePtr initialState) : initialState_(std::move(initialState)) {
        if (initialState_) {
            states_[initialState_->name()] = initialState_;
            currentState_ = initialState_;
        }
    }

    void StateMachine::addState(StatePtr state) {
        if (!state) {
            throw std::invalid_argument("Cannot add null state");
        }
        states_[state->name()] = state;
    }

    void StateMachine::addTransition(TransitionPtr transition) {
        if (!transition) {
            throw std::invalid_argument("Cannot add null transition");
        }
        transitions_.push_back(transition);
    }

    void StateMachine::addTransition(const StatePtr &from, const StatePtr &to, Transition::Condition condition) {
        if (!from || !to) {
            throw std::invalid_argument("Cannot create transition with null states");
        }
        auto transition = std::make_shared<Transition>(from, to, std::move(condition));
        addTransition(transition);
    }

    void StateMachine::setInitialState(StatePtr state) {
        if (!state) {
            throw std::invalid_argument("Initial state cannot be null");
        }
        initialState_ = state;
        addState(state);
    }

    void StateMachine::tick() {
        if (!currentState_) {
            if (initialState_) {
                transitionTo(initialState_);
            }
            return;
        }

        // Update current state
        currentState_->onUpdate(blackboard_);

        // Check for transitions - FIX: Sort by priority
        auto possibleTransitions = getTransitionsFrom(currentState_);
        std::sort(possibleTransitions.begin(), possibleTransitions.end(),
                  [](const TransitionPtr &a, const TransitionPtr &b) { return a->getPriority() > b->getPriority(); });
        for (const auto &transition : possibleTransitions) {
            // Validate the transition first
            if (transition->cannotHappen()) {
                transition->validate(); // This will throw
            }

            // Skip ignored events
            if (transition->isIgnored()) {
                continue;
            }

            // Check if transition should occur
            if (transition->shouldTransition(blackboard_)) {
                transitionTo(transition->to());
                break; // Take first valid transition
            }
        }
    }

    void StateMachine::reset() {
        if (currentState_) {
            currentState_->onExit(blackboard_);
        }
        currentState_ = nullptr;
        blackboard_.clear();
        if (initialState_) {
            transitionTo(initialState_);
        }
    }

    const std::string &StateMachine::getCurrentStateName() const {
        static const std::string empty = "";
        return currentState_ ? currentState_->name() : empty;
    }

    void StateMachine::transitionTo(const StatePtr &newState) {
        if (!newState) {
            return;
        }

        // FIX: Check guard condition BEFORE exiting current state
        if (!newState->onGuard(blackboard_)) {
            return;
        }

        // Exit current state
        if (currentState_) {
            currentState_->onExit(blackboard_);
            previousState_ = currentState_; // FIX: Track previous state
        }

        // Enter new state
        currentState_ = newState;
        currentState_->onEnter(blackboard_);

        // FIX: Add to history
        if (stateHistory_.size() >= MAX_HISTORY) {
            stateHistory_.erase(stateHistory_.begin());
        }
        stateHistory_.push_back(newState->name());
    }

    void StateMachine::transitionToPrevious() {
        if (previousState_) {
            transitionTo(previousState_);
        }
    }

    std::vector<TransitionPtr> StateMachine::getTransitionsFrom(const StatePtr &state) const {
        std::vector<TransitionPtr> result;
        for (const auto &transition : transitions_) {
            if (transition->from() == state) {
                result.push_back(transition);
            }
        }
        return result;
    }

} // namespace bonsai::state
