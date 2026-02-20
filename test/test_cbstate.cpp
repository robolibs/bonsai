#include "doctest/doctest.h"

#include "stateup/state/state.hpp"
#include <string>

using namespace stateup;

TEST_CASE("CbState: Simple callback state with update only") {
    auto machine = std::make_unique<state::StateMachine>();

    auto idle = state::CbState::create("idle", [](tree::Blackboard &bb) { bb.set<std::string>("status", "idle"); });

    machine->setInitialState(idle);
    machine->tick(); // Enter idle

    CHECK(machine->getCurrentStateName() == "idle");

    machine->tick(); // Update idle
    CHECK(machine->blackboard().get<std::string>("status").value() == "idle");
}

TEST_CASE("CbState: Full lifecycle with guard/entry/update/exit") {
    auto machine = std::make_unique<state::StateMachine>();

    bool guardCalled = false;
    bool enterCalled = false;
    bool updateCalled = false;
    bool exitCalled = false;

    auto testState = state::CbState::create(
        "test",
        [&guardCalled](tree::Blackboard &) {
            guardCalled = true;
            return true;
        },                                                            // guard
        [&enterCalled](tree::Blackboard &) { enterCalled = true; },   // enter
        [&updateCalled](tree::Blackboard &) { updateCalled = true; }, // update
        [&exitCalled](tree::Blackboard &) { exitCalled = true; }      // exit
    );

    machine->setInitialState(testState);

    machine->tick(); // Guard + Enter
    CHECK(guardCalled);
    CHECK(enterCalled);
    CHECK_FALSE(updateCalled);
    CHECK_FALSE(exitCalled);

    machine->tick(); // Update
    CHECK(updateCalled);

    machine->reset(); // Exit
    CHECK(exitCalled);
}

TEST_CASE("CbState: Counter example with callback state") {
    auto machine = std::make_unique<state::StateMachine>();

    auto counting = state::CbState::create("counting", [](tree::Blackboard &bb) {
        int count = bb.get<int>("count").value_or(0);
        bb.set<int>("count", count + 1);
    });

    auto done = state::CbState::create("done", [](tree::Blackboard &bb) { bb.set<bool>("finished", true); });

    machine->addState(counting);
    machine->addState(done);
    machine->setInitialState(counting);

    machine->addTransition(counting, done, [](tree::Blackboard &bb) { return bb.get<int>("count").value_or(0) >= 3; });

    machine->tick(); // Enter counting
    CHECK(machine->getCurrentStateName() == "counting");

    machine->tick(); // count = 1
    CHECK(machine->blackboard().get<int>("count").value() == 1);

    machine->tick(); // count = 2
    CHECK(machine->getCurrentStateName() == "counting");

    machine->tick(); // count = 3, transition to done
    CHECK(machine->getCurrentStateName() == "done");

    machine->tick(); // Update done
    CHECK(machine->blackboard().get<bool>("finished").value());
}

TEST_CASE("CbState: Traffic light with callback states") {
    auto machine = std::make_unique<state::StateMachine>();

    auto red = state::CbState::create(
        "red",
        nullptr,                                               // no guard
        [](tree::Blackboard &bb) { bb.set<int>("timer", 0); }, // enter
        [](tree::Blackboard &bb) {                             // update
            int timer = bb.get<int>("timer").value_or(0);
            bb.set<int>("timer", timer + 1);
        },
        nullptr // no exit
    );

    auto green = state::CbState::create(
        "green", nullptr, [](tree::Blackboard &bb) { bb.set<int>("timer", 0); },
        [](tree::Blackboard &bb) {
            int timer = bb.get<int>("timer").value_or(0);
            bb.set<int>("timer", timer + 1);
        },
        nullptr);

    auto yellow = state::CbState::create(
        "yellow", nullptr, [](tree::Blackboard &bb) { bb.set<int>("timer", 0); },
        [](tree::Blackboard &bb) {
            int timer = bb.get<int>("timer").value_or(0);
            bb.set<int>("timer", timer + 1);
        },
        nullptr);

    machine->addState(red);
    machine->addState(green);
    machine->addState(yellow);
    machine->setInitialState(red);

    machine->addTransition(red, green, [](tree::Blackboard &bb) { return bb.get<int>("timer").value_or(0) >= 3; });

    machine->addTransition(green, yellow, [](tree::Blackboard &bb) { return bb.get<int>("timer").value_or(0) >= 5; });

    machine->addTransition(yellow, red, [](tree::Blackboard &bb) { return bb.get<int>("timer").value_or(0) >= 2; });

    // Red: 3 ticks
    machine->tick(); // Enter red (timer=0)
    CHECK(machine->getCurrentStateName() == "red");

    machine->tick(); // timer=1
    machine->tick(); // timer=2
    machine->tick(); // timer=3, transition to green

    CHECK(machine->getCurrentStateName() == "green");

    // Green: 5 ticks
    for (int i = 0; i < 5; i++) {
        machine->tick();
    }
    CHECK(machine->getCurrentStateName() == "yellow");

    // Yellow: 2 ticks
    machine->tick();
    machine->tick();
    CHECK(machine->getCurrentStateName() == "red");
}

