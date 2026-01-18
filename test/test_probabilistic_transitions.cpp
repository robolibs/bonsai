#include <doctest/doctest.h>

#include "bonsai/state/builder.hpp"
#include "bonsai/state/machine.hpp"
#include <map>

using namespace bonsai::state;

TEST_CASE("Probabilistic transitions - basic probability") {
    // Test that weights affect transition selection proportionally
    std::map<std::string, int> results;

    for (int trial = 0; trial < 1000; ++trial) {
        Builder builder;
        auto machine = builder.state("start")
                           .transitionTo("a", [](auto &) { return true; })
                           .withWeight(0.7f)
                           .transitionTo("b", [](auto &) { return true; })
                           .withWeight(0.3f)
                           .state("a")
                           .state("b")
                           .initial("start")
                           .build();

        machine->tick(); // Initialize
        machine->tick(); // Transition

        results[machine->getCurrentStateName()]++;
    }

    // With 1000 trials:
    // Expected: ~700 to 'a', ~300 to 'b'
    // Allow for statistical variance (chi-square test would be better, but this is simpler)
    CHECK(results["a"] > 600); // Should be around 700
    CHECK(results["a"] < 800);
    CHECK(results["b"] > 200); // Should be around 300
}

TEST_CASE("Probabilistic transitions - weighted selection") {
    // Test weighted random selection
    std::map<std::string, int> results;

    for (int trial = 0; trial < 1000; ++trial) {
        Builder builder;
        auto machine = builder.state("start")
                           .transitionTo("heavy", [](auto &) { return true; })
                           .withWeight(7.0f)
                           .transitionTo("light", [](auto &) { return true; })
                           .withWeight(3.0f)
                           .state("heavy")
                           .state("light")
                           .initial("start")
                           .build();

        machine->tick(); // Initialize
        machine->tick(); // Transition

        results[machine->getCurrentStateName()]++;
    }

    // With weights 7:3, expect ~70% to heavy, ~30% to light
    CHECK(results["heavy"] > 600);
    CHECK(results["heavy"] < 800);
    CHECK(results["light"] > 200);
    CHECK(results["light"] < 400);
}

TEST_CASE("Probabilistic transitions - probability can fail") {
    // Low probability should sometimes not trigger
    int stayed = 0;
    int transitioned = 0;

    for (int trial = 0; trial < 100; ++trial) {
        Builder builder;
        auto machine = builder.state("start")
                           .transitionTo("end", [](auto &) { return true; })
                           .withProbability(0.1f) // Very low probability
                           .state("end")
                           .initial("start")
                           .build();

        machine->tick(); // Initialize
        machine->tick(); // Try transition

        if (machine->getCurrentStateName() == "start") {
            stayed++;
        } else {
            transitioned++;
        }
    }

    // With 10% probability, most should stay
    CHECK(stayed > 70);
    CHECK(transitioned < 30);
}

TEST_CASE("Probabilistic transitions - combined with conditions") {
    Builder builder;
    auto machine = builder.state("idle")
                       .transitionTo("active", [](auto &bb) { return bb.template get<bool>("ready").value_or(false); })
                       .withProbability(0.8f)
                       .state("active")
                       .initial("idle")
                       .build();

    machine->tick(); // Initialize

    // Condition not met - should not transition regardless of probability
    machine->tick();
    REQUIRE(machine->getCurrentStateName() == "idle");

    // Condition met - probabilistic transition
    machine->blackboard().set("ready", true);
    int transitioned = 0;
    for (int i = 0; i < 100; ++i) {
        machine->reset();
        machine->blackboard().set("ready", true);
        machine->tick(); // Initialize
        machine->tick(); // Try transition
        if (machine->getCurrentStateName() == "active") {
            transitioned++;
        }
    }

    // Should transition about 80% of the time (allow some statistical variance)
    CHECK(transitioned > 60);
    CHECK(transitioned < 100);
}

TEST_CASE("Probabilistic transitions - weights with different values") {
    std::map<std::string, int> results;

    for (int trial = 0; trial < 1000; ++trial) {
        Builder builder;
        auto machine = builder.state("start")
                           .transitionTo("rare", [](auto &) { return true; })
                           .withWeight(1.0f)
                           .transitionTo("common", [](auto &) { return true; })
                           .withWeight(9.0f)
                           .state("rare")
                           .state("common")
                           .initial("start")
                           .build();

        machine->tick(); // Initialize
        machine->tick(); // Transition

        results[machine->getCurrentStateName()]++;
    }

    // With weights 1:9, expect ~10% to rare, ~90% to common
    CHECK(results["rare"] > 50);
    CHECK(results["rare"] < 150);
    CHECK(results["common"] > 850);
    CHECK(results["common"] < 950);
}

TEST_CASE("Probabilistic transitions - non-probabilistic takes precedence") {
    // If there's a non-probabilistic transition that's valid, it should take priority
    Builder builder;
    auto machine =
        builder.state("start")
            .transitionTo("certain", [](auto &bb) { return bb.template get<bool>("certain").value_or(false); })
            .transitionTo("maybe", [](auto &) { return true; })
            .withProbability(0.9f)
            .state("certain")
            .state("maybe")
            .initial("start")
            .build();

    machine->tick(); // Initialize
    machine->blackboard().set("certain", true);

    // Non-probabilistic transition should always win
    for (int i = 0; i < 10; ++i) {
        machine->reset();
        machine->blackboard().set("certain", true);
        machine->tick(); // Initialize
        machine->tick(); // Transition
        REQUIRE(machine->getCurrentStateName() == "certain");
    }
}

TEST_CASE("Probabilistic transitions - zero weight") {
    // Transition with zero weight should never be chosen
    std::map<std::string, int> results;

    for (int trial = 0; trial < 100; ++trial) {
        Builder builder;
        auto machine = builder.state("start")
                           .transitionTo("never", [](auto &) { return true; })
                           .withWeight(0.0f)
                           .transitionTo("always", [](auto &) { return true; })
                           .withWeight(1.0f)
                           .state("never")
                           .state("always")
                           .initial("start")
                           .build();

        machine->tick(); // Initialize
        machine->tick(); // Transition

        results[machine->getCurrentStateName()]++;
    }

    CHECK(results["always"] == 100);
    CHECK(results["never"] == 0);
}

TEST_CASE("Probabilistic transitions - probability of 1.0 always transitions") {
    Builder builder;
    auto machine = builder.state("start")
                       .transitionTo("end", [](auto &) { return true; })
                       .withProbability(1.0f)
                       .state("end")
                       .initial("start")
                       .build();

    // Test multiple times to ensure consistency
    for (int i = 0; i < 10; ++i) {
        machine->reset();
        machine->tick(); // Initialize
        machine->tick(); // Transition
        REQUIRE(machine->getCurrentStateName() == "end");
    }
}

TEST_CASE("Probabilistic transitions - probability of 0.0 never transitions") {
    Builder builder;
    auto machine = builder.state("start")
                       .transitionTo("end", [](auto &) { return true; })
                       .withProbability(0.0f)
                       .state("end")
                       .initial("start")
                       .build();

    // Should never transition
    for (int i = 0; i < 10; ++i) {
        machine->reset();
        machine->tick(); // Initialize
        machine->tick(); // Try transition
        REQUIRE(machine->getCurrentStateName() == "start");
    }
}
