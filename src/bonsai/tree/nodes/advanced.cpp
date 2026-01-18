#include <bonsai/tree/nodes/advanced.hpp>

namespace bonsai::tree {

    // Thread-local RNG for RandomSelector
    thread_local std::mt19937 RandomSelector::rng_(std::random_device{}());

    // Thread-local RNG for ProbabilitySelector
    thread_local std::mt19937 ProbabilitySelector::rng_(std::random_device{}());

    // ============================================================================
    // RandomSelector Implementation
    // ============================================================================

    void RandomSelector::addChild(const NodePtr &child) { children_.push_back(child); }

    Status RandomSelector::tick(Blackboard &blackboard) {
        if (children_.empty()) {
            return Status::Failure;
        }

        // If no child is currently running, pick a random one
        if (currentIndex_ == SIZE_MAX || state_ == State::Idle) {
            std::uniform_int_distribution<size_t> dist(0, children_.size() - 1);
            currentIndex_ = dist(rng_);
            setState(State::Running);
        }

        // Execute the selected child
        Status childStatus = children_[currentIndex_]->tick(blackboard);

        if (childStatus == Status::Success) {
            currentIndex_ = SIZE_MAX;
            setState(State::Idle);
            return Status::Success;
        }

        if (childStatus == Status::Failure) {
            currentIndex_ = SIZE_MAX;
            setState(State::Idle);
            return Status::Failure;
        }

        // Child is still running
        setState(State::Running);
        return Status::Running;
    }

    void RandomSelector::reset() {
        Node::reset();
        for (auto &child : children_) {
            if (child)
                child->reset();
        }
        currentIndex_ = SIZE_MAX;
    }

    void RandomSelector::halt() {
        Node::halt();
        if (currentIndex_ != SIZE_MAX && currentIndex_ < children_.size()) {
            children_[currentIndex_]->halt();
        }
        currentIndex_ = SIZE_MAX;
    }

    // ============================================================================
    // ProbabilitySelector Implementation
    // ============================================================================

    void ProbabilitySelector::addChild(const NodePtr &child, float probability) {
        // Clamp probability to [0.0, 1.0]
        float clampedProb = std::max(0.0f, std::min(1.0f, probability));
        children_.push_back({child, clampedProb});
    }

    Status ProbabilitySelector::tick(Blackboard &blackboard) {
        if (children_.empty()) {
            return Status::Failure;
        }

        // If no child is currently running, select one based on probabilities
        if (currentIndex_ == SIZE_MAX || state_ == State::Idle) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float roll = dist(rng_);

            // Find the first child whose probability threshold is met
            bool childSelected = false;
            for (size_t i = 0; i < children_.size(); ++i) {
                if (roll <= children_[i].probability) {
                    currentIndex_ = i;
                    childSelected = true;
                    break;
                }
            }

            // If no child was selected (all probabilities < roll), pick the last one
            if (!childSelected) {
                currentIndex_ = children_.size() - 1;
            }

            setState(State::Running);
        }

        // Execute the selected child
        Status childStatus = children_[currentIndex_].node->tick(blackboard);

        if (childStatus == Status::Success) {
            currentIndex_ = SIZE_MAX;
            setState(State::Idle);
            return Status::Success;
        }

        if (childStatus == Status::Failure) {
            currentIndex_ = SIZE_MAX;
            setState(State::Idle);
            return Status::Failure;
        }

        // Child is still running
        setState(State::Running);
        return Status::Running;
    }

    void ProbabilitySelector::reset() {
        Node::reset();
        for (auto &child : children_) {
            if (child.node)
                child.node->reset();
        }
        currentIndex_ = SIZE_MAX;
    }

    void ProbabilitySelector::halt() {
        Node::halt();
        if (currentIndex_ != SIZE_MAX && currentIndex_ < children_.size()) {
            children_[currentIndex_].node->halt();
        }
        currentIndex_ = SIZE_MAX;
    }

    // ============================================================================
    // OneShotSequence Implementation
    // ============================================================================

    void OneShotSequence::addChild(const NodePtr &child) { children_.push_back(child); }

    Status OneShotSequence::tick(Blackboard &blackboard) {
        if (children_.empty()) {
            return Status::Success;
        }

        // Execute children in sequence, skipping already-executed ones
        while (currentIndex_ < children_.size()) {
            // Skip children that have already been executed
            if (executedChildren_.find(currentIndex_) != executedChildren_.end()) {
                currentIndex_++;
                continue;
            }

            // Execute current child
            Status childStatus = children_[currentIndex_]->tick(blackboard);

            if (childStatus == Status::Running) {
                setState(State::Running);
                return Status::Running;
            }

            if (childStatus == Status::Failure) {
                // Mark as executed even on failure
                executedChildren_.insert(currentIndex_);
                currentIndex_ = 0; // Reset for next time
                setState(State::Idle);
                return Status::Failure;
            }

            // Child succeeded, mark as executed and move to next
            executedChildren_.insert(currentIndex_);
            currentIndex_++;
        }

        // All children completed (or were already executed)
        currentIndex_ = 0;
        setState(State::Idle);
        return Status::Success;
    }

    void OneShotSequence::reset() {
        Node::reset();
        for (auto &child : children_) {
            if (child)
                child->reset();
        }
        currentIndex_ = 0;
        // Note: We don't clear executedChildren_ on reset - that's intentional
        // Use clearExecutionHistory() if you want to allow re-execution
    }

    void OneShotSequence::halt() {
        Node::halt();
        if (currentIndex_ < children_.size()) {
            children_[currentIndex_]->halt();
        }
        currentIndex_ = 0;
    }

    void OneShotSequence::clearExecutionHistory() {
        executedChildren_.clear();
        currentIndex_ = 0;
    }

    // ============================================================================
    // DebounceDecorator Implementation
    // ============================================================================

    DebounceDecorator::DebounceDecorator(NodePtr child, Duration debounceTime)
        : child_(std::move(child)), debounceTime_(debounceTime) {}

    Status DebounceDecorator::tick(Blackboard &blackboard) {
        if (!child_) {
            return Status::Failure;
        }

        // Execute child
        Status childStatus = child_->tick(blackboard);

        auto now = Clock::now();

        // Check if result changed
        if (!lastResult_.has_value() || lastResult_.value() != childStatus) {
            // Result changed, reset stability timer
            lastResult_ = childStatus;
            lastChangeTime_ = now;
            isStable_ = false;
            setState(State::Running);
            return Status::Running;
        }

        // Result is same as last time
        if (!isStable_ && lastChangeTime_.has_value()) {
            auto elapsed = std::chrono::duration_cast<Duration>(now - lastChangeTime_.value());

            if (elapsed >= debounceTime_) {
                // Result has been stable long enough
                isStable_ = true;
                setState(State::Idle);
                return childStatus;
            } else {
                // Still waiting for stability
                setState(State::Running);
                return Status::Running;
            }
        }

        // Already stable, return the stable result
        if (isStable_) {
            setState(State::Idle);
            return childStatus;
        }

        // Shouldn't reach here, but return running as safe default
        setState(State::Running);
        return Status::Running;
    }

    void DebounceDecorator::reset() {
        Node::reset();
        if (child_)
            child_->reset();
        lastResult_.reset();
        lastChangeTime_.reset();
        isStable_ = false;
    }

    void DebounceDecorator::halt() {
        Node::halt();
        if (child_)
            child_->halt();
        lastResult_.reset();
        lastChangeTime_.reset();
        isStable_ = false;
    }

    void DebounceDecorator::setDebounceTime(Duration time) { debounceTime_ = time; }

    DebounceDecorator::Duration DebounceDecorator::getDebounceTime() const { return debounceTime_; }

} // namespace bonsai::tree
