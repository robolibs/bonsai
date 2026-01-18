#include <bonsai/bonsai.hpp>
#include <bonsai/tree/nodes/advanced.hpp>
#include <doctest/doctest.h>
#include <thread>
#include <unordered_set>

using namespace bonsai::tree;

// ============================================================================
// RandomSelector Tests
// ============================================================================

TEST_CASE("RandomSelector - Basic functionality") {
    auto randSel = std::make_shared<RandomSelector>();

    randSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child1", true);
        return Status::Success;
    }));

    randSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child2", true);
        return Status::Success;
    }));

    randSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child3", true);
        return Status::Success;
    }));

    Tree tree(randSel);
    Status result = tree.tick();

    // One child should have run
    CHECK(result == Status::Success);
    int childrenRun = (tree.blackboard().has("child1") ? 1 : 0) + (tree.blackboard().has("child2") ? 1 : 0) +
                      (tree.blackboard().has("child3") ? 1 : 0);
    CHECK(childrenRun == 1);
}

TEST_CASE("RandomSelector - Distribution") {
    // Run many times to verify randomness
    std::unordered_map<std::string, int> executionCounts;

    for (int i = 0; i < 300; ++i) {
        auto randSel = std::make_shared<RandomSelector>();

        randSel->addChild(std::make_shared<Action>([&](Blackboard &) {
            executionCounts["child1"]++;
            return Status::Success;
        }));

        randSel->addChild(std::make_shared<Action>([&](Blackboard &) {
            executionCounts["child2"]++;
            return Status::Success;
        }));

        randSel->addChild(std::make_shared<Action>([&](Blackboard &) {
            executionCounts["child3"]++;
            return Status::Success;
        }));

        Tree tree(randSel);
        tree.tick();
    }

    // Each child should have been selected roughly 100 times (1/3 of 300)
    // Allow for variance: 50-150 is reasonable
    CHECK(executionCounts["child1"] > 50);
    CHECK(executionCounts["child1"] < 150);
    CHECK(executionCounts["child2"] > 50);
    CHECK(executionCounts["child2"] < 150);
    CHECK(executionCounts["child3"] > 50);
    CHECK(executionCounts["child3"] < 150);
}

TEST_CASE("RandomSelector - Empty selector fails") {
    auto randSel = std::make_shared<RandomSelector>();
    Tree tree(randSel);
    Status result = tree.tick();

    CHECK(result == Status::Failure);
}

TEST_CASE("RandomSelector - Running child") {
    auto randSel = std::make_shared<RandomSelector>();

    randSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
        int ticks = bb.get<int>("ticks").value_or(0);
        bb.set("ticks", ticks + 1);
        if (ticks < 2) {
            return Status::Running;
        }
        return Status::Success;
    }));

    Tree tree(randSel);

    // First tick: returns running
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 1);

    // Second tick: continues same child
    Status result2 = tree.tick();
    CHECK(result2 == Status::Running);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 2);

    // Third tick: completes
    Status result3 = tree.tick();
    CHECK(result3 == Status::Success);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 3);
}

TEST_CASE("RandomSelector - With Builder") {
    Builder builder;
    builder.randomSelector()
        .action([](Blackboard &bb) {
            bb.set("ran", true);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("ran", true);
            return Status::Success;
        })
        .end();

    Tree tree = builder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("ran").value_or(false) == true);
}

// ============================================================================
// ProbabilitySelector Tests
// ============================================================================

TEST_CASE("ProbabilitySelector - Basic functionality") {
    auto probSel = std::make_shared<ProbabilitySelector>();

    // High probability child
    probSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                          bb.set("high_prob", true);
                          return Status::Success;
                      }),
                      0.9f);

    // Low probability child
    probSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                          bb.set("low_prob", true);
                          return Status::Success;
                      }),
                      0.1f);

    Tree tree(probSel);
    Status result = tree.tick();

    CHECK(result == Status::Success);
    // One of them should have run
    bool oneRan = tree.blackboard().has("high_prob") || tree.blackboard().has("low_prob");
    CHECK(oneRan == true);
}

TEST_CASE("ProbabilitySelector - Distribution") {
    std::unordered_map<std::string, int> executionCounts;

    for (int i = 0; i < 1000; ++i) {
        auto probSel = std::make_shared<ProbabilitySelector>();

        // 70% probability
        probSel->addChild(std::make_shared<Action>([&](Blackboard &) {
                              executionCounts["high"]++;
                              return Status::Success;
                          }),
                          0.7f);

        // 30% probability
        probSel->addChild(std::make_shared<Action>([&](Blackboard &) {
                              executionCounts["low"]++;
                              return Status::Success;
                          }),
                          0.3f);

        Tree tree(probSel);
        tree.tick();
    }

    // High probability child should run ~70% of the time (600-800 out of 1000)
    // Low probability child should run ~30% of the time (200-400 out of 1000)
    CHECK(executionCounts["high"] > 600);
    CHECK(executionCounts["high"] < 800);
    CHECK(executionCounts["low"] > 200);
    CHECK(executionCounts["low"] < 400);
}

