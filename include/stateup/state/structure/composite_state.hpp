#pragma once
#include "../machine.hpp"
#include "state.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace stateup::state {

    // Forward declaration
    class StateMachine;

    /**
     * CompositeState - A state that contains its own state machine (nested states)
     * Enables hierarchical state machines (HSM) with proper entry/exit handling
     */
    class CompositeState : public State {
      public:
        enum class HistoryType {
            None,    // No history - always start from initial state
            Shallow, // Remember last state at this level only
            Deep     // Remember complete state configuration including nested states
        };

        explicit CompositeState(const std::string &name, HistoryType historyType = HistoryType::None);
        virtual ~CompositeState() = default;

        // Override state lifecycle methods
        bool onGuard(tree::Blackboard &blackboard) override;
        void onEnter(tree::Blackboard &blackboard) override;
        void onUpdate(tree::Blackboard &blackboard) override;
        void onExit(tree::Blackboard &blackboard) override;

        // Composite state specific methods
        void setNestedMachine(std::unique_ptr<StateMachine> machine);
        StateMachine *getNestedMachine() { return nestedMachine_.get(); }
        const StateMachine *getNestedMachine() const { return nestedMachine_.get(); }

        // Orthogonal regions support
        void addRegion(const std::string &name, std::unique_ptr<StateMachine> machine);
        std::vector<std::string> getRegionNames() const;
        std::string getRegionCurrentState(const std::string &name) const;

        // History management
        void setHistoryType(HistoryType type) { historyType_ = type; }
        HistoryType getHistoryType() const { return historyType_; }
        void clearHistory();

        // Entry/Exit points for targeted entry into substates
        void addEntryPoint(const std::string &name, const std::string &targetState);
        void addExitPoint(const std::string &name);
        bool enterVia(const std::string &entryPoint, tree::Blackboard &blackboard);

        // Check if a transition should bubble up to parent
        bool shouldHandleTransition(const std::string &transitionName) const;

        // Get the full qualified state name (including parent hierarchy)
        std::string getQualifiedStateName() const;

        // Check if this composite state is currently in a specific substate
        bool isInSubstate(const std::string &substateName) const;

        // Get current active substate (if any)
        std::string getCurrentSubstate() const;

      private:
        std::unique_ptr<StateMachine> nestedMachine_;
        HistoryType historyType_;

        // Orthogonal regions
        struct Region {
            std::string name;
            std::unique_ptr<StateMachine> machine;
        };
        std::vector<Region> regions_;

        // History storage
        std::string lastActiveState_;                              // For shallow history
        std::unordered_map<std::string, std::string> deepHistory_; // For deep history

        // Entry/Exit points
        std::unordered_map<std::string, std::string> entryPoints_;
        std::unordered_set<std::string> exitPoints_;

        // Parent reference for hierarchical communication
        CompositeState *parentComposite_ = nullptr;

        // Save and restore history
        void saveHistory();
        void restoreHistory(tree::Blackboard &blackboard);
    };

    using CompositeStatePtr = std::shared_ptr<CompositeState>;

} // namespace stateup::state
