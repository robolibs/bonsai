#pragma once
#include "nodes/action.hpp"
#include "nodes/control_flow.hpp"
#include "nodes/decorator.hpp"
#include "nodes/parallel.hpp"
#include "nodes/selector.hpp"
#include "nodes/sequence.hpp"
#include "tree.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace bonsai::tree {

    class Builder {
      public:
        Builder &sequence();
        Builder &selector();
        Builder &parallel(Parallel::Policy successPolicy, Parallel::Policy failurePolicy);
        Builder &parallel(size_t successThreshold, std::optional<size_t> failureThreshold = std::nullopt);
        Builder &decorator(Decorator::Func func);
        Builder &action(Action::Func func);
        Builder &end();
        Tree build();

        // Convenience methods for common decorators
        Builder &inverter();
        Builder &succeeder();
        Builder &failer();
        Builder &repeat(int maxTimes = -1);
        Builder &repeater(int maxTimes = -1);
        Builder &retry(int maxTimes = -1);

        // Control flow nodes
        Builder &condition(std::function<bool(Blackboard &)> cond, std::function<void(Builder &)> thenBranch,
                           std::function<void(Builder &)> elseBranch = nullptr);
        Builder &whileLoop(std::function<bool(Blackboard &)> condition, std::function<void(Builder &)> body,
                           int maxIterations = -1);
        Builder &forLoop(int count, std::function<void(Builder &)> body);
        Builder &forLoop(std::function<int(Blackboard &)> countFunc, std::function<void(Builder &)> body);
        Builder &switchNode(std::function<std::string(Blackboard &)> selector);
        Builder &addCase(const std::string &caseValue, std::function<void(Builder &)> body);
        Builder &defaultCase(std::function<void(Builder &)> body);
        Builder &memory(MemoryNode::MemoryPolicy policy = MemoryNode::MemoryPolicy::REMEMBER_FINISHED);
        Builder &conditionalSequence();

      private:
        void add(const NodePtr &node);
        NodePtr applyPendingDecorators(NodePtr node);
        void ensureNoPendingDecorators(const char *context) const;
        void ensureNoPendingLeafModifiers(const char *context) const;

        static constexpr int kNoPendingModifier = -2;

        NodePtr root_;
        std::vector<NodePtr> stack_;
        std::vector<Decorator::Func> decorators_;
        int pendingRepeat_ = kNoPendingModifier; // -2 means no repeat pending, -1 means infinite repeat
        int pendingRetry_ = kNoPendingModifier;  // -2 means no retry pending, -1 means infinite retry

        // For switch node building
        std::shared_ptr<SwitchNode> currentSwitch_ = nullptr;
    };

} // namespace bonsai::tree
