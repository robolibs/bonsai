#pragma once
#include "../structure/node.hpp"
#include <vector>

namespace bonsai::tree {

    class Parallel : public Node {
      public:
        enum class Policy { RequireAll, RequireOne };

        Parallel(Policy successPolicy, Policy failurePolicy)
            : successPolicy_(successPolicy), failurePolicy_(failurePolicy) {}

        inline void addChild(const NodePtr &child) { children_.emplace_back(child); }

        inline Status tick(Blackboard &blackboard) override {
            if (halted_)
                return Status::Failure;

            size_t success = 0, failure = 0;
            for (auto &child : children_) {
                Status status = child->tick(blackboard);
                if (status == Status::Success)
                    ++success;
                else if (status == Status::Failure)
                    ++failure;
            }

            if ((successPolicy_ == Policy::RequireAll && success == children_.size()) ||
                (successPolicy_ == Policy::RequireOne && success > 0)) {
                return Status::Success;
            }

            if ((failurePolicy_ == Policy::RequireAll && failure == children_.size()) ||
                (failurePolicy_ == Policy::RequireOne && failure > 0)) {
                return Status::Failure;
            }

            return Status::Running;
        }

        inline void reset() override {
            halted_ = false;
            for (auto &child : children_) {
                child->reset();
            }
        }

        inline void halt() override {
            halted_ = true;
            for (auto &child : children_) {
                child->halt();
            }
        }

      private:
        std::vector<NodePtr> children_;
        Policy successPolicy_, failurePolicy_;
    };

} // namespace bonsai::tree