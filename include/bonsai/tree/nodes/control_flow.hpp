#pragma once
#include "../structure/node.hpp"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace bonsai::tree {

    // Conditional/If node - executes different branches based on condition
    class ConditionalNode : public Node {
      public:
        using ConditionFunc = std::function<bool(Blackboard &)>;

        ConditionalNode(ConditionFunc condition, NodePtr thenNode, NodePtr elseNode = nullptr);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        ConditionFunc condition_;
        NodePtr thenNode_;
        NodePtr elseNode_;
        NodePtr activeChild_ = nullptr;
    };

    // While/Loop node - repeats child while condition is true
    class WhileNode : public Node {
      public:
        using ConditionFunc = std::function<bool(Blackboard &)>;

        // maxIterations: -1 for infinite, > 0 for limited iterations per tick
        WhileNode(ConditionFunc condition, NodePtr child, int maxIterations = -1);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        ConditionFunc condition_;
        NodePtr child_;
        int maxIterations_;
        int currentIterations_;
        bool isRunning_;
    };

    // Switch/Case node - selects child based on key value
    class SwitchNode : public Node {
      public:
        using SelectorFunc = std::function<std::string(Blackboard &)>;

        SwitchNode(SelectorFunc selector);

        // Add a case branch
        void addCase(const std::string &caseValue, NodePtr node);

        // Set default case (when no case matches)
        void setDefault(NodePtr node);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        SelectorFunc selector_;
        std::unordered_map<std::string, NodePtr> cases_;
        NodePtr defaultNode_;
        NodePtr activeChild_ = nullptr;
        std::string lastCase_;
    };

    // Memory node - remembers and returns the last status of its child
    class MemoryNode : public Node {
      public:
        enum class MemoryPolicy {
            REMEMBER_SUCCESSFUL, // Only remember Success status
            REMEMBER_FAILURE,    // Only remember Failure status
            REMEMBER_FINISHED,   // Remember Success or Failure
            REMEMBER_ALL         // Remember any status including Running
        };

        MemoryNode(NodePtr child, MemoryPolicy policy = MemoryPolicy::REMEMBER_FINISHED);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

        // Clear the remembered status
        void clearMemory();

      private:
        NodePtr child_;
        MemoryPolicy policy_;
        std::optional<Status> rememberedStatus_;
        bool shouldRemember(Status status) const;
    };

    // For loop node - executes child a fixed number of times
    class ForNode : public Node {
      public:
        using CountFunc = std::function<int(Blackboard &)>;

        // Can use static count or dynamic count from blackboard
        ForNode(NodePtr child, int count);
        ForNode(NodePtr child, CountFunc countFunc);

        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        NodePtr child_;
        CountFunc countFunc_;
        int currentIndex_;
        int targetCount_;
        bool useStaticCount_;
    };

    // Conditional Sequence - short-circuits on first false condition
    class ConditionalSequence : public Node {
      public:
        using ConditionFunc = std::function<bool(Blackboard &)>;

        void addChild(NodePtr child, ConditionFunc precondition = nullptr);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        struct ConditionalChild {
            NodePtr node;
            ConditionFunc precondition;
        };
        std::vector<ConditionalChild> children_;
        size_t currentIndex_ = 0;
    };

} // namespace bonsai::tree