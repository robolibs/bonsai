#include "bonsai/tree/builder.hpp"
#include "bonsai/tree/nodes/control_flow.hpp"
#include <string>

namespace bonsai::tree {

    Builder &Builder::sequence() {
        auto node = std::make_shared<Sequence>();
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        stack_.emplace_back(node);
        return *this;
    }

    Builder &Builder::selector() {
        auto node = std::make_shared<Selector>();
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        stack_.emplace_back(node);
        return *this;
    }

    Builder &Builder::parallel(Parallel::Policy successPolicy, Parallel::Policy failurePolicy) {
        auto node = std::make_shared<Parallel>(successPolicy, failurePolicy);
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        stack_.emplace_back(node);
        return *this;
    }

    Builder &Builder::parallel(size_t successThreshold, std::optional<size_t> failureThreshold) {
        auto node = std::make_shared<Parallel>(successThreshold, failureThreshold);
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        stack_.emplace_back(node);
        return *this;
    }

    Builder &Builder::decorator(Decorator::Func func) {
        decorators_.emplace_back(std::move(func));
        return *this;
    }

    Builder &Builder::action(Action::Func func) {
        NodePtr node = std::make_shared<Action>(std::move(func));

        // Apply pending repeat decorator
        if (pendingRepeat_ != kNoPendingModifier) {
            node = std::make_shared<RepeatDecorator>(pendingRepeat_, node);
            pendingRepeat_ = kNoPendingModifier; // Reset
        }

        // Apply pending retry decorator
        if (pendingRetry_ != kNoPendingModifier) {
            node = std::make_shared<RetryDecorator>(pendingRetry_, node);
            pendingRetry_ = kNoPendingModifier; // Reset
        }

        node = applyPendingDecorators(node);

        add(node);
        return *this;
    }

    Builder &Builder::end() {
        if (stack_.empty()) {
            throw std::runtime_error("Cannot end(): no open composite node to close");
        }
        ensureNoPendingDecorators("end()");
        ensureNoPendingLeafModifiers("end()");
        stack_.pop_back();
        return *this;
    }

    Tree Builder::build() {
        ensureNoPendingDecorators("build()");
        ensureNoPendingLeafModifiers("build()");
        if (!stack_.empty()) {
            throw std::runtime_error("Cannot build tree: unbalanced builder, missing end()");
        }
        if (!root_) {
            throw std::runtime_error("Cannot build tree: no root node");
        }
        return Tree(root_);
    }

    // Convenience methods for common decorators
    Builder &Builder::inverter() { return decorator(decorators::Inverter()); }

    Builder &Builder::succeeder() { return decorator(decorators::Succeeder()); }

    Builder &Builder::failer() { return decorator(decorators::Failer()); }

    Builder &Builder::repeat(int maxTimes) {
        // Store the repeat decorator to be applied to the next action
        pendingRepeat_ = maxTimes;
        return *this;
    }

    Builder &Builder::repeater(int maxTimes) { return repeat(maxTimes); }

    Builder &Builder::retry(int maxTimes) {
        // Store the retry decorator to be applied to the next action
        pendingRetry_ = maxTimes;
        return *this;
    }

    // Control flow nodes implementation
    Builder &Builder::condition(std::function<bool(Blackboard &)> cond, std::function<void(Builder &)> thenBranch,
                                std::function<void(Builder &)> elseBranch) {
        // Build then branch
        Builder thenBuilder;
        if (thenBranch) {
            thenBranch(thenBuilder);
        }
        NodePtr thenNode = thenBuilder.root_;

        // Build else branch if provided
        NodePtr elseNode = nullptr;
        if (elseBranch) {
            Builder elseBuilder;
            elseBranch(elseBuilder);
            elseNode = elseBuilder.root_;
        }

        auto node = std::make_shared<ConditionalNode>(std::move(cond), thenNode, elseNode);
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        return *this;
    }

    Builder &Builder::whileLoop(std::function<bool(Blackboard &)> condition, std::function<void(Builder &)> body,
                                int maxIterations) {
        // Build loop body
        Builder bodyBuilder;
        if (body) {
            body(bodyBuilder);
        }
        NodePtr bodyNode = bodyBuilder.root_;

        auto node = std::make_shared<WhileNode>(std::move(condition), bodyNode, maxIterations);
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        return *this;
    }

