#pragma once
#include "../structure/node.hpp"
#include <functional>

namespace bonsai::tree {

    class Action : public Node {
      public:
        using Func = std::function<Status(Blackboard &)>;

        explicit Action(Func func) : func_(std::move(func)) {}

        inline Status tick(Blackboard &blackboard) override {
            if (halted_)
                return Status::Failure;
            return func_(blackboard);
        }

        inline void reset() override { halted_ = false; }

        inline void halt() override { halted_ = true; }

      private:
        Func func_;
    };

} // namespace bonsai::tree
