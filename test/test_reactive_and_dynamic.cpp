#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <bonsai/bonsai.hpp>
#include <bonsai/tree/events.hpp>
#include <doctest/doctest.h>

using namespace bonsai::tree;

// ============================================================================
// Reactive Sequence Tests
// ============================================================================

TEST_CASE("ReactiveSequence - Basic execution") {
    Builder builder;
    builder.reactiveSequence()
        .action([](Blackboard &bb) {
            int count = bb.get<int>("count").value_or(0);
            bb.set("count", count + 1);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("done", true);
            return Status::Success;
        })
        .end();

    Tree tree = builder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("done").value_or(false) == true);
    CHECK(tree.blackboard().get<int>("count").value_or(0) == 1);
}

TEST_CASE("ReactiveSequence - Condition checking on each tick") {
    auto reactiveSeq = std::make_shared<ReactiveSequence>();

    // First child: action
    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              bb.set("step1", true);
                              return Status::Success;
                          }),
                          [](Blackboard &bb) { return bb.get<bool>("enabled").value_or(true); });

    // Second child: running action
    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              int ticks = bb.get<int>("ticks").value_or(0);
                              bb.set("ticks", ticks + 1);
                              if (ticks < 2) {
                                  return Status::Running;
                              }
                              return Status::Success;
                          }),
                          nullptr);

    Tree tree(reactiveSeq);

    // First tick: step1 succeeds, second child returns running
    tree.blackboard().set("enabled", true);
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running);
    CHECK(tree.blackboard().get<bool>("step1").value_or(false) == true);
    CHECK(tree.blackboard().get<int>("ticks").value_or(0) == 1);

    // Disable condition mid-execution
    tree.blackboard().set("enabled", false);

    // Second tick: condition fails, sequence should restart and fail
    Status result2 = tree.tick();
    CHECK(result2 == Status::Failure);
}

TEST_CASE("ReactiveSequence - Condition failure restarts sequence") {
    auto reactiveSeq = std::make_shared<ReactiveSequence>();

    // First child with condition
    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              int count = bb.get<int>("step1_count").value_or(0);
                              bb.set("step1_count", count + 1);
                              return Status::Success;
                          }),
                          [](Blackboard &bb) { return bb.get<bool>("allow_step1").value_or(true); });

    // Second child - running action
    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              int count = bb.get<int>("step2_count").value_or(0);
                              bb.set("step2_count", count + 1);
                              return Status::Running;
                          }),
                          nullptr);

    Tree tree(reactiveSeq);
    tree.blackboard().set("allow_step1", true);

    // Tick 1: step1 succeeds, step2 starts (running)
    tree.tick();
    CHECK(tree.blackboard().get<int>("step1_count").value_or(0) == 1);
    CHECK(tree.blackboard().get<int>("step2_count").value_or(0) == 1);

    // Tick 2: re-check step1 condition (still true), continue step2
    tree.tick();
    CHECK(tree.blackboard().get<int>("step1_count").value_or(0) == 1); // Not re-executed
    CHECK(tree.blackboard().get<int>("step2_count").value_or(0) == 2);

    // Disable step1 condition
    tree.blackboard().set("allow_step1", false);

    // Tick 3: step1 condition fails, sequence restarts
    Status result = tree.tick();
    CHECK(result == Status::Failure);
}

TEST_CASE("ReactiveSequence - All children succeed") {
    auto reactiveSeq = std::make_shared<ReactiveSequence>();

    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              bb.set("a", true);
                              return Status::Success;
                          }),
                          [](Blackboard &) { return true; });

    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              bb.set("b", true);
                              return Status::Success;
                          }),
                          [](Blackboard &) { return true; });

    reactiveSeq->addChild(std::make_shared<Action>([](Blackboard &bb) {
                              bb.set("c", true);
                              return Status::Success;
                          }),
                          [](Blackboard &) { return true; });

    Tree tree(reactiveSeq);
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("a").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("b").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("c").value_or(false) == true);
}

// ============================================================================
// Dynamic Selector Tests
// ============================================================================

TEST_CASE("DynamicSelector - Basic priority selection") {
    auto dynSel = std::make_shared<DynamicSelector>();

    // Low priority child
    dynSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                         bb.set("low_ran", true);
                         return Status::Success;
                     }),
                     [](Blackboard &) { return 1.0f; });

    // High priority child
    dynSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                         bb.set("high_ran", true);
                         return Status::Success;
                     }),
                     [](Blackboard &) { return 10.0f; });

    Tree tree(dynSel);
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("high_ran").value_or(false) == true);
    CHECK_FALSE(tree.blackboard().has("low_ran")); // Low priority should not run
}