TEST_CASE("ProbabilitySelector - Probability clamping") {
    auto probSel = std::make_shared<ProbabilitySelector>();

    // Probabilities outside [0.0, 1.0] should be clamped
    probSel->addChild(std::make_shared<Action>([](Blackboard &) { return Status::Success; }), -0.5f); // Clamped to 0.0

    probSel->addChild(std::make_shared<Action>([](Blackboard &) { return Status::Success; }), 1.5f); // Clamped to 1.0

    Tree tree(probSel);
    // Should not crash, should execute without error
    Status result = tree.tick();
    CHECK(result == Status::Success);
}

TEST_CASE("ProbabilitySelector - Empty selector fails") {
    auto probSel = std::make_shared<ProbabilitySelector>();
    Tree tree(probSel);
    Status result = tree.tick();

    CHECK(result == Status::Failure);
}

TEST_CASE("ProbabilitySelector - With Builder") {
    Builder builder;
    builder.probabilitySelector()
        .action([](Blackboard &bb) {
            bb.set("ran", true);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("ran", true);
            return Status::Success;
        })
        .end();

    Tree tree = builder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("ran").value_or(false) == true);
}

// ============================================================================
// OneShotSequence Tests
// ============================================================================

TEST_CASE("OneShotSequence - Basic functionality") {
    auto oneShot = std::make_shared<OneShotSequence>();

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        int count = bb.get<int>("child1_count").value_or(0);
        bb.set("child1_count", count + 1);
        return Status::Success;
    }));

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        int count = bb.get<int>("child2_count").value_or(0);
        bb.set("child2_count", count + 1);
        return Status::Success;
    }));

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        int count = bb.get<int>("child3_count").value_or(0);
        bb.set("child3_count", count + 1);
        return Status::Success;
    }));

    Tree tree(oneShot);

    // First tick: all children run once
    Status result1 = tree.tick();
    CHECK(result1 == Status::Success);
    CHECK(tree.blackboard().get<int>("child1_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("child2_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("child3_count").value_or(0) == 1);

    // Second tick: no children run (already executed)
    Status result2 = tree.tick();
    CHECK(result2 == Status::Success);
    CHECK(tree.blackboard().get<int>("child1_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("child2_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("child3_count").value_or(0) == 1);

    // Third tick: still no children run
    Status result3 = tree.tick();
    CHECK(result3 == Status::Success);
    CHECK(tree.blackboard().get<int>("child1_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("child2_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("child3_count").value_or(0) == 1);
}

TEST_CASE("OneShotSequence - Clear execution history") {
    auto oneShot = std::make_shared<OneShotSequence>();

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        int count = bb.get<int>("count").value_or(0);
        bb.set("count", count + 1);
        return Status::Success;
    }));

    Tree tree(oneShot);

    // First tick
    tree.tick();
    CHECK(tree.blackboard().get<int>("count").value_or(0) == 1);

    // Second tick (no execution)
    tree.tick();
    CHECK(tree.blackboard().get<int>("count").value_or(0) == 1);

    // Clear history
    oneShot->clearExecutionHistory();

    // Third tick (should execute again)
    tree.tick();
    CHECK(tree.blackboard().get<int>("count").value_or(0) == 2);
}

TEST_CASE("OneShotSequence - Failure stops sequence") {
    auto oneShot = std::make_shared<OneShotSequence>();

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child1", true);
        return Status::Success;
    }));

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child2", true);
        return Status::Failure; // Fails
    }));

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child3", true);
        return Status::Success;
    }));

    Tree tree(oneShot);

    // First tick: fails at child2
    Status result = tree.tick();
    CHECK(result == Status::Failure);
    CHECK(tree.blackboard().get<bool>("child1").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("child2").value_or(false) == true);
    CHECK_FALSE(tree.blackboard().has("child3")); // Should not execute

    // Second tick: child1 and child2 already executed, only child3 runs
    Status result2 = tree.tick();
    CHECK(result2 == Status::Success);
    CHECK(tree.blackboard().get<bool>("child3").value_or(false) == true);
}

TEST_CASE("OneShotSequence - Running child") {
    auto oneShot = std::make_shared<OneShotSequence>();

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        bb.set("child1", true);
        return Status::Success;
    }));

    oneShot->addChild(std::make_shared<Action>([](Blackboard &bb) {
        int ticks = bb.get<int>("ticks").value_or(0);
        bb.set("ticks", ticks + 1);
        if (ticks < 2) {
            return Status::Running;
        }
        return Status::Success;
    }));

    Tree tree(oneShot);

    // First tick: child1 succeeds, child2 starts running
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 1);

    // Second tick: child2 continues (child1 not re-executed)
    Status result2 = tree.tick();
    CHECK(result2 == Status::Running);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 2);

    // Third tick: child2 completes
    Status result3 = tree.tick();
    CHECK(result3 == Status::Success);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 3);
}

