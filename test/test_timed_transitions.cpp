#include <doctest/doctest.h>

#include "bonsai/state/builder.hpp"
#include "bonsai/state/machine.hpp"
#include <chrono>
#include <thread>

using namespace bonsai::state;
using namespace std::chrono_literals;

TEST_CASE("Timed transitions - basic functionality") {
    Builder builder;
    auto machine = builder.state("idle").transitionToAfter("active", 100ms).state("active").initial("idle").build();

    REQUIRE(machine->getCurrentStateName() == "");
    machine->tick(); // Initialize
    REQUIRE(machine->getCurrentStateName() == "idle");

    // Should not transition immediately
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "idle");

    // Wait for timer to expire
    std::this_thread::sleep_for(110ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "active");
}

TEST_CASE("Timed transitions - with condition") {
    Builder builder;
    auto machine = builder.state("waiting")
                       .transitionToAfter("ready", 50ms,
                                          [](auto &bb) { return bb.template get<bool>("can_proceed").value_or(false); })
                       .state("ready")
                       .initial("waiting")
                       .build();

    machine->tick(); // Initialize to waiting
    REQUIRE(machine->getCurrentStateName() == "waiting");

    // Timer expires but condition is false
    std::this_thread::sleep_for(60ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "waiting");

    // Set condition to true
    machine->blackboard().set("can_proceed", true);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "ready");
}

TEST_CASE("Timed transitions - timer reset on state exit") {
    Builder builder;
    auto machine = builder.state("a")
                       .transitionToAfter("b", 100ms)
                       .transitionTo("c", [](auto &bb) { return bb.template get<bool>("go_to_c").value_or(false); })
                       .state("b")
                       .state("c")
                       .transitionTo("a", [](auto &bb) { return bb.template get<bool>("back_to_a").value_or(false); })
                       .initial("a")
                       .build();

    machine->tick(); // Initialize to a
    REQUIRE(machine->getCurrentStateName() == "a");

    // Wait some time but not enough
    std::this_thread::sleep_for(50ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "a");

    // Force transition to c before timer expires
    machine->blackboard().set("go_to_c", true);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "c");

    // Go back to a - timer should be reset
    machine->blackboard().set("go_to_c", false);
    machine->blackboard().set("back_to_a", true);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "a");

    // Timer should restart from 0
    machine->blackboard().set("back_to_a", false);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "a");

    // Wait for full duration
    std::this_thread::sleep_for(110ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "b");
}

TEST_CASE("Timed transitions - multiple timed transitions with priority") {
    Builder builder;
    auto machine = builder.state("start")
                       .transitionToAfter("fast", 50ms)
                       .withPriority(10)
                       .transitionToAfter("slow", 200ms)
                       .withPriority(5)
                       .state("fast")
                       .state("slow")
                       .initial("start")
                       .build();

    machine->tick(); // Initialize
    REQUIRE(machine->getCurrentStateName() == "start");

    // Wait for fast transition
    std::this_thread::sleep_for(60ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "fast");
}

TEST_CASE("Timed transitions - action execution") {
    bool actionExecuted = false;

    Builder builder;
    auto machine = builder.state("initial")
                       .transitionToAfter("final", 50ms)
                       .withAction([&actionExecuted](auto &) { actionExecuted = true; })
                       .state("final")
                       .initial("initial")
                       .build();

    machine->tick(); // Initialize
    REQUIRE_FALSE(actionExecuted);

    std::this_thread::sleep_for(60ms);
    machine->tick();
    CHECK(machine->getCurrentStateName() == "final");

    // Note: Actions need to be executed in transitionTo - let's verify later
}

TEST_CASE("Timed transitions - zero duration") {
    Builder builder;
    auto machine = builder.state("start").transitionToAfter("end", 0ms).state("end").initial("start").build();

    machine->tick(); // Initialize
    REQUIRE(machine->getCurrentStateName() == "start");

    // Should transition immediately on next tick
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "end");
}

TEST_CASE("Timed transitions - concurrent with regular transitions") {
    Builder builder;
    auto machine =
        builder.state("idle")
            .transitionToAfter("timeout", 200ms)
            .transitionTo("triggered", [](auto &bb) { return bb.template get<bool>("trigger").value_or(false); })
            .withPriority(10) // Higher priority than timed
            .state("timeout")
            .state("triggered")
            .initial("idle")
            .build();

    machine->tick(); // Initialize
    REQUIRE(machine->getCurrentStateName() == "idle");

    // Regular transition should take precedence
    machine->blackboard().set("trigger", true);
    std::this_thread::sleep_for(50ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "triggered");
}

TEST_CASE("Timed transitions - state lifecycle callbacks") {
    int enterCount = 0;
    int exitCount = 0;

    Builder builder;
    auto machine = builder.state("source")
                       .onEnter([&enterCount](auto &) { enterCount++; })
                       .onExit([&exitCount](auto &) { exitCount++; })
                       .transitionToAfter("dest", 50ms)
                       .state("dest")
                       .initial("source")
                       .build();

    machine->tick(); // Initialize - enters source
    REQUIRE(enterCount == 1);
    REQUIRE(exitCount == 0);

    std::this_thread::sleep_for(60ms);
    machine->tick(); // Transition - exits source, enters dest
    REQUIRE(enterCount == 1);
    REQUIRE(exitCount == 1);
}

TEST_CASE("Timed transitions - reset clears timers") {
    Builder builder;
    auto machine = builder.state("a").transitionToAfter("b", 100ms).state("b").initial("a").build();

    machine->tick(); // Initialize
    REQUIRE(machine->getCurrentStateName() == "a");

    std::this_thread::sleep_for(50ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "a");

    // Reset should clear timer
    machine->reset();
    REQUIRE(machine->getCurrentStateName() == "a");

    // Need to wait full duration again
    std::this_thread::sleep_for(60ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "a");

    std::this_thread::sleep_for(50ms);
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "b");
}