TEST_CASE("CbState: Guard blocks callback state transition") {
    auto machine = std::make_unique<state::StateMachine>();

    auto unlocked =
        state::CbState::create("unlocked", [](tree::Blackboard &bb) { bb.set<std::string>("status", "unlocked"); });

    auto locked = state::CbState::create(
        "locked",
        [](tree::Blackboard &bb) { // guard
            return bb.get<bool>("can_lock").value_or(false);
        },
        [](tree::Blackboard &bb) { // enter
            bb.set<std::string>("status", "locked");
        },
        nullptr, // update
        nullptr  // exit
    );

    machine->addState(unlocked);
    machine->addState(locked);
    machine->setInitialState(unlocked);

    machine->addTransition(unlocked, locked, [](tree::Blackboard &) {
        return true; // Always try to lock
    });

    machine->tick(); // Enter unlocked
    CHECK(machine->getCurrentStateName() == "unlocked");

    // Try to lock but guard blocks (can_lock not set)
    machine->tick();
    CHECK(machine->getCurrentStateName() == "unlocked");

    // Set can_lock and try again
    machine->blackboard().set<bool>("can_lock", true);
    machine->tick();
    CHECK(machine->getCurrentStateName() == "locked");
    CHECK(machine->blackboard().get<std::string>("status").value() == "locked");
}

TEST_CASE("CbState: Mixing CbState with regular State") {
    auto machine = std::make_unique<state::StateMachine>();

    // Regular state
    class CustomState : public state::State {
      public:
        CustomState() : State("custom") {}
        void onUpdate(tree::Blackboard &bb) override { bb.set<std::string>("type", "regular"); }
    };

    auto regularState = std::make_shared<CustomState>();
    auto callbackState =
        state::CbState::create("callback", [](tree::Blackboard &bb) { bb.set<std::string>("type", "callback"); });

    machine->addState(regularState);
    machine->addState(callbackState);
    machine->setInitialState(regularState);

    machine->addTransition(regularState, callbackState,
                           [](tree::Blackboard &bb) { return bb.get<std::string>("type").value_or("") == "regular"; });

    machine->tick(); // Enter regular state
    CHECK(machine->getCurrentStateName() == "custom");

    machine->tick(); // Update regular state
    CHECK(machine->blackboard().get<std::string>("type").value() == "regular");

    machine->tick(); // Transition to callback state
    CHECK(machine->getCurrentStateName() == "callback");

    machine->tick(); // Update callback state
    CHECK(machine->blackboard().get<std::string>("type").value() == "callback");
}

TEST_CASE("CbState: Resource management with entry/exit") {
    auto machine = std::make_unique<state::StateMachine>();

    auto working = state::CbState::create(
        "working",
        nullptr,                   // no guard
        [](tree::Blackboard &bb) { // entry - acquire resource
            bb.set<bool>("resource_acquired", true);
            bb.set<int>("work_done", 0);
        },
        [](tree::Blackboard &bb) { // update - do work
            int work = bb.get<int>("work_done").value_or(0);
            bb.set<int>("work_done", work + 1);
        },
        [](tree::Blackboard &bb) { // exit - release resource
            bb.set<bool>("resource_acquired", false);
        });

    auto idle = state::CbState::create("idle", [](tree::Blackboard &) {});

    machine->addState(working);
    machine->addState(idle);
    machine->setInitialState(working);

    machine->addTransition(working, idle,
                           [](tree::Blackboard &bb) { return bb.get<int>("work_done").value_or(0) >= 3; });

    machine->tick(); // Enter working (acquire resource)
    CHECK(machine->blackboard().get<bool>("resource_acquired").value());

    machine->tick(); // work_done = 1
    machine->tick(); // work_done = 2
    machine->tick(); // work_done = 3, transition to idle (release resource)

    CHECK(machine->getCurrentStateName() == "idle");
    CHECK_FALSE(machine->blackboard().get<bool>("resource_acquired").value());
}