TEST_CASE("DynamicSelector - Priority changes during execution") {
    auto dynSel = std::make_shared<DynamicSelector>();

    // Child A - initially high priority
    dynSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                         int ticks = bb.get<int>("a_ticks").value_or(0);
                         bb.set("a_ticks", ticks + 1);
                         if (ticks < 2) {
                             return Status::Running;
                         }
                         return Status::Success;
                     }),
                     [](Blackboard &bb) { return bb.get<float>("a_priority").value_or(10.0f); });

    // Child B - initially low priority
    dynSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                         int ticks = bb.get<int>("b_ticks").value_or(0);
                         bb.set("b_ticks", ticks + 1);
                         return Status::Success;
                     }),
                     [](Blackboard &bb) { return bb.get<float>("b_priority").value_or(1.0f); });

    Tree tree(dynSel);

    // Tick 1: A has highest priority, starts running
    tree.blackboard().set("a_priority", 10.0f);
    tree.blackboard().set("b_priority", 1.0f);
    Status result1 = tree.tick();
    CHECK(result1 == Status::Running);
    CHECK(tree.blackboard().get<int>("a_ticks").value_or(0) == 1);

    // Tick 2: Increase B's priority higher than A
    tree.blackboard().set("b_priority", 20.0f);
    Status result2 = tree.tick();

    // B should now be selected and execute
    CHECK(result2 == Status::Success);
    CHECK(tree.blackboard().get<int>("b_ticks").value_or(0) == 1);
}

TEST_CASE("DynamicSelector - All children fail") {
    auto dynSel = std::make_shared<DynamicSelector>();

    dynSel->addChild(std::make_shared<Action>([](Blackboard &) { return Status::Failure; }),
                     [](Blackboard &) { return 5.0f; });

    dynSel->addChild(std::make_shared<Action>([](Blackboard &) { return Status::Failure; }),
                     [](Blackboard &) { return 3.0f; });

    Tree tree(dynSel);
    Status result = tree.tick();

    CHECK(result == Status::Failure);
}

TEST_CASE("DynamicSelector - Empty selector fails") {
    auto dynSel = std::make_shared<DynamicSelector>();
    Tree tree(dynSel);
    Status result = tree.tick();

    CHECK(result == Status::Failure);
}

TEST_CASE("DynamicSelector - Dynamic priority re-evaluation") {
    auto dynSel = std::make_shared<DynamicSelector>();

    // Track which child is running
    dynSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                         bb.set("current", std::string("A"));
                         int count = bb.get<int>("a_count").value_or(0);
                         bb.set("a_count", count + 1);
                         return Status::Running;
                     }),
                     [](Blackboard &bb) { return bb.get<float>("a_score").value_or(0.0f); });

    dynSel->addChild(std::make_shared<Action>([](Blackboard &bb) {
                         bb.set("current", std::string("B"));
                         int count = bb.get<int>("b_count").value_or(0);
                         bb.set("b_count", count + 1);
                         return Status::Running;
                     }),
                     [](Blackboard &bb) { return bb.get<float>("b_score").value_or(0.0f); });

    Tree tree(dynSel);

    // Tick 1: A has higher priority
    tree.blackboard().set("a_score", 10.0f);
    tree.blackboard().set("b_score", 5.0f);
    tree.tick();
    CHECK(tree.blackboard().get<std::string>("current").value_or("") == "A");
    CHECK(tree.blackboard().get<int>("a_count").value_or(0) == 1);

    // Tick 2: B's priority becomes higher
    tree.blackboard().set("a_score", 3.0f);
    tree.blackboard().set("b_score", 15.0f);
    tree.tick();
    CHECK(tree.blackboard().get<std::string>("current").value_or("") == "B");
    CHECK(tree.blackboard().get<int>("b_count").value_or(0) == 1);

    // Tick 3: A's priority becomes highest again
    tree.blackboard().set("a_score", 20.0f);
    tree.tick();
    CHECK(tree.blackboard().get<std::string>("current").value_or("") == "A");
    // A should be restarted, so count increments from 1
    CHECK(tree.blackboard().get<int>("a_count").value_or(0) == 2);
}

// ============================================================================
// Subtree Tests
// ============================================================================

