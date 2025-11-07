#pragma once
#include "nodes/action.hpp"
#include "nodes/decorator.hpp"
#include "nodes/parallel.hpp"
#include "nodes/selector.hpp"
#include "nodes/sequence.hpp"
#include "tree.hpp"
#include <memory>
#include <vector>

namespace bonsai::tree {

    class Builder {
      public:
        inline Builder &sequence() {
            auto node = std::make_shared<Sequence>();
            add(node);
            stack_.emplace_back(node);
            return *this;
        }

        inline Builder &selector() {
            auto node = std::make_shared<Selector>();
            add(node);
            stack_.emplace_back(node);
            return *this;
        }

        inline Builder &parallel(Parallel::Policy successPolicy, Parallel::Policy failurePolicy) {
            auto node = std::make_shared<Parallel>(successPolicy, failurePolicy);
            add(node);
            stack_.emplace_back(node);
            return *this;
        }

        inline Builder &decorator(Decorator::Func func) {
            decorators_.emplace_back(std::move(func));
            return *this;
        }

        inline Builder &action(Action::Func func) {
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

        inline Builder &end() {
            if (!stack_.empty()) {
                stack_.pop_back();
            }
            return *this;
        }

        inline Tree build() {
            if (!root_) {
                throw std::runtime_error("Cannot build tree: no root node");
            }
            return Tree(root_);
        }

        // Convenience methods for common decorators
        inline Builder &inverter() { return decorator(decorators::Inverter()); }

        inline Builder &succeeder() { return decorator(decorators::Succeeder()); }

        inline Builder &failer() { return decorator(decorators::Failer()); }

        inline Builder &repeat(int maxTimes = -1) {
            // Store the repeat decorator to be applied to the next action
            pendingRepeat_ = maxTimes;
            return *this;
        }

        inline Builder &repeater(int maxTimes = -1) { return repeat(maxTimes); }

        inline Builder &retry(int maxTimes = -1) {
            // Store the retry decorator to be applied to the next action
            pendingRetry_ = maxTimes;
            return *this;
        }

      private:
        inline void add(const NodePtr &node) {
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

        NodePtr root_;
        std::vector<NodePtr> stack_;
        std::vector<Decorator::Func> decorators_;
        int pendingRepeat_ = -2; // -2 means no repeat pending, -1 means infinite repeat
        int pendingRetry_ = -2;  // -2 means no retry pending, -1 means infinite retry
    };

} // namespace bonsai::tree