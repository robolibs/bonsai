#include "bonsai/tree/nodes/parallel.hpp"
#include "bonsai/core/executor.hpp"
// <execution> removed: using internal ThreadPool
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace bonsai::tree {

    Parallel::Parallel(Policy successPolicy, Policy failurePolicy)
        : successPolicy_(successPolicy), failurePolicy_(failurePolicy) {}

    Parallel::Parallel(size_t successThreshold, std::optional<size_t> failureThreshold)
        : successPolicy_(Policy::RequireAll), failurePolicy_(Policy::RequireAll),
          successThreshold_(successThreshold ? std::optional<size_t>(successThreshold) : std::nullopt),
          failureThreshold_(failureThreshold) {
        if (!successThreshold_.has_value()) {
            throw std::invalid_argument("Parallel success threshold must be greater than zero");
        }
        if (failureThreshold_.has_value() && failureThreshold_.value() == 0) {
            throw std::invalid_argument("Parallel failure threshold must be greater than zero");
        }
    }

    void Parallel::addChild(const NodePtr &child) {
        children_.emplace_back(child);
        childStates_.emplace_back(Status::Idle);
    }

    Status Parallel::tick(Blackboard &blackboard) {
        if (state_ == State::Halted)
            return Status::Failure;

        state_ = State::Running;

        if (children_.empty())
            return Status::Success;

        // Prepare indices to process all non-terminal children
        std::vector<size_t> indices(children_.size());
        std::iota(indices.begin(), indices.end(), 0);

        // Run child ticks in parallel where available; otherwise sequential.
        // Note: Blackboard writes are synchronized internally; parallel children may still observe each other's writes.
        static bonsai::core::ThreadPool pool;
        pool.bulk(
            [&](size_t k) {
                size_t i = indices[k];
                auto prev = childStates_[i];
                if (prev == Status::Success || prev == Status::Failure) {
                    return; // Already resolved this child
                }
                Status status = children_[i]->tick(blackboard);
                childStates_[i] = status;
            },
            indices.size());

        // Aggregate results
        size_t success = 0, failure = 0;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (childStates_[i] == Status::Success)
                ++success;
            else if (childStates_[i] == Status::Failure)
                ++failure;
        }

        if (successSatisfied(success)) {
            haltRunningChildren();
            reset();
            return Status::Success;
        }
        if (failureSatisfied(failure)) {
            haltRunningChildren();
            reset();
            return Status::Failure;
        }

        size_t unresolved = children_.size() - success - failure;
        if (!successStillPossible(success, unresolved)) {
            haltRunningChildren();
            reset();
            return Status::Failure;
        }

        return Status::Running;
    }

    void Parallel::reset() {
        Node::reset();
        for (size_t i = 0; i < children_.size(); ++i) {
            childStates_[i] = Status::Idle;
            children_[i]->reset();
        }
    }

    void Parallel::halt() {
        Node::halt();
        haltRunningChildren();
        for (size_t i = 0; i < children_.size(); ++i) {
            children_[i]->reset();
            childStates_[i] = Status::Idle;
        }
    }

    void Parallel::haltRunningChildren() {
        for (size_t i = 0; i < children_.size(); ++i) {
            if (childStates_[i] == Status::Running) {
                children_[i]->halt();
                childStates_[i] = Status::Idle;
            }
        }
    }

    bool Parallel::successSatisfied(size_t successCount) const {
        if (successThreshold_.has_value()) {
            return successCount >= successThreshold_.value();
        }

        if (successPolicy_ == Policy::RequireAll)
            return successCount == children_.size();
        return successCount > 0;
    }

    bool Parallel::failureSatisfied(size_t failureCount) const {
        if (failureThreshold_.has_value()) {
            return failureCount >= failureThreshold_.value();
        }

        if (failurePolicy_ == Policy::RequireAll)
            return failureCount == children_.size();
        return failureCount > 0;
    }

    bool Parallel::successStillPossible(size_t successCount, size_t unresolved) const {
        if (!successThreshold_.has_value())
            return true;

        size_t required = successThreshold_.value();
        return successCount + unresolved >= required;
    }

} // namespace bonsai::tree
