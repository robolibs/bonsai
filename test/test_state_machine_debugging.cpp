#include <doctest/doctest.h>

#include "bonsai/state/builder.hpp"
#include "bonsai/state/machine.hpp"
#include <chrono>
#include <thread>
#include <vector>

using namespace bonsai::state;
using namespace std::chrono_literals;

TEST_CASE("Debugging - basic debug callback") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine =
        builder.state("idle")
            .transitionTo("active", [](auto &bb) { return bb.template get<bool>("activate").value_or(false); })
            .state("active")
            .initial("idle")
            .build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });

    machine->tick(); // Initialize - should enter idle

    // Check we got TRANSITION_TAKEN and STATE_ENTERED events
    REQUIRE(events.size() >= 2);
    CHECK(events[0].event == DebugEvent::TRANSITION_TAKEN);
    CHECK(events[0].toState == "idle");
    CHECK(events[1].event == DebugEvent::STATE_ENTERED);
    CHECK(events[1].toState == "idle");

    events.clear();
    machine->blackboard().set("activate", true);
    machine->tick(); // Should transition from idle to active

    // Should have: STATE_UPDATED, TRANSITION_TAKEN, STATE_EXITED, STATE_ENTERED
    CHECK(events.size() >= 3);

    bool foundUpdate = false;
    bool foundExit = false;
    bool foundTransition = false;
    bool foundEnter = false;

    for (const auto &event : events) {
        if (event.event == DebugEvent::STATE_UPDATED && event.fromState == "idle") {
            foundUpdate = true;
        }
        if (event.event == DebugEvent::STATE_EXITED && event.fromState == "idle") {
            foundExit = true;
        }
        if (event.event == DebugEvent::TRANSITION_TAKEN && event.toState == "active") {
            foundTransition = true;
        }
        if (event.event == DebugEvent::STATE_ENTERED && event.toState == "active") {
            foundEnter = true;
        }
    }

    CHECK(foundUpdate);
    CHECK(foundExit);
    CHECK(foundTransition);
    CHECK(foundEnter);
}

TEST_CASE("Debugging - guard rejection") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine = builder.state("start")
                       .onGuard([](auto &bb) { return bb.template get<bool>("allow").value_or(true); })
                       .state("guarded")
                       .onGuard([](auto &bb) { return bb.template get<bool>("allow_guarded").value_or(false); })
                       .transitionTo("start", [](auto &) { return true; })
                       .state("start")
                       .transitionTo("guarded", [](auto &) { return true; })
                       .initial("start")
                       .build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });

    machine->tick(); // Initialize
    events.clear();

    machine->tick(); // Try to transition to guarded, but guard should reject

    // Should have TRANSITION_REJECTED event
    bool foundRejection = false;
    for (const auto &event : events) {
        if (event.event == DebugEvent::TRANSITION_REJECTED) {
            foundRejection = true;
            CHECK(event.toState == "guarded");
            CHECK(event.guardPassed == false);
        }
    }
    CHECK(foundRejection);
}

TEST_CASE("Debugging - transition history tracking") {
    Builder builder;
    auto machine = builder.state("a")
                       .transitionTo("b", [](auto &bb) { return bb.template get<bool>("go_b").value_or(false); })
                       .state("b")
                       .transitionTo("c", [](auto &bb) { return bb.template get<bool>("go_c").value_or(false); })
                       .state("c")
                       .transitionTo("a", [](auto &bb) { return bb.template get<bool>("go_a").value_or(false); })
                       .initial("a")
                       .build();

    machine->enableTransitionHistory(true);
    machine->tick(); // Initialize to a

    // Make a series of transitions
    machine->blackboard().set("go_b", true);
    machine->tick(); // a -> b

    machine->blackboard().set("go_b", false);
    machine->blackboard().set("go_c", true);
    machine->tick(); // b -> c

    machine->blackboard().set("go_c", false);
    machine->blackboard().set("go_a", true);
    machine->tick(); // c -> a

    const auto &history = machine->getTransitionHistory();
    REQUIRE(history.size() == 4); // Initial + 3 transitions

    CHECK(history[0].fromState == "");
    CHECK(history[0].toState == "a");
    CHECK(history[0].reason == "condition");

    CHECK(history[1].fromState == "a");
    CHECK(history[1].toState == "b");
    CHECK(history[1].reason == "condition");

    CHECK(history[2].fromState == "b");
    CHECK(history[2].toState == "c");
    CHECK(history[2].reason == "condition");

    CHECK(history[3].fromState == "c");
    CHECK(history[3].toState == "a");
    CHECK(history[3].reason == "condition");
}

