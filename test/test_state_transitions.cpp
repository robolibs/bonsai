#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "bonsai/state/builder.hpp"
#include <string>

using namespace bonsai;

TEST_CASE("Transition: Basic valid transition") {
    auto machine = state::Builder()
                       .initial("a")
                       .state("a")
                       .transitionTo("b", [](tree::Blackboard &) { return true; })
                       .state("b")
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("reached", "b"); })
                       .build();

    machine->tick(); // Enter a
    machine->tick(); // Transition to b

    CHECK(machine->getCurrentStateName() == "b");
    CHECK(machine->blackboard().get<std::string>("reached").value() == "b");
}

TEST_CASE("Transition: EVENT_IGNORED keeps state unchanged") {
    auto machine = state::Builder()
                       .initial("idle")
                       .state("idle")
                       .onEnter([](tree::Blackboard &bb) { bb.set<int>("enter_count", 0); })
                       .onUpdate([](tree::Blackboard &bb) {
                           int count = bb.get<int>("enter_count").value_or(0);
                           bb.set<int>("enter_count", count + 1);
                       })
                       .ignoreEvent() // This event is ignored
                       .build();

    machine->tick(); // Enter idle
    CHECK(machine->getCurrentStateName() == "idle");
    CHECK(machine->blackboard().get<int>("enter_count").value() == 0);

    machine->tick(); // Update idle (event ignored, stays in idle)
    CHECK(machine->getCurrentStateName() == "idle");
    CHECK(machine->blackboard().get<int>("enter_count").value() == 1);

    machine->tick(); // Still in idle
    CHECK(machine->getCurrentStateName() == "idle");
    CHECK(machine->blackboard().get<int>("enter_count").value() == 2);
}

TEST_CASE("Transition: CANNOT_HAPPEN throws error") {
    auto machine = state::Builder()
                       .initial("locked")
                       .state("locked")
                       .onUpdate([](tree::Blackboard &bb) {
                           // Simulate invalid state manipulation
                           bb.set<bool>("trigger_invalid", true);
                       })
                       .cannotHappen() // This should never happen
                       .state("invalid")
                       .build();

    machine->tick(); // Enter locked
    CHECK(machine->getCurrentStateName() == "locked");

    // This should throw because CANNOT_HAPPEN is triggered
    CHECK_THROWS_AS(machine->tick(), std::runtime_error);
}

TEST_CASE("Transition: Multiple transitions with guard selects correct one") {
    auto machine =
        state::Builder()
            .initial("start")
            .state("start")
            .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("choice", "left"); })
            .transitionTo("left",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("choice").value_or("") == "left"; })
            .transitionTo("right",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("choice").value_or("") == "right"; })
            .state("left")
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("went_left", true); })
            .state("right")
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("went_right", true); })
            .build();

    machine->tick(); // Enter start
    machine->tick(); // Choose left

    CHECK(machine->getCurrentStateName() == "left");
    CHECK(machine->blackboard().get<bool>("went_left").value());
    CHECK_FALSE(machine->blackboard().has("went_right"));
}

TEST_CASE("Transition: Complex state machine with mixed transitions") {
    auto machine =
        state::Builder()
            .initial("idle")
            .state("idle")
            .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("mode", "ready"); })
            .transitionTo("working",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("mode").value_or("") == "start"; })
            .ignoreEvent() // Ignore other events when idle
            .state("working")
            .onEnter([](tree::Blackboard &bb) { bb.set<int>("work_units", 0); })
            .onUpdate([](tree::Blackboard &bb) {
                int units = bb.get<int>("work_units").value_or(0);
                bb.set<int>("work_units", units + 1);
            })
            .transitionTo("completed", [](tree::Blackboard &bb) { return bb.get<int>("work_units").value_or(0) >= 3; })
            .state("completed")
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("done", true); })
            .cannotHappen() // Can't transition from completed
            .build();

    // Start in idle
    machine->tick();
    CHECK(machine->getCurrentStateName() == "idle");

    // Stay in idle (event ignored)
    machine->tick();
    CHECK(machine->getCurrentStateName() == "idle");

    // Start working
    machine->blackboard().set<std::string>("mode", "start");
    machine->tick();
    CHECK(machine->getCurrentStateName() == "working");

    // Work for 3 units
    machine->tick(); // work_units = 1
    machine->tick(); // work_units = 2
    machine->tick(); // work_units = 3, transition to completed

    CHECK(machine->getCurrentStateName() == "completed");
    CHECK(machine->blackboard().get<bool>("done").value());
}

TEST_CASE("Transition: Guard blocks transition even if condition is true") {
    auto machine = state::Builder()
                       .initial("unlocked")
                       .state("unlocked")
                       .onEnter([](tree::Blackboard &bb) { bb.set<bool>("can_lock", false); })
                       .transitionTo("locked",
                                     [](tree::Blackboard &bb) {
                                         return true; // Condition always true
                                     })
                       .state("locked")
                       .onGuard([](tree::Blackboard &bb) {
                           return bb.get<bool>("can_lock").value_or(false); // But guard blocks
                       })
                       .onEnter([](tree::Blackboard &bb) { bb.set<std::string>("status", "locked"); })
                       .build();

    machine->tick(); // Enter unlocked (can_lock = false)
    machine->tick(); // Try to transition - condition true, but guard blocks

    CHECK(machine->getCurrentStateName() == "unlocked");
    CHECK_FALSE(machine->blackboard().has("status"));
}

TEST_CASE("Transition: Door state machine with validation") {
    auto machine =
        state::Builder()
            .initial("closed")
            .state("closed")
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("is_locked", false); })
            .transitionTo("open",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("action").value_or("") == "open"; })
            .state("open")
            .onGuard([](tree::Blackboard &bb) {
                // Can only open if not locked
                return !bb.get<bool>("is_locked").value_or(false);
            })
            .onEnter([](tree::Blackboard &bb) { bb.set<bool>("is_open", true); })
            .transitionTo("closed",
                          [](tree::Blackboard &bb) { return bb.get<std::string>("action").value_or("") == "close"; })
            .build();

    // Door starts closed and unlocked
    machine->tick();
    CHECK(machine->getCurrentStateName() == "closed");
    CHECK_FALSE(machine->blackboard().get<bool>("is_locked").value());

    // Try to open - should succeed
    machine->blackboard().set<std::string>("action", "open");
    machine->tick();
    CHECK(machine->getCurrentStateName() == "open");
    CHECK(machine->blackboard().get<bool>("is_open").value());

    // Close the door
    machine->blackboard().set<std::string>("action", "close");
    machine->tick();
    CHECK(machine->getCurrentStateName() == "closed");

    // Lock the door
    machine->blackboard().set<bool>("is_locked", true);

    // Try to open - should fail (guard blocks)
    machine->blackboard().set<std::string>("action", "open");
    machine->tick();
    CHECK(machine->getCurrentStateName() == "closed"); // Still closed
}
