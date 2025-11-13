#pragma once
#include "../structure/node.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace bonsai::tree {

    class Decorator : public Node {
      public:
        using Func = std::function<Status(Status)>;

        Decorator(Func func, NodePtr child);
        virtual ~Decorator() = default; // FIX: Add virtual destructor

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        Func func_;
        NodePtr child_;
    };

    // FIX: Avoid shared_ptr cycles by using stateless lambdas or weak references
    namespace decorators {
        inline std::function<Status(Status)> Inverter() {
            return [](Status status) {
                if (status == Status::Success)
                    return Status::Failure;
                if (status == Status::Failure)
                    return Status::Success;
                return status;
            };
        }

        inline std::function<Status(Status)> Succeeder() {
            return [](Status status) { return (status != Status::Running) ? Status::Success : Status::Running; };
        }

        inline std::function<Status(Status)> Failer() {
            return [](Status status) { return (status != Status::Running) ? Status::Failure : Status::Running; };
        }

        // FIX: Use a class-based approach to avoid shared_ptr cycles
        class RepeatDecoratorFunc {
            mutable int attempts_ = 0;
            int maxTimes_;

          public:
            explicit RepeatDecoratorFunc(int maxTimes) : maxTimes_(maxTimes) {}

            Status operator()(Status status) const {
                if (status == Status::Running) {
                    return Status::Running;
                }

                if (status == Status::Failure) {
                    attempts_ = 0; // Reset on failure
                    return Status::Failure;
                }

                // On success, repeat if we have attempts left
                if (status == Status::Success) {
                    attempts_++;
                    if (maxTimes_ > 0 && attempts_ >= maxTimes_) {
                        attempts_ = 0;          // Reset for next use
                        return Status::Success; // Done repeating
                    }
                    return Status::Running; // Repeat again
                }

                return status;
            }
        };

        inline std::function<Status(Status)> Repeat(int maxTimes = -1) { return RepeatDecoratorFunc(maxTimes); }

        // FIX: Use a class-based approach for Retry
        class RetryDecoratorFunc {
            mutable int attempts_ = 0;
            int maxTimes_;

          public:
            explicit RetryDecoratorFunc(int maxTimes) : maxTimes_(maxTimes) {}

            Status operator()(Status status) const {
                if (status == Status::Running) {
                    return Status::Running;
                }

                if (status == Status::Success) {
                    attempts_ = 0; // Reset on success
                    return Status::Success;
                }

                // On failure, try again if we have attempts left
                if (status == Status::Failure) {
                    attempts_++;
                    if (maxTimes_ > 0 && attempts_ >= maxTimes_) {
                        attempts_ = 0; // Reset for next use
                        return Status::Failure;
                    }
                    return Status::Running; // Try again
                }

                return status;
            }
        };

        inline std::function<Status(Status)> Retry(int maxTimes = -1) { return RetryDecoratorFunc(maxTimes); }

        // FIX: Use a class-based approach for Timeout
        class TimeoutDecoratorFunc {
            mutable std::chrono::steady_clock::time_point startTime_;
            float seconds_;

          public:
            explicit TimeoutDecoratorFunc(float seconds) : seconds_(seconds) {}

            Status operator()(Status status) const {
                auto now = std::chrono::steady_clock::now();

                if (startTime_ == std::chrono::steady_clock::time_point{}) {
                    startTime_ = now; // Initialize start time
                }

                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count() / 1000.0f;

                if (elapsed >= seconds_) {
                    startTime_ = {};        // Reset for next use
                    return Status::Failure; // Timeout reached
                }

                if (status != Status::Running) {
                    startTime_ = {}; // Reset on completion
                }

                return status;
            }
        };

        inline std::function<Status(Status)> Timeout(float seconds) { return TimeoutDecoratorFunc(seconds); }

        // FIX: Use a class-based approach for Cooldown
        class CooldownDecoratorFunc {
            mutable std::chrono::steady_clock::time_point lastSuccess_;
            float seconds_;

          public:
            explicit CooldownDecoratorFunc(float seconds) : seconds_(seconds) {}

            Status operator()(Status status) const {
                auto now = std::chrono::steady_clock::now();

                if (status == Status::Success) {
                    lastSuccess_ = now;
                    return Status::Success;
                }

                if (lastSuccess_ != std::chrono::steady_clock::time_point{}) {
                    auto elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSuccess_).count() / 1000.0f;
                    if (elapsed < seconds_) {
                        return Status::Failure; // Still in cooldown
                    }
                }

                return status;
            }
        };

        inline std::function<Status(Status)> Cooldown(float seconds) { return CooldownDecoratorFunc(seconds); }
    } // namespace decorators

    // Specialized repeat decorator that can execute child multiple times per tick
    class RepeatDecorator : public Node {
      public:
        RepeatDecorator(int maxTimes, NodePtr child);
        virtual ~RepeatDecorator() = default; // FIX: Add virtual destructor

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        int maxTimes_;
        NodePtr child_;
        int currentCount_;
    };

    // Specialized retry decorator that retries on failure
    class RetryDecorator : public Node {
      public:
        RetryDecorator(int maxTimes, NodePtr child);
        virtual ~RetryDecorator() = default; // FIX: Add virtual destructor

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        int maxTimes_;
        NodePtr child_;
        int currentAttempts_;
    };

} // namespace bonsai::tree