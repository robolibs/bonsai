#include <bonsai/tree/nodes/control_flow.hpp>

namespace bonsai::tree {

    // ============================================================================
    // ConditionalNode Implementation
    // ============================================================================

    ConditionalNode::ConditionalNode(ConditionFunc condition, NodePtr thenNode, NodePtr elseNode)
        : condition_(std::move(condition)), thenNode_(std::move(thenNode)), elseNode_(std::move(elseNode)) {}

    Status ConditionalNode::tick(Blackboard &blackboard) {
        if (state_ == State::Idle) {
            // Evaluate condition and select branch
            bool conditionResult = condition_(blackboard);
            activeChild_ = conditionResult ? thenNode_ : elseNode_;

            if (!activeChild_) {
                // No else branch and condition is false
                return Status::Success;
            }

            setState(State::Running);
        }

        if (activeChild_) {
            Status childStatus = activeChild_->tick(blackboard);

            if (childStatus != Status::Running) {
                setState(State::Idle);
                activeChild_ = nullptr;
            }

            return childStatus;
        }

        return Status::Success;
    }

    void ConditionalNode::reset() {
        Node::reset();
        if (thenNode_)
            thenNode_->reset();
        if (elseNode_)
            elseNode_->reset();
        activeChild_ = nullptr;
    }

    void ConditionalNode::halt() {
        Node::halt();
        if (activeChild_)
            activeChild_->halt();
        activeChild_ = nullptr;
    }

    // ============================================================================
    // WhileNode Implementation
    // ============================================================================

    WhileNode::WhileNode(ConditionFunc condition, NodePtr child, int maxIterations)
        : condition_(std::move(condition)), child_(std::move(child)), maxIterations_(maxIterations),
          currentIterations_(0), isRunning_(false) {}

    Status WhileNode::tick(Blackboard &blackboard) {
        // Check if we're starting fresh
        if (!isRunning_) {
            currentIterations_ = 0;
            isRunning_ = true;
        }

        // Execute while condition is true
        while (condition_(blackboard)) {
            // Check iteration limit
            if (maxIterations_ > 0 && currentIterations_ >= maxIterations_) {
                isRunning_ = false;
                return Status::Success;
            }

            Status childStatus = child_->tick(blackboard);

            if (childStatus == Status::Running) {
                return Status::Running;
            }

            if (childStatus == Status::Failure) {
                isRunning_ = false;
                child_->reset();
                return Status::Failure;
            }

            // Child succeeded, prepare for next iteration
            child_->reset();
            currentIterations_++;
        }

        // Condition became false
        isRunning_ = false;
        return Status::Success;
    }

    void WhileNode::reset() {
        Node::reset();
        child_->reset();
        currentIterations_ = 0;
        isRunning_ = false;
    }

    void WhileNode::halt() {
        Node::halt();
        child_->halt();
        isRunning_ = false;
    }

    // ============================================================================
    // SwitchNode Implementation
    // ============================================================================

    SwitchNode::SwitchNode(SelectorFunc selector) : selector_(std::move(selector)) {}

    void SwitchNode::addCase(const std::string &caseValue, NodePtr node) { cases_[caseValue] = std::move(node); }

    void SwitchNode::setDefault(NodePtr node) { defaultNode_ = std::move(node); }

    Status SwitchNode::tick(Blackboard &blackboard) {
        if (state_ == State::Idle) {
            // Select which case to execute
            std::string selectedCase = selector_(blackboard);

            auto it = cases_.find(selectedCase);
            if (it != cases_.end()) {
                activeChild_ = it->second;
                lastCase_ = selectedCase;
            } else {
                activeChild_ = defaultNode_;
                lastCase_ = "default";
            }

            if (!activeChild_) {
                return Status::Failure; // No matching case and no default
            }

            setState(State::Running);
        }

        // Continue executing the selected child
        if (activeChild_) {
            Status childStatus = activeChild_->tick(blackboard);

            if (childStatus != Status::Running) {
                setState(State::Idle);
                activeChild_ = nullptr;
                lastCase_.clear();
            }

            return childStatus;
        }

        return Status::Failure;
    }

    void SwitchNode::reset() {
        Node::reset();
        for (auto &[_, node] : cases_) {
            if (node)
                node->reset();
        }
        if (defaultNode_)
            defaultNode_->reset();
        activeChild_ = nullptr;
        lastCase_.clear();
    }

    void SwitchNode::halt() {
        Node::halt();
        if (activeChild_)
            activeChild_->halt();
        activeChild_ = nullptr;
    }