TEST_CASE("OneShotSequence - With Builder") {
    Builder builder;
    builder.oneShotSequence()
        .action([](Blackboard &bb) {
            int count = bb.get<int>("count").value_or(0);
            bb.set("count", count + 1);
            return Status::Success;
        })
        .end();

    Tree tree = builder.build();

    tree.tick();
    CHECK(tree.blackboard().get<int>("count").value_or(0) == 1);

    tree.tick();
    CHECK(tree.blackboard().get<int>("count").value_or(0) == 1); // Should not increment
}

// ============================================================================
// DebounceDecorator Tests
// ============================================================================

TEST_CASE("DebounceDecorator - Basic functionality") {
    auto child = std::make_shared<Action>([](Blackboard &) { return Status::Success; });

    auto debounce = std::make_shared<DebounceDecorator>(child, std::chrono::milliseconds(50));

    Tree tree(debounce);

    // First tick: starts debounce timer
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running); // Waiting for stability

    // Immediately tick again: still waiting
    Status result2 = tree.tick();
    CHECK(result2 == Status::Running);

    // Wait for debounce time
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Now it should return the stable result
    Status result3 = tree.tick();
    CHECK(result3 == Status::Success);
}

TEST_CASE("DebounceDecorator - Result change resets timer") {
    bool returnSuccess = false;
    auto child =
        std::make_shared<Action>([&](Blackboard &) { return returnSuccess ? Status::Success : Status::Running; });

    auto debounce = std::make_shared<DebounceDecorator>(child, std::chrono::milliseconds(50));

    Tree tree(debounce);

    // Tick several times with alternating results
    for (int i = 0; i < 10; ++i) {
        returnSuccess = (i % 2 == 0); // Alternate
        Status result = tree.tick();
        CHECK(result == Status::Running); // Should keep running due to changes
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Stop changing and wait for stability
    returnSuccess = true; // Force consistent Success
    tree.tick();          // New result starts timer
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    Status result = tree.tick();
    CHECK(result == Status::Success);
}

TEST_CASE("DebounceDecorator - Null child fails") {
    auto debounce = std::make_shared<DebounceDecorator>(nullptr, std::chrono::milliseconds(50));

    Tree tree(debounce);
    Status result = tree.tick();

    CHECK(result == Status::Failure);
}

TEST_CASE("DebounceDecorator - Set and get debounce time") {
    auto child = std::make_shared<Action>([](Blackboard &) { return Status::Success; });

    auto debounce = std::make_shared<DebounceDecorator>(child, std::chrono::milliseconds(50));

    CHECK(debounce->getDebounceTime() == std::chrono::milliseconds(50));

    debounce->setDebounceTime(std::chrono::milliseconds(100));
    CHECK(debounce->getDebounceTime() == std::chrono::milliseconds(100));
}

TEST_CASE("DebounceDecorator - With Builder") {
    Builder builder;
    builder.debounce(std::chrono::milliseconds(50)).action([](Blackboard &) { return Status::Success; });

    Tree tree = builder.build();

    // First tick: waiting for stability
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running);

    // Wait and tick again
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    Status result2 = tree.tick();
    CHECK(result2 == Status::Success);
}

TEST_CASE("DebounceDecorator - Stable failure") {
    auto child = std::make_shared<Action>([](Blackboard &) { return Status::Failure; });

    auto debounce = std::make_shared<DebounceDecorator>(child, std::chrono::milliseconds(30));

    Tree tree(debounce);

    // Tick and wait for stability
    tree.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    Status result = tree.tick();
    CHECK(result == Status::Failure); // Should return stable failure
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Integration - Combined advanced nodes") {
    Builder builder;

    builder.sequence()
        .randomSelector()
        .action([](Blackboard &bb) {
            bb.set("random_ran", true);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("random_ran", true);
            return Status::Success;
        })
        .end()
        .oneShotSequence()
        .action([](Blackboard &bb) {
            int count = bb.get<int>("oneshot_count").value_or(0);
            bb.set("oneshot_count", count + 1);
            return Status::Success;
        })
        .end()
        .end();

    Tree tree = builder.build();

    // First execution
    Status result1 = tree.tick();
    CHECK(result1 == Status::Success);
    CHECK(tree.blackboard().get<bool>("random_ran").value_or(false) == true);
    CHECK(tree.blackboard().get<int>("oneshot_count").value_or(0) == 1);

    // Second execution: oneshot should not run again
    tree.blackboard().set("random_ran", false); // Reset flag
    Status result2 = tree.tick();
    CHECK(result2 == Status::Success);
    CHECK(tree.blackboard().get<bool>("random_ran").value_or(false) == true);
    CHECK(tree.blackboard().get<int>("oneshot_count").value_or(0) == 1); // Should not increment
}

TEST_CASE("Integration - Debounce with random selector") {
    Builder builder;

    builder.debounce(std::chrono::milliseconds(30))
        .randomSelector()
        .action([](Blackboard &) { return Status::Success; })
        .action([](Blackboard &) { return Status::Success; })
        .end();

    Tree tree = builder.build();

    // First tick: debounce waiting
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running);

    // Wait for debounce
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    // Should now return success
    Status result2 = tree.tick();
    CHECK(result2 == Status::Success);
}