TEST_CASE("SubtreeNode - Basic subtree execution") {
    // Create a subtree
    Builder subtreeBuilder;
    subtreeBuilder.sequence()
        .action([](Blackboard &bb) {
            bb.set("subtree_step1", true);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("subtree_step2", true);
            return Status::Success;
        })
        .end();

    // Build to get the root
    Tree subtreeTree = subtreeBuilder.build();
    NodePtr subtree = subtreeTree.getRoot();

    // Use subtree in main tree
    Builder mainBuilder;
    mainBuilder.sequence()
        .action([](Blackboard &bb) {
            bb.set("main_start", true);
            return Status::Success;
        })
        .subtree(subtree)
        .action([](Blackboard &bb) {
            bb.set("main_end", true);
            return Status::Success;
        })
        .end();

    Tree tree = mainBuilder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("main_start").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("subtree_step1").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("subtree_step2").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("main_end").value_or(false) == true);
}

TEST_CASE("SubtreeNode - Subtree reuse") {
    // Create a reusable subtree
    Builder subtreeBuilder;
    subtreeBuilder.action([](Blackboard &bb) {
        int count = bb.get<int>("subtree_executions").value_or(0);
        bb.set("subtree_executions", count + 1);
        return Status::Success;
    });

    Tree subtreeTree = subtreeBuilder.build();
    NodePtr subtree = subtreeTree.getRoot();

    // Use the same subtree twice
    Builder mainBuilder;
    mainBuilder.sequence().subtree(subtree).subtree(subtree).end();

    Tree tree = mainBuilder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    // Subtree should be executed twice (as it's referenced twice)
    CHECK(tree.blackboard().get<int>("subtree_executions").value_or(0) == 2);
}

TEST_CASE("SubtreeNode - Subtree failure propagates") {
    // Subtree that fails
    Builder subtreeBuilder;
    subtreeBuilder.action([](Blackboard &) { return Status::Failure; });

    Tree subtreeTree = subtreeBuilder.build();
    NodePtr subtree = subtreeTree.getRoot();

    // Main tree with subtree
    Builder mainBuilder;
    mainBuilder.sequence()
        .action([](Blackboard &bb) {
            bb.set("before", true);
            return Status::Success;
        })
        .subtree(subtree)
        .action([](Blackboard &bb) {
            bb.set("after", true); // Should not run
            return Status::Success;
        })
        .end();

    Tree tree = mainBuilder.build();
    Status result = tree.tick();

    CHECK(result == Status::Failure);
    CHECK(tree.blackboard().get<bool>("before").value_or(false) == true);
    CHECK_FALSE(tree.blackboard().has("after"));
}

TEST_CASE("SubtreeNode - Null subtree fails") {
    auto subtreeNode = std::make_shared<SubtreeNode>(nullptr);
    Tree tree(subtreeNode);
    Status result = tree.tick();

    CHECK(result == Status::Failure);
}

TEST_CASE("SubtreeNode - Complex nested subtrees") {
    // Inner subtree
    Builder innerBuilder;
    innerBuilder.action([](Blackboard &bb) {
        bb.set("inner", true);
        return Status::Success;
    });
    Tree innerTree = innerBuilder.build();
    NodePtr innerSubtree = innerTree.getRoot();

    // Outer subtree containing inner subtree
    Builder outerBuilder;
    outerBuilder.sequence()
        .action([](Blackboard &bb) {
            bb.set("outer_start", true);
            return Status::Success;
        })
        .subtree(innerSubtree)
        .action([](Blackboard &bb) {
            bb.set("outer_end", true);
            return Status::Success;
        })
        .end();
    Tree outerTree = outerBuilder.build();
    NodePtr outerSubtree = outerTree.getRoot();

    // Main tree
    Builder mainBuilder;
    mainBuilder.sequence()
        .action([](Blackboard &bb) {
            bb.set("main", true);
            return Status::Success;
        })
        .subtree(outerSubtree)
        .end();

    Tree tree = mainBuilder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("main").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("outer_start").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("inner").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("outer_end").value_or(false) == true);
}

// ============================================================================
// Event System Tests
// ============================================================================

TEST_CASE("EventBus - Basic publish and subscribe") {
    EventBus bus;

    bool eventReceived = false;
    bus.subscribe("test_event", [&](const EventDataPtr &) { eventReceived = true; });

    bus.publish("test_event");

    CHECK(eventReceived == true);
}