    Builder &Builder::forLoop(int count, std::function<void(Builder &)> body) {
        // Build loop body
        Builder bodyBuilder;
        if (body) {
            body(bodyBuilder);
        }
        NodePtr bodyNode = bodyBuilder.root_;

        auto node = std::make_shared<ForNode>(bodyNode, count);
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        return *this;
    }

    Builder &Builder::forLoop(std::function<int(Blackboard &)> countFunc, std::function<void(Builder &)> body) {
        // Build loop body
        Builder bodyBuilder;
        if (body) {
            body(bodyBuilder);
        }
        NodePtr bodyNode = bodyBuilder.root_;

        auto node = std::make_shared<ForNode>(bodyNode, std::move(countFunc));
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        return *this;
    }

    Builder &Builder::switchNode(std::function<std::string(Blackboard &)> selector) {
        currentSwitch_ = std::make_shared<SwitchNode>(std::move(selector));
        return *this;
    }

    Builder &Builder::addCase(const std::string &caseValue, std::function<void(Builder &)> body) {
        if (!currentSwitch_) {
            throw std::runtime_error("addCase() must be called after switchNode()");
        }

        // Build case body
        Builder caseBuilder;
        if (body) {
            body(caseBuilder);
        }
        NodePtr caseNode = caseBuilder.root_;

        currentSwitch_->addCase(caseValue, caseNode);
        return *this;
    }

    Builder &Builder::defaultCase(std::function<void(Builder &)> body) {
        if (!currentSwitch_) {
            throw std::runtime_error("defaultCase() must be called after switchNode()");
        }

        // Build default case body
        Builder defaultBuilder;
        if (body) {
            body(defaultBuilder);
        }
        NodePtr defaultNode = defaultBuilder.root_;

        currentSwitch_->setDefault(defaultNode);

        // Add the complete switch node to the tree
        auto decorated = applyPendingDecorators(currentSwitch_);
        add(decorated);
        currentSwitch_ = nullptr; // Reset for next switch
        return *this;
    }

    Builder &Builder::memory(MemoryNode::MemoryPolicy policy) {
        decorators_.emplace_back([](Status status) {
            // This is a placeholder - the actual memory node needs special handling
            // The policy parameter would be used when properly integrating with MemoryNode
            return status;
        });
        return *this;
    }

    Builder &Builder::conditionalSequence() {
        auto node = std::make_shared<ConditionalSequence>();
        auto decorated = applyPendingDecorators(node);
        add(decorated);
        stack_.emplace_back(node);
        return *this;
    }

    void Builder::add(const NodePtr &node) {
        if (stack_.empty()) {
            root_ = node;
        } else {
            auto &parent = stack_.back();

            // Try to cast to different composite node types and add child
            if (auto seq = std::dynamic_pointer_cast<Sequence>(parent)) {
                seq->addChild(node);
            } else if (auto sel = std::dynamic_pointer_cast<Selector>(parent)) {
                sel->addChild(node);
            } else if (auto par = std::dynamic_pointer_cast<Parallel>(parent)) {
                par->addChild(node);
            } else if (auto condSeq = std::dynamic_pointer_cast<ConditionalSequence>(parent)) {
                condSeq->addChild(node);
            }
        }
    }

    NodePtr Builder::applyPendingDecorators(NodePtr node) {
        while (!decorators_.empty()) {
            node = std::make_shared<Decorator>(decorators_.back(), node);
            decorators_.pop_back();
        }
        return node;
    }

    void Builder::ensureNoPendingDecorators(const char *context) const {
        if (!decorators_.empty()) {
            throw std::runtime_error(std::string("Cannot ") + context +
                                     ": pending decorators must wrap a node before closing");
        }
    }

    void Builder::ensureNoPendingLeafModifiers(const char *context) const {
        if (pendingRepeat_ != kNoPendingModifier) {
            throw std::runtime_error(std::string("Cannot ") + context + ": pending repeat() must wrap an action");
        }
        if (pendingRetry_ != kNoPendingModifier) {
            throw std::runtime_error(std::string("Cannot ") + context + ": pending retry() must wrap an action");
        }
    }

} // namespace bonsai::tree
