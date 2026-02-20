#include "stateup/tree/nodes/parallel.hpp"
#include "stateup/core/executor.hpp"
// <execution> removed: using internal ThreadPool
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace stateup::tree {

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
        static stateup::core::ThreadPool defaultPool;
        stateup::core::ThreadPool *pool = this->executor_ ? this->executor_ : &defaultPool;
        std::atomic<bool> stop{false};
        const size_t total = indices.size();
        std::atomic<size_t> processed{0};
        std::atomic<size_t> succ{0};
        std::atomic<size_t> fail{0};
        pool->bulk_early_stop(
            [&](size_t k) -> bool {
                if (stop.load(std::memory_order_relaxed))
                    return true;
                size_t i = indices[k];
                auto prev = childStates_[i];
                if (prev == Status::Success || prev == Status::Failure) {
                    processed.fetch_add(1, std::memory_order_relaxed);
                    return true; // skip
                }
                Status status = children_[i]->tick(blackboard);
                childStates_[i] = status;
                if (status == Status::Success)
                    succ.fetch_add(1, std::memory_order_relaxed);
                if (status == Status::Failure)
                    fail.fetch_add(1, std::memory_order_relaxed);
                size_t done = processed.fetch_add(1, std::memory_order_relaxed) + 1;
                size_t unresolved =
                    total - (succ.load(std::memory_order_relaxed) + fail.load(std::memory_order_relaxed));
                // Early-stop conditions
                if (!successThreshold_.has_value()) {
                    if (successPolicy_ == Policy::RequireOne && status == Status::Success)
                        return false;
                    if (successPolicy_ == Policy::RequireAll && status == Status::Failure)
                        return false; // cannot all succeed
                } else {
                    size_t s = succ.load(std::memory_order_relaxed);
                    size_t required = successThreshold_.value();
                    if (s >= required)
                        return false;
                    if (s + unresolved < required)
                        return false; // impossible now
                }
                if (failureThreshold_.has_value()) {
                    size_t f = fail.load(std::memory_order_relaxed);
                    if (f >= failureThreshold_.value())
                        return false;
                } else {
                    if (failurePolicy_ == Policy::RequireOne && status == Status::Failure)
                        return false;
                    // RequireAll failure early-stop is non-trivial safely; skip
                }
                (void)done;
                return true;
            },
            indices.size(), stop);

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

} // namespace stateup::tree
