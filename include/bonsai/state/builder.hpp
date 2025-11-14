#pragma once
#include "machine.hpp"
#include "structure/composite_state.hpp"
#include "structure/state.hpp"
#include "structure/transition.hpp"
#include <functional>
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
        Builder &onGuard(State::GuardFunc func) {
            ensureCurrentState("onGuard");
            states_[currentStateName_]->setOnGuard(std::move(func));
            return *this;
        }

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

            pendingTransitions_.push_back(PendingTransition(currentStateName_, targetStateName, std::move(condition)));
            return *this;
        }

        // Add a timed transition from current state to another state
        Builder &transitionToAfter(const std::string &targetStateName, std::chrono::milliseconds duration,
                                   Transition::Condition condition = nullptr) {
            ensureCurrentState("transitionToAfter");

            // Ensure target state exists
            if (states_.find(targetStateName) == states_.end()) {
                states_[targetStateName] = std::make_shared<State>(targetStateName);
            }

            auto trans = PendingTransition(currentStateName_, targetStateName, std::move(condition));
            trans.duration = duration;
            pendingTransitions_.push_back(std::move(trans));
            return *this;
        }

        // Set action for the last added transition
        Builder &withAction(Transition::Action action) {
            if (pendingTransitions_.empty()) {
                throw std::runtime_error("No transition to add action to. Call transitionTo() first.");
            }
            pendingTransitions_.back().action = std::move(action);
            return *this;
        }

        // Set priority for the last added transition
        Builder &withPriority(int priority) {
            if (pendingTransitions_.empty()) {
                throw std::runtime_error("No transition to add priority to. Call transitionTo() first.");
            }
            pendingTransitions_.back().priority = priority;
            return *this;
        }

        // Set probability for the last added transition (0.0 to 1.0)
        Builder &withProbability(float probability) {
            if (pendingTransitions_.empty()) {
                throw std::runtime_error("No transition to add probability to. Call transitionTo() first.");
            }
            if (probability < 0.0f || probability > 1.0f) {
                throw std::invalid_argument("Probability must be between 0.0 and 1.0");
            }
            pendingTransitions_.back().probability = probability;
            return *this;
        }

        // Set weight for the last added transition (non-negative)
        Builder &withWeight(float weight) {
            if (pendingTransitions_.empty()) {
                throw std::runtime_error("No transition to add weight to. Call transitionTo() first.");
            }
            if (weight < 0.0f) {
                throw std::invalid_argument("Weight must be non-negative");
            }
            pendingTransitions_.back().weight = weight;
            return *this;
        }

        // Mark transition as ignored (event is ignored, no state change)
        Builder &ignoreEvent() {
            ensureCurrentState("ignoreEvent");
            pendingTransitions_.push_back(PendingTransition(currentStateName_, TransitionResult::EVENT_IGNORED));
            return *this;
        }

        // Mark transition as cannot happen (triggers error if attempted)
        Builder &cannotHappen() {
            ensureCurrentState("cannotHappen");
            pendingTransitions_.push_back(PendingTransition(currentStateName_, TransitionResult::CANNOT_HAPPEN));
            return *this;
        }

        // Create a composite (hierarchical) state
        Builder &compositeState(const std::string &name,
                                CompositeState::HistoryType historyType = CompositeState::HistoryType::None,
                                std::function<void(Builder &)> nestedStates = nullptr) {
            currentStateName_ = name;

            // Create composite state
            auto composite = std::make_shared<CompositeState>(name, historyType);
            states_[name] = composite;

            // Build nested state machine if provided
            if (nestedStates) {
                Builder nestedBuilder;
                nestedStates(nestedBuilder);
                auto nestedMachine = nestedBuilder.build();
                composite->setNestedMachine(std::move(nestedMachine));
            }

            return *this;
        }

        // Add entry point to current composite state
        Builder &entryPoint(const std::string &name, const std::string &targetState) {
            ensureCurrentState("entryPoint");
            if (auto composite = std::dynamic_pointer_cast<CompositeState>(states_[currentStateName_])) {
                composite->addEntryPoint(name, targetState);
            } else {
                throw std::runtime_error("entryPoint can only be added to composite states");
            }
            return *this;
        }

        // Add exit point to current composite state
        Builder &exitPoint(const std::string &name) {
            ensureCurrentState("exitPoint");
            if (auto composite = std::dynamic_pointer_cast<CompositeState>(states_[currentStateName_])) {
                composite->addExitPoint(name);
            } else {
                throw std::runtime_error("exitPoint can only be added to composite states");
            }
            return *this;
        }

        // Add an orthogonal region to the current composite state
        Builder &region(const std::string &name, std::function<void(Builder &)> buildRegion) {
            ensureCurrentState("region");
            auto composite = std::dynamic_pointer_cast<CompositeState>(states_[currentStateName_]);
            if (!composite) {
                throw std::runtime_error("region can only be added to composite states");
            }
            Builder regionBuilder;
            buildRegion(regionBuilder);
            auto regionMachine = regionBuilder.build();
            composite->addRegion(name, std::move(regionMachine));
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

        // Optional: set executor used by StateMachine during build
        Builder &executor(bonsai::core::ThreadPool *pool) {
            executor_ = pool;
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
            for (const auto &trans : pendingTransitions_) {
                if (trans.result == TransitionResult::VALID) {
                    auto transition = std::make_shared<Transition>(states_[trans.from], states_[trans.to],
                                                                   trans.condition, trans.priority);

                    // Apply optional features
                    if (trans.duration.has_value()) {
                        transition->setDuration(trans.duration.value());
                    }
                    if (trans.action.has_value()) {
                        transition->setAction(trans.action.value());
                    }
                    if (trans.probability.has_value()) {
                        transition->setProbability(trans.probability.value());
                    }
                    if (trans.weight.has_value()) {
                        transition->setWeight(trans.weight.value());
                    }

                    machine->addTransition(transition);
                } else {
                    // EVENT_IGNORED or CANNOT_HAPPEN
                    auto transition = std::make_shared<Transition>(states_[trans.from], trans.result);
                    machine->addTransition(transition);
                }
            }

            // Propagate executor if provided
            if (executor_) {
                machine->setExecutor(executor_);
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
            TransitionResult result;
            std::optional<std::chrono::milliseconds> duration;
            std::optional<Transition::Action> action;
            int priority = 0;
            std::optional<float> probability;
            std::optional<float> weight;

            PendingTransition(std::string f, std::string t, Transition::Condition c)
                : from(std::move(f)), to(std::move(t)), condition(std::move(c)), result(TransitionResult::VALID) {}

            PendingTransition(std::string f, TransitionResult r)
                : from(std::move(f)), to(""), condition(nullptr), result(r) {}
        };

        std::string currentStateName_;
        std::string initialStateName_;
        std::unordered_map<std::string, StatePtr> states_;
        std::vector<PendingTransition> pendingTransitions_;
        bonsai::core::ThreadPool *executor_ = nullptr;
    };

} // namespace bonsai::state