TEST_CASE("Debugging - timed transition reason") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine = builder.state("waiting").transitionToAfter("done", 50ms).state("done").initial("waiting").build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });
    machine->enableTransitionHistory(true);

    machine->tick(); // Initialize
    std::this_thread::sleep_for(60ms);
    events.clear();
    machine->tick(); // Timed transition should fire

    // Check transition reason
    bool foundTimedTransition = false;
    for (const auto &event : events) {
        if (event.event == DebugEvent::TRANSITION_TAKEN) {
            CHECK(event.transitionInfo == "timed");
            foundTimedTransition = true;
        }
    }
    CHECK(foundTimedTransition);

    // Check transition history
    const auto &history = machine->getTransitionHistory();
    bool foundTimedInHistory = false;
    for (const auto &record : history) {
        if (record.fromState == "waiting" && record.toState == "done") {
            CHECK(record.reason == "timed");
            foundTimedInHistory = true;
        }
    }
    CHECK(foundTimedInHistory);
}

TEST_CASE("Debugging - weighted transition reason") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine = builder.state("start")
                       .transitionTo("a", [](auto &) { return true; })
                       .withWeight(1.0f)
                       .transitionTo("b", [](auto &) { return true; })
                       .withWeight(9.0f)
                       .state("a")
                       .state("b")
                       .initial("start")
                       .build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });

    machine->tick(); // Initialize
    events.clear();
    machine->tick(); // Weighted transition

    // Check transition reason
    bool foundWeightedTransition = false;
    for (const auto &event : events) {
        if (event.event == DebugEvent::TRANSITION_TAKEN) {
            CHECK(event.transitionInfo == "weighted");
            foundWeightedTransition = true;
        }
    }
    CHECK(foundWeightedTransition);
}

TEST_CASE("Debugging - probabilistic transition reason") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine = builder.state("start")
                       .transitionTo("end", [](auto &) { return true; })
                       .withProbability(1.0f) // Always succeeds
                       .state("end")
                       .initial("start")
                       .build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });

    machine->tick(); // Initialize
    events.clear();
    machine->tick(); // Probabilistic transition

    // Check transition reason
    bool foundProbabilisticTransition = false;
    for (const auto &event : events) {
        if (event.event == DebugEvent::TRANSITION_TAKEN) {
            CHECK(event.transitionInfo == "probabilistic");
            foundProbabilisticTransition = true;
        }
    }
    CHECK(foundProbabilisticTransition);
}

TEST_CASE("Debugging - clear callbacks and history") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine = builder.state("a").transitionTo("b", [](auto &) { return true; }).state("b").initial("a").build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });
    machine->enableTransitionHistory(true);

    machine->tick(); // Initialize
    machine->tick(); // Transition

    CHECK(events.size() > 0);
    CHECK(machine->getTransitionHistory().size() > 0);

    // Clear callback
    machine->clearDebugCallback();
    events.clear();
    machine->reset();
    machine->tick();
    CHECK(events.size() == 0); // Callback should not be called

    // Clear history
    machine->clearTransitionHistory();
    CHECK(machine->getTransitionHistory().size() == 0);
}

TEST_CASE("Debugging - transition history disabled by default") {
    Builder builder;
    auto machine = builder.state("a").transitionTo("b", [](auto &) { return true; }).state("b").initial("a").build();

    CHECK_FALSE(machine->isTransitionHistoryEnabled());

    machine->tick(); // Initialize
    machine->tick(); // Transition

    // History should be empty since it's disabled
    CHECK(machine->getTransitionHistory().size() == 0);
}

TEST_CASE("Debugging - timestamp ordering") {
    std::vector<DebugInfo> events;

    Builder builder;
    auto machine = builder.state("a").transitionTo("b", [](auto &) { return true; }).state("b").initial("a").build();

    machine->setDebugCallback([&events](const DebugInfo &info) { events.push_back(info); });

    machine->tick(); // Initialize
    machine->tick(); // Transition

    // Check that timestamps are in order
    for (size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i].timestamp >= events[i - 1].timestamp);
    }
}

TEST_CASE("Debugging - history size limit") {
    Builder builder;
    auto machine = builder.state("a")
                       .transitionTo("b", [](auto &) { return true; })
                       .state("b")
                       .transitionTo("a", [](auto &) { return true; })
                       .initial("a")
                       .build();

    machine->enableTransitionHistory(true);

    // Make many transitions to test size limit
    for (int i = 0; i < 1100; ++i) {
        machine->tick();
    }

    // Should be capped at MAX_TRANSITION_HISTORY (1000)
    CHECK(machine->getTransitionHistory().size() <= 1000);
}
