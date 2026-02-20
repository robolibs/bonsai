#pragma once
#include "../../tree/structure/blackboard.hpp"
#include "state.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace stateup::state {

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
        using Duration = std::chrono::milliseconds;
        using TimePoint = std::chrono::steady_clock::time_point;

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

            // Check timed transition
            if (isTimedTransition() && hasTimerStarted()) {
                auto elapsed = std::chrono::steady_clock::now() - timerStartTime_.value();
                if (elapsed >= duration_.value()) {
                    // Timer expired - check condition if present, otherwise allow transition
                    return !condition_ || condition_(blackboard);
                }
                // Timer not expired yet
                return false;
            }

            // Check condition - if no condition, allow transition (for weighted/probabilistic)
            return !condition_ || condition_(blackboard);
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

        // Timed transition support
        void setDuration(Duration duration) { duration_ = duration; }
        bool isTimedTransition() const { return duration_.has_value(); }
        void startTimer() { timerStartTime_ = std::chrono::steady_clock::now(); }
        void resetTimer() { timerStartTime_.reset(); }
        bool hasTimerStarted() const { return timerStartTime_.has_value(); }
        std::optional<Duration> getDuration() const { return duration_; }

        // Probabilistic transition support
        void setProbability(float probability) {
            if (probability < 0.0f || probability > 1.0f) {
                throw std::invalid_argument("Probability must be between 0.0 and 1.0");
            }
            probability_ = probability;
        }
        void setWeight(float weight) {
            if (weight < 0.0f) {
                throw std::invalid_argument("Weight must be non-negative");
            }
            weight_ = weight;
        }
        bool isProbabilistic() const { return probability_.has_value() || weight_.has_value(); }
        std::optional<float> getProbability() const { return probability_; }
        std::optional<float> getWeight() const { return weight_; }

      private:
        StatePtr from_;
        StatePtr to_;
        Condition condition_;
        TransitionResult result_;
        int priority_ = 0; // FIX: Add priority for transition ordering
        Action action_;    // FIX: Add transition actions

        // Timed transitions
        std::optional<Duration> duration_;
        std::optional<TimePoint> timerStartTime_;

        // Probabilistic transitions
        std::optional<float> probability_;
        std::optional<float> weight_;
    };

    using TransitionPtr = std::shared_ptr<Transition>;

} // namespace stateup::state
