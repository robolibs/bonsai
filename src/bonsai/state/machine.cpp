#include "bonsai/state/machine.hpp"
#include "bonsai/core/executor.hpp"
#include <algorithm>
#include <limits>
#include <random>
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

        // Check for transitions - evaluate conditions in parallel then pick highest priority
        auto possibleTransitions = getTransitionsFrom(currentState_);
        if (possibleTransitions.empty()) {
            return;
        }

        // Pre-validate and filter ignored; keep indices for stable mapping
        std::vector<size_t> indices;
        indices.reserve(possibleTransitions.size());
        for (size_t i = 0; i < possibleTransitions.size(); ++i) {
            const auto &tr = possibleTransitions[i];
            if (tr->cannotHappen()) {
                tr->validate(); // will throw
            }
            if (tr->isIgnored()) {
                continue;
            }
            indices.push_back(i);
        }
        if (indices.empty()) {
            return;
        }
        std::vector<char> results(possibleTransitions.size(), 0);

        // Determine max priority to allow correct early-stop when found
        int maxPriority = std::numeric_limits<int>::min();
        for (size_t idx : indices) {
            maxPriority = std::max(maxPriority, possibleTransitions[idx]->getPriority());
        }

        static bonsai::core::ThreadPool defaultPool;
        bonsai::core::ThreadPool *pool = executor_ ? executor_ : &defaultPool;
        std::atomic<bool> stop{false};
        pool->bulk_early_stop(
            [&](size_t k) -> bool {
                if (stop.load(std::memory_order_relaxed))
                    return true;
                size_t idx = indices[k];
                bool ok = possibleTransitions[idx]->shouldTransition(blackboard_);
                results[idx] = ok ? 1 : 0;
                // Only do early stop for non-probabilistic/non-weighted transitions
                if (ok && possibleTransitions[idx]->getPriority() == maxPriority &&
                    !possibleTransitions[idx]->isProbabilistic()) {
                    // Found the best possible transition; safe to stop further work
                    return false; // signal stop
                }
                return true;
            },
            indices.size(), stop);

        // Collect valid transitions
        std::vector<size_t> validIndices;
        for (size_t i = 0; i < possibleTransitions.size(); ++i) {
            if (results[i]) {
                validIndices.push_back(i);
            }
        }

        if (validIndices.empty()) {
            return;
        }

        size_t chosen = static_cast<size_t>(-1);

        // Separate probabilistic/weighted from non-probabilistic
        std::vector<size_t> weightedIndices;
        std::vector<size_t> probabilisticIndices;
        std::vector<size_t> normalIndices;

        for (size_t idx : validIndices) {
            if (possibleTransitions[idx]->getWeight().has_value()) {
                weightedIndices.push_back(idx);
            } else if (possibleTransitions[idx]->getProbability().has_value()) {
                probabilisticIndices.push_back(idx);
            } else {
                normalIndices.push_back(idx);
            }
        }

        // Non-probabilistic transitions take precedence
        if (!normalIndices.empty()) {
            // Pick highest priority among normal transitions
            int bestPriority = std::numeric_limits<int>::min();
            for (size_t idx : normalIndices) {
                int pr = possibleTransitions[idx]->getPriority();
                if (pr > bestPriority) {
                    bestPriority = pr;
                    chosen = idx;
                }
            }
        } else if (!weightedIndices.empty()) {
            // Weighted random selection
            thread_local std::mt19937 rng(std::random_device{}());
            float totalWeight = 0.0f;
            for (size_t idx : weightedIndices) {
                totalWeight += possibleTransitions[idx]->getWeight().value();
            }

            if (totalWeight <= 0.0f) {
                // All weights are zero, pick first one
                chosen = weightedIndices[0];
            } else {
                std::uniform_real_distribution<float> dist(0.0f, totalWeight);
                float random = dist(rng);
                float cumulative = 0.0f;

                for (size_t idx : weightedIndices) {
                    cumulative += possibleTransitions[idx]->getWeight().value();
                    if (random <= cumulative) {
                        chosen = idx;
                        break;
                    }
                }
            }
        } else if (!probabilisticIndices.empty()) {
            // Probability-based selection - each transition tested independently in order
            thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            for (size_t idx : probabilisticIndices) {
                float prob = possibleTransitions[idx]->getProbability().value();
                if (prob > 0.0f) {
                    float roll = dist(rng);
                    if (roll < prob) {
                        chosen = idx;
                        break;
                    }
                }
            }
            // If no transition succeeded, chosen remains -1 (stay in current state)
        } else {
            // Pick highest priority among transitions (fallback)
            int bestPriority = std::numeric_limits<int>::min();
            for (size_t idx : validIndices) {
                int pr = possibleTransitions[idx]->getPriority();
                if (pr > bestPriority) {
                    bestPriority = pr;
                    chosen = idx;
                }
            }
        }

        if (chosen != static_cast<size_t>(-1)) {
            auto &transition = possibleTransitions[chosen];
            transition->executeAction(blackboard_);
            transitionTo(transition->to());
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
            // Reset timers for the old state
            resetTimersForState(currentState_);
        }

        // Enter new state
        currentState_ = newState;
        currentState_->onEnter(blackboard_);

        // Start timers for the new state
        startTimersForState(newState);

        // FIX: Add to history
        if (stateHistory_.size() >= MAX_HISTORY) {
            stateHistory_.erase(stateHistory_.begin());
        }
        stateHistory_.push_back(newState->name());
    }

    void StateMachine::startTimersForState(const StatePtr &state) {
        auto transitions = getTransitionsFrom(state);
        for (auto &transition : transitions) {
            if (transition->isTimedTransition()) {
                transition->startTimer();
            }
        }
    }

    void StateMachine::resetTimersForState(const StatePtr &state) {
        auto transitions = getTransitionsFrom(state);
        for (auto &transition : transitions) {
            if (transition->isTimedTransition()) {
                transition->resetTimer();
            }
        }
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
