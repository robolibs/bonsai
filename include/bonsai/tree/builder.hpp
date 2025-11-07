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
        Builder &sequence();
        Builder &selector();
        Builder &parallel(Parallel::Policy successPolicy, Parallel::Policy failurePolicy);
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

      private:
        void add(const NodePtr &node);

        NodePtr root_;
        std::vector<NodePtr> stack_;
        std::vector<Decorator::Func> decorators_;
        int pendingRepeat_ = -2; // -2 means no repeat pending, -1 means infinite repeat
        int pendingRetry_ = -2;  // -2 means no retry pending, -1 means infinite retry
    };

} // namespace bonsai::tree
