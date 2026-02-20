#include "doctest/doctest.h"

#include "stateup/state/builder.hpp"
#include "stateup/state/machine.hpp"
#include <string>

using namespace stateup;

// Test states for state machine execution
class IdleState : public state::State {
  public:
    IdleState() : State("Idle") {}

    void onEnter(tree::Blackboard &bb) override {
        State::onEnter(bb);
        bb.set<std::string>("current_state", "Idle");
    }

    void onUpdate(tree::Blackboard &bb) override {
        State::onUpdate(bb);
        auto ticks = bb.get<int>("idle_ticks").value_or(0);
        bb.set<int>("idle_ticks", ticks + 1);
    }
};

class WalkState : public state::State {
  public:
    WalkState() : State("Walk") {}

    void onEnter(tree::Blackboard &bb) override {
        State::onEnter(bb);
        bb.set<std::string>("current_state", "Walk");
        bb.set<int>("walk_ticks", 0);
    }

    void onUpdate(tree::Blackboard &bb) override {
        State::onUpdate(bb);
        auto ticks = bb.get<int>("walk_ticks").value_or(0);
        bb.set<int>("walk_ticks", ticks + 1);
    }
};

class RunState : public state::State {
  public:
    RunState() : State("Run") {}

    void onEnter(tree::Blackboard &bb) override {
        State::onEnter(bb);
        bb.set<std::string>("current_state", "Run");
    }

    void onUpdate(tree::Blackboard &bb) override {
        State::onUpdate(bb);
        auto ticks = bb.get<int>("run_ticks").value_or(0);
        bb.set<int>("run_ticks", ticks + 1);
    }
};

TEST_CASE("StateMachine: Basic initialization") {
    auto machine = std::make_unique<state::StateMachine>();
    CHECK(machine->getCurrentStateName() == "");
}

TEST_CASE("StateMachine: Set initial state") {
    auto machine = std::make_unique<state::StateMachine>();
    auto idleState = std::make_shared<IdleState>();

    machine->setInitialState(idleState);
    CHECK(machine->getCurrentState() == nullptr); // Not started yet

    machine->tick();
    CHECK(machine->getCurrentStateName() == "Idle");
}

TEST_CASE("StateMachine: Basic state transitions") {
    auto machine = std::make_unique<state::StateMachine>();
    auto idleState = std::make_shared<IdleState>();
    auto walkState = std::make_shared<WalkState>();

    machine->addState(idleState);
    machine->addState(walkState);
    machine->setInitialState(idleState);

    // Add transition from idle to walk when idle_ticks >= 3
    machine->addTransition(idleState, walkState,
                           [](tree::Blackboard &bb) { return bb.get<int>("idle_ticks").value_or(0) >= 3; });

    // Tick 1: Enter idle (no update on first tick, idle_ticks not set yet)
    machine->tick();
    CHECK(machine->getCurrentStateName() == "Idle");
    CHECK_FALSE(machine->blackboard().has("idle_ticks"));

    // Tick 2: Update idle (idle_ticks = 1)
    machine->tick();
    CHECK(machine->getCurrentStateName() == "Idle");
    CHECK(machine->blackboard().get<int>("idle_ticks").value() == 1);

    // Tick 3: Update idle (idle_ticks = 2)
    machine->tick();
    CHECK(machine->getCurrentStateName() == "Idle");
    CHECK(machine->blackboard().get<int>("idle_ticks").value() == 2);

    // Tick 4: Update idle (idle_ticks = 3), then transition to walk
    machine->tick();
    CHECK(machine->getCurrentStateName() == "Walk");
    CHECK(machine->blackboard().get<std::string>("current_state").value() == "Walk");
}

TEST_CASE("StateMachine: Multiple transitions") {
    auto machine = std::make_unique<state::StateMachine>();
    auto idleState = std::make_shared<IdleState>();
    auto walkState = std::make_shared<WalkState>();
    auto runState = std::make_shared<RunState>();

    machine->addState(idleState);
    machine->addState(walkState);
    machine->addState(runState);
    machine->setInitialState(idleState);

    // Idle -> Walk when idle_ticks >= 2
    machine->addTransition(idleState, walkState,
                           [](tree::Blackboard &bb) { return bb.get<int>("idle_ticks").value_or(0) >= 2; });

    // Walk -> Run when walk_ticks >= 3
    machine->addTransition(walkState, runState,
                           [](tree::Blackboard &bb) { return bb.get<int>("walk_ticks").value_or(0) >= 3; });

    // Run -> Idle when run_ticks >= 2
    machine->addTransition(runState, idleState,
                           [](tree::Blackboard &bb) { return bb.get<int>("run_ticks").value_or(0) >= 2; });

    // Idle: enter, update (idle_ticks=1), update (idle_ticks=2, transition)
    machine->tick(); // Enter Idle
    CHECK(machine->getCurrentStateName() == "Idle");
    machine->tick(); // Update Idle (idle_ticks=1)
    machine->tick(); // Update Idle (idle_ticks=2), transition to Walk
    CHECK(machine->getCurrentStateName() == "Walk");

    // Walk: enter (walk_ticks=0), update (walk_ticks=1), update (walk_ticks=2), update (walk_ticks=3, transition)
    machine->tick(); // Update Walk (walk_ticks=1)
    machine->tick(); // Update Walk (walk_ticks=2)
    machine->tick(); // Update Walk (walk_ticks=3), transition to Run
    CHECK(machine->getCurrentStateName() == "Run");

    // Run: enter, update (run_ticks=1), update (run_ticks=2, transition)
    machine->tick(); // Update Run (run_ticks=1)
    machine->tick(); // Update Run (run_ticks=2), transition to Idle
    CHECK(machine->getCurrentStateName() == "Idle");
}

TEST_CASE("StateMachine: Reset functionality") {
    auto machine = std::make_unique<state::StateMachine>();
    auto idleState = std::make_shared<IdleState>();
    auto walkState = std::make_shared<WalkState>();

    machine->addState(idleState);
    machine->addState(walkState);
    machine->setInitialState(idleState);

    machine->addTransition(idleState, walkState,
                           [](tree::Blackboard &bb) { return bb.get<int>("idle_ticks").value_or(0) >= 2; });

    // Run to walk state
    machine->tick(); // Enter Idle
    machine->tick(); // Update Idle (idle_ticks=1)
    machine->tick(); // Update Idle (idle_ticks=2), transition to Walk
    CHECK(machine->getCurrentStateName() == "Walk");

    // Reset should go back to initial state
    machine->reset();
    CHECK(machine->getCurrentStateName() == "Idle");
    CHECK_FALSE(machine->blackboard().has("idle_ticks")); // Blackboard cleared
}

TEST_CASE("StateMachine: Blackboard access") {
    auto machine = std::make_unique<state::StateMachine>();
    auto idleState = std::make_shared<IdleState>();
    machine->setInitialState(idleState);

    machine->blackboard().set<std::string>("test_key", "test_value");
    CHECK(machine->blackboard().get<std::string>("test_key").value() == "test_value");

    machine->tick();
    CHECK(machine->blackboard().get<std::string>("current_state").value() == "Idle");
}
