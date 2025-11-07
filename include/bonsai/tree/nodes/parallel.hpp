#pragma once
#include "../structure/node.hpp"
#include <optional>
#include <vector>

namespace bonsai::tree {

    class Parallel : public Node {
      public:
        enum class Policy { RequireAll, RequireOne };

        Parallel(Policy successPolicy, Policy failurePolicy);
        Parallel(size_t successThreshold, std::optional<size_t> failureThreshold = std::nullopt);

        void addChild(const NodePtr &child);
        Status tick(Blackboard &blackboard) override;
        void reset() override;
        void halt() override;

      private:
        std::vector<NodePtr> children_;
        std::vector<Status> childStates_;
        Policy successPolicy_, failurePolicy_;
        std::optional<size_t> successThreshold_;
        std::optional<size_t> failureThreshold_;

        void haltRunningChildren();
        bool successSatisfied(size_t successCount) const;
        bool failureSatisfied(size_t failureCount) const;
        bool successStillPossible(size_t successCount, size_t unresolved) const;
    };

} // namespace bonsai::tree
