#include "bonsai/tree/builder.hpp"

namespace bonsai::tree {

    Builder &Builder::sequence() {
        auto node = std::make_shared<Sequence>();
        add(node);
        stack_.emplace_back(node);
        return *this;
    }

    Builder &Builder::selector() {
        auto node = std::make_shared<Selector>();
        add(node);
        stack_.emplace_back(node);
        return *this;
    }

    Builder &Builder::parallel(Parallel::Policy successPolicy, Parallel::Policy failurePolicy) {
        auto node = std::make_shared<Parallel>(successPolicy, failurePolicy);
        add(node);
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
        if (pendingRepeat_ != -2) {
            node = std::make_shared<RepeatDecorator>(pendingRepeat_, node);
            pendingRepeat_ = -2; // Reset
        }

        // Apply pending retry decorator
        if (pendingRetry_ != -2) {
            node = std::make_shared<RetryDecorator>(pendingRetry_, node);
            pendingRetry_ = -2; // Reset
        }

        // Apply other decorators in reverse order (last added first)
        while (!decorators_.empty()) {
            node = std::make_shared<Decorator>(decorators_.back(), node);
            decorators_.pop_back();
        }

        add(node);
        return *this;
    }

    Builder &Builder::end() {
        if (!stack_.empty()) {
            stack_.pop_back();
        }
        return *this;
    }

    Tree Builder::build() {
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
            }
        }
    }

} // namespace bonsai::tree