    // ============================================================================
    // MemoryNode Implementation
    // ============================================================================

    MemoryNode::MemoryNode(NodePtr child, MemoryPolicy policy) : child_(std::move(child)), policy_(policy) {}

    Status MemoryNode::tick(Blackboard &blackboard) {
        // If we have a remembered status, return it
        if (rememberedStatus_.has_value()) {
            return rememberedStatus_.value();
        }

        // Execute child
        Status childStatus = child_->tick(blackboard);

        // Remember status based on policy
        if (shouldRemember(childStatus)) {
            rememberedStatus_ = childStatus;
        }

        return childStatus;
    }

    void MemoryNode::reset() {
        Node::reset();
        child_->reset();
        clearMemory();
    }

    void MemoryNode::halt() {
        Node::halt();
        child_->halt();
    }

    void MemoryNode::clearMemory() { rememberedStatus_.reset(); }

    bool MemoryNode::shouldRemember(Status status) const {
        switch (policy_) {
        case MemoryPolicy::REMEMBER_SUCCESSFUL:
            return status == Status::Success;
        case MemoryPolicy::REMEMBER_FAILURE:
            return status == Status::Failure;
        case MemoryPolicy::REMEMBER_FINISHED:
            return status == Status::Success || status == Status::Failure;
        case MemoryPolicy::REMEMBER_ALL:
            return true;
        }
        return false;
    }

    // ============================================================================
    // ForNode Implementation
    // ============================================================================

    ForNode::ForNode(NodePtr child, int count)
        : child_(std::move(child)), countFunc_([count](Blackboard &) { return count; }), currentIndex_(0),
          targetCount_(count), useStaticCount_(true) {}

    ForNode::ForNode(NodePtr child, CountFunc countFunc)
        : child_(std::move(child)), countFunc_(std::move(countFunc)), currentIndex_(0), targetCount_(0),
          useStaticCount_(false) {}

    Status ForNode::tick(Blackboard &blackboard) {
        // Get target count on first tick or if dynamic
        if (currentIndex_ == 0 || !useStaticCount_) {
            targetCount_ = countFunc_(blackboard);
            if (targetCount_ <= 0) {
                return Status::Success; // Nothing to do
            }
        }

        // Execute child for remaining iterations
        while (currentIndex_ < targetCount_) {
            Status childStatus = child_->tick(blackboard);

            if (childStatus == Status::Running) {
                return Status::Running;
            }

            if (childStatus == Status::Failure) {
                currentIndex_ = 0; // Reset for next time
                child_->reset();
                return Status::Failure;
            }

            // Child succeeded, move to next iteration
            currentIndex_++;
            child_->reset();
        }

        // All iterations completed
        currentIndex_ = 0;
        return Status::Success;
    }

    void ForNode::reset() {
        Node::reset();
        child_->reset();
        currentIndex_ = 0;
    }

    void ForNode::halt() {
        Node::halt();
        child_->halt();
        currentIndex_ = 0;
    }

    // ============================================================================
    // ConditionalSequence Implementation
    // ============================================================================

    void ConditionalSequence::addChild(NodePtr child, ConditionFunc precondition) {
        children_.push_back({std::move(child), std::move(precondition)});
    }

    Status ConditionalSequence::tick(Blackboard &blackboard) {
        while (currentIndex_ < children_.size()) {
            auto &conditionalChild = children_[currentIndex_];

            // Check precondition if exists
            if (conditionalChild.precondition) {
                if (!conditionalChild.precondition(blackboard)) {
                    // Precondition failed, skip remaining children
                    currentIndex_ = 0;
                    return Status::Failure;
                }
            }

            // Execute child
            Status childStatus = conditionalChild.node->tick(blackboard);

            if (childStatus == Status::Running) {
                setState(State::Running);
                return Status::Running;
            }

            if (childStatus == Status::Failure) {
                currentIndex_ = 0;
                setState(State::Idle);
                return Status::Failure;
            }

            // Move to next child
            currentIndex_++;
        }

        // All children succeeded
        currentIndex_ = 0;
        setState(State::Idle);
        return Status::Success;
    }

    void ConditionalSequence::reset() {
        Node::reset();
        for (auto &child : children_) {
            if (child.node)
                child.node->reset();
        }
        currentIndex_ = 0;
    }

    void ConditionalSequence::halt() {
        Node::halt();
        if (currentIndex_ < children_.size()) {
            children_[currentIndex_].node->halt();
        }
        currentIndex_ = 0;
    }

} // namespace bonsai::tree