#pragma once
#include "machine.hpp"
#include "structure/state.hpp"
#include "structure/transition.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace bonsai::state {

    class Builder {
      public:
        Builder() = default;

        // Create a new state with a name
        Builder &state(const std::string &name) {
            currentStateName_ = name;
            if (states_.find(name) == states_.end()) {
                states_[name] = std::make_shared<State>(name);
            }
            return *this;
        }

        // Set callbacks for the current state
        Builder &onEnter(State::Func func) {
            ensureCurrentState("onEnter");
            states_[currentStateName_]->setOnEnter(std::move(func));
            return *this;
        }

        Builder &onUpdate(State::Func func) {
            ensureCurrentState("onUpdate");
            states_[currentStateName_]->setOnUpdate(std::move(func));
            return *this;
        }

        Builder &onExit(State::Func func) {
            ensureCurrentState("onExit");
            states_[currentStateName_]->setOnExit(std::move(func));
            return *this;
        }

        // Add a transition from current state to another state
        Builder &transitionTo(const std::string &targetStateName, Transition::Condition condition) {
            ensureCurrentState("transitionTo");

            // Ensure target state exists
            if (states_.find(targetStateName) == states_.end()) {
                states_[targetStateName] = std::make_shared<State>(targetStateName);
            }

            pendingTransitions_.push_back({currentStateName_, targetStateName, std::move(condition)});
            return *this;
        }

        // Set the initial state
        Builder &initial(const std::string &stateName) {
            initialStateName_ = stateName;
            if (states_.find(stateName) == states_.end()) {
                states_[stateName] = std::make_shared<State>(stateName);
            }
            return *this;
        }

        // Build the state machine
        std::unique_ptr<StateMachine> build() {
            if (initialStateName_.empty()) {
                throw std::runtime_error("No initial state set. Use initial() to set the starting state.");
            }

            auto machine = std::make_unique<StateMachine>();

            // Add all states
            for (const auto &[name, state] : states_) {
                machine->addState(state);
            }

            // Set initial state
            machine->setInitialState(states_[initialStateName_]);

            // Add all transitions
            for (const auto &[fromName, toName, condition] : pendingTransitions_) {
                machine->addTransition(states_[fromName], states_[toName], condition);
            }

            return machine;
        }

      private:
        void ensureCurrentState(const char *context) const {
            if (currentStateName_.empty()) {
                throw std::runtime_error(std::string("No current state set. Call state() before ") + context);
            }
        }

        struct PendingTransition {
            std::string from;
            std::string to;
            Transition::Condition condition;
        };

        std::string currentStateName_;
        std::string initialStateName_;
        std::unordered_map<std::string, StatePtr> states_;
        std::vector<PendingTransition> pendingTransitions_;
    };

} // namespace bonsai::state