TEST_CASE("EventBus - Event with data") {
    struct TestData : public EventData {
        int value;
        explicit TestData(int v) : value(v) {}
    };

    EventBus bus;

    int receivedValue = 0;
    bus.subscribe("data_event", [&](const EventDataPtr &data) {
        if (auto testData = std::dynamic_pointer_cast<TestData>(data)) {
            receivedValue = testData->value;
        }
    });

    auto eventData = std::make_shared<TestData>(42);
    bus.publish("data_event", eventData);

    CHECK(receivedValue == 42);
}

TEST_CASE("EventBus - Multiple subscribers") {
    EventBus bus;

    int count = 0;
    bus.subscribe("multi_event", [&](const EventDataPtr &) { count++; });
    bus.subscribe("multi_event", [&](const EventDataPtr &) { count += 10; });
    bus.subscribe("multi_event", [&](const EventDataPtr &) { count += 100; });

    bus.publish("multi_event");

    CHECK(count == 111);
}

TEST_CASE("EventBus - Unsubscribe") {
    EventBus bus;

    int count = 0;
    auto id = bus.subscribe("unsub_event", [&](const EventDataPtr &) { count++; });

    bus.publish("unsub_event");
    CHECK(count == 1);

    bus.unsubscribe("unsub_event", id);

    bus.publish("unsub_event");
    CHECK(count == 1); // Should not increment again
}

TEST_CASE("EventBus - Clear event subscribers") {
    EventBus bus;

    int count = 0;
    bus.subscribe("clear_event", [&](const EventDataPtr &) { count++; });
    bus.subscribe("clear_event", [&](const EventDataPtr &) { count++; });

    bus.clearEvent("clear_event");
    bus.publish("clear_event");

    CHECK(count == 0);
}

TEST_CASE("EventBus - Clear all events") {
    EventBus bus;

    int count1 = 0, count2 = 0;
    bus.subscribe("event1", [&](const EventDataPtr &) { count1++; });
    bus.subscribe("event2", [&](const EventDataPtr &) { count2++; });

    bus.clearAll();

    bus.publish("event1");
    bus.publish("event2");

    CHECK(count1 == 0);
    CHECK(count2 == 0);
}

TEST_CASE("Tree - Event bus integration") {
    Builder builder;
    builder.action([](Blackboard &bb) {
        bb.set("action_ran", true);
        return Status::Success;
    });

    Tree tree = builder.build();

    // Access event bus
    bool eventFired = false;
    tree.events().subscribe("custom_event", [&](const EventDataPtr &) { eventFired = true; });

    // Publish event
    tree.events().publish("custom_event");

    CHECK(eventFired == true);
}

TEST_CASE("EventBus - Non-existent event") {
    EventBus bus;

    int count = 0;
    bus.subscribe("event1", [&](const EventDataPtr &) { count++; });

    // Publishing to a different event should not trigger
    bus.publish("event2");

    CHECK(count == 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Integration - ReactiveSequence with Builder") {
    Builder builder;
    builder.reactiveSequence()
        .action([](Blackboard &bb) {
            bb.set("step1", true);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("step2", true);
            return Status::Success;
        })
        .end();

    Tree tree = builder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("step1").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("step2").value_or(false) == true);
}

TEST_CASE("Integration - DynamicSelector with Builder") {
    Builder builder;
    builder.dynamicSelector()
        .action([](Blackboard &bb) {
            bb.set("child1", true);
            return Status::Success;
        })
        .action([](Blackboard &bb) {
            bb.set("child2", true);
            return Status::Success;
        })
        .end();

    Tree tree = builder.build();
    Status result = tree.tick();

    // Both have same priority (0.0), first should win
    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("child1").value_or(false) == true);
}

TEST_CASE("Integration - Combined reactive nodes") {
    Builder builder;

    // Create a complex tree with reactive and dynamic nodes
    builder.sequence()
        .action([](Blackboard &bb) {
            bb.set("init", true);
            return Status::Success;
        })
        .reactiveSequence()
        .action([](Blackboard &bb) {
            bb.set("reactive_step", true);
            return Status::Success;
        })
        .end()
        .dynamicSelector()
        .action([](Blackboard &bb) {
            bb.set("dynamic_step", true);
            return Status::Success;
        })
        .end()
        .end();

    Tree tree = builder.build();
    Status result = tree.tick();

    CHECK(result == Status::Success);
    CHECK(tree.blackboard().get<bool>("init").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("reactive_step").value_or(false) == true);
    CHECK(tree.blackboard().get<bool>("dynamic_step").value_or(false) == true);
}
