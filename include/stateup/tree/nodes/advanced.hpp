#pragma once
#include "../structure/node.hpp"
#include <chrono>
#include <random>
#include <unordered_set>
#include <vector>

namespace stateup::tree {

    // ============================================================================
    // RandomSelector - Picks a random child uniformly (not weighted)
    // ============================================================================
    class RandomSelector : public Node {
      public:
        void addChild(const NodePtr &child);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<NodePtr> children_;
        size_t currentIndex_ = SIZE_MAX;
        static thread_local std::mt19937 rng_;
    };

    // ============================================================================
    // ProbabilitySelector - Each child has fixed probability of being selected
    // ============================================================================
    class ProbabilitySelector : public Node {
      public:
        struct ProbabilityChild {
            NodePtr node;
            float probability; // 0.0 to 1.0
        };

        void addChild(const NodePtr &child, float probability);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<ProbabilityChild> children_;
        size_t currentIndex_ = SIZE_MAX;
        static thread_local std::mt19937 rng_;
    };

    // ============================================================================
    // OneShotSequence - Each child runs exactly once across all ticks
    // ============================================================================
    class OneShotSequence : public Node {
      public:
        void addChild(const NodePtr &child);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

        // Reset execution state (allow children to run again)
        void clearExecutionHistory();

      private:
        std::vector<NodePtr> children_;
        std::unordered_set<size_t> executedChildren_; // Track which children have run
        size_t currentIndex_ = 0;
    };

    // ============================================================================
    // DebounceDecorator - Delays execution until child result is stable
    // ============================================================================
    class DebounceDecorator : public Node {
      public:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::milliseconds;

        explicit DebounceDecorator(NodePtr child, Duration debounceTime);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

        void setDebounceTime(Duration time);
        Duration getDebounceTime() const;

      private:
        NodePtr child_;
        Duration debounceTime_;
        std::optional<Status> lastResult_;
        std::optional<Clock::time_point> lastChangeTime_;
        bool isStable_ = false;
    };

} // namespace stateup::tree
