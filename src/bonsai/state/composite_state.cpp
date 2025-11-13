#include "bonsai/state/structure/composite_state.hpp"
#include "bonsai/state/machine.hpp"

namespace bonsai::state {

    CompositeState::CompositeState(const std::string &name, HistoryType historyType)
        : State(name), historyType_(historyType) {}

    bool CompositeState::onGuard(tree::Blackboard &blackboard) {
        // First check parent guard
        if (!State::onGuard(blackboard)) {
            return false;
        }

        // If we have a nested machine, check if it can enter its initial state
        if (nestedMachine_ && nestedMachine_->getCurrentState()) {
            return nestedMachine_->getCurrentState()->onGuard(blackboard);
        }

        return true;
    }

    void CompositeState::onEnter(tree::Blackboard &blackboard) {
        // Call parent onEnter
        State::onEnter(blackboard);

        // Initialize or restore nested state machine
        if (nestedMachine_) {
            if (historyType_ != HistoryType::None && !lastActiveState_.empty()) {
                // Restore history
                restoreHistory(blackboard);
            } else {
                // Start from initial state
                nestedMachine_->reset();
                nestedMachine_->tick(); // Initialize
            }
        }

        // Initialize regions
        for (auto &region : regions_) {
            if (region.machine) {
                region.machine->reset();
                region.machine->tick();
            }
        }
    }

    void CompositeState::onUpdate(tree::Blackboard &blackboard) {
        // Call parent onUpdate
        State::onUpdate(blackboard);

        // Update nested state machine
        if (nestedMachine_) {
            nestedMachine_->tick();
        }

        // Update all regions independently
        for (auto &region : regions_) {
            if (region.machine) {
                region.machine->tick();
            }
        }
    }

    void CompositeState::onExit(tree::Blackboard &blackboard) {
        // Save history before exiting
        if (historyType_ != HistoryType::None && nestedMachine_) {
            saveHistory();
        }

        // Exit nested states
        if (nestedMachine_ && nestedMachine_->getCurrentState()) {
            nestedMachine_->getCurrentState()->onExit(blackboard);
        }

        // Exit regions' current states
        for (auto &region : regions_) {
            if (region.machine && region.machine->getCurrentState()) {
                region.machine->getCurrentState()->onExit(blackboard);
            }
        }

        // Call parent onExit
        State::onExit(blackboard);
    }

    void CompositeState::setNestedMachine(std::unique_ptr<StateMachine> machine) {
        nestedMachine_ = std::move(machine);
    }

    void CompositeState::addRegion(const std::string &name, std::unique_ptr<StateMachine> machine) {
        regions_.push_back(Region{name, std::move(machine)});
    }

    std::vector<std::string> CompositeState::getRegionNames() const {
        std::vector<std::string> names;
        names.reserve(regions_.size());
        for (const auto &r : regions_)
            names.push_back(r.name);
        return names;
    }

    std::string CompositeState::getRegionCurrentState(const std::string &name) const {
        for (const auto &r : regions_) {
            if (r.name == name) {
                return r.machine ? r.machine->getCurrentStateName() : std::string();
            }
        }
        return std::string();
    }

    void CompositeState::clearHistory() {
        lastActiveState_.clear();
        deepHistory_.clear();
    }

    void CompositeState::addEntryPoint(const std::string &name, const std::string &targetState) {
        entryPoints_[name] = targetState;
    }

    void CompositeState::addExitPoint(const std::string &name) { exitPoints_.insert(name); }

    bool CompositeState::enterVia(const std::string &entryPoint, tree::Blackboard &blackboard) {
        auto it = entryPoints_.find(entryPoint);
        if (it == entryPoints_.end()) {
            return false;
        }

        // Enter the composite state
        onEnter(blackboard);

        // Navigate to specific substate
        if (nestedMachine_) {
            // Force transition to target state
            auto states = nestedMachine_->states_;
            auto targetIt = states.find(it->second);
            if (targetIt != states.end()) {
                nestedMachine_->transitionTo(targetIt->second);
                return true;
            }
        }

        return false;
    }

    bool CompositeState::shouldHandleTransition(const std::string &transitionName) const {
        // Check if this is an exit point
        return exitPoints_.find(transitionName) != exitPoints_.end();
    }

    std::string CompositeState::getQualifiedStateName() const {
        std::string qualified = name_;

        // Build hierarchy path
        if (parentComposite_) {
            qualified = parentComposite_->getQualifiedStateName() + "." + qualified;
        }

        // Add current substate if active
        if (nestedMachine_ && nestedMachine_->getCurrentState()) {
            qualified += "." + nestedMachine_->getCurrentStateName();
        }

        return qualified;
    }

    bool CompositeState::isInSubstate(const std::string &substateName) const {
        if (!nestedMachine_) {
            return false;
        }

        return nestedMachine_->getCurrentStateName() == substateName;
    }

    std::string CompositeState::getCurrentSubstate() const {
        if (!nestedMachine_) {
            return "";
        }

        return nestedMachine_->getCurrentStateName();
    }

    void CompositeState::saveHistory() {
        if (!nestedMachine_ || !nestedMachine_->getCurrentState()) {
            return;
        }

        lastActiveState_ = nestedMachine_->getCurrentStateName();

        if (historyType_ == HistoryType::Deep) {
            // Recursively save history of nested composite states
            auto currentState = nestedMachine_->getCurrentState();
            if (auto composite = std::dynamic_pointer_cast<CompositeState>(currentState)) {
                composite->saveHistory();
                deepHistory_[currentState->name()] = composite->getCurrentSubstate();
            }
        }
    }

    void CompositeState::restoreHistory(tree::Blackboard &blackboard) {
        if (!nestedMachine_ || lastActiveState_.empty()) {
            return;
        }

        // Find and transition to saved state
        auto states = nestedMachine_->states_;
        auto it = states.find(lastActiveState_);
        if (it != states.end()) {
            nestedMachine_->transitionTo(it->second);

            // Restore deep history if applicable
            if (historyType_ == HistoryType::Deep) {
                if (auto composite = std::dynamic_pointer_cast<CompositeState>(it->second)) {
                    auto deepIt = deepHistory_.find(lastActiveState_);
                    if (deepIt != deepHistory_.end() && !deepIt->second.empty()) {
                        composite->lastActiveState_ = deepIt->second;
                        composite->restoreHistory(blackboard);
                    }
                }
            }
        }
    }

} // namespace bonsai::state
