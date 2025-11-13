#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <bonsai/bonsai.hpp>
#include <coroutine>
#include <doctest/doctest.h>

using namespace bonsai::tree;

TEST_CASE("Action node basic functionality") {
    Blackboard bb;

    SUBCASE("Success action") {
        auto success_action = Action([](Blackboard &) -> Status { return Status::Success; });

        CHECK(success_action.tick(bb) == Status::Success);
    }

    SUBCASE("Failure action") {
        auto failure_action = Action([](Blackboard &) -> Status { return Status::Failure; });

        CHECK(failure_action.tick(bb) == Status::Failure);
    }

    SUBCASE("Running action") {
        auto running_action = Action([](Blackboard &) -> Status { return Status::Running; });

        CHECK(running_action.tick(bb) == Status::Running);
    }
}

TEST_CASE("Action node with blackboard interaction") {
    Blackboard bb;

    SUBCASE("Action modifies blackboard") {
        auto counter_action = Action([](Blackboard &bb) -> Status {
            auto count = bb.get<int>("counter").value_or(0);
            bb.set("counter", count + 1);
            return Status::Success;
        });

        CHECK_FALSE(bb.has("counter"));
        counter_action.tick(bb);

        auto count = bb.get<int>("counter");
        REQUIRE(count.has_value());
        CHECK(count.value() == 1);

        // Tick again
        counter_action.tick(bb);
        count = bb.get<int>("counter");
        REQUIRE(count.has_value());
        CHECK(count.value() == 2);
    }

    SUBCASE("Action reads from blackboard") {
        bb.set("threshold", 10);
        bb.set("current_value", 5);

        auto threshold_checker = Action([](Blackboard &bb) -> Status {
            auto threshold = bb.get<int>("threshold").value_or(0);
            auto current = bb.get<int>("current_value").value_or(0);
            return (current >= threshold) ? Status::Success : Status::Failure;
        });

        CHECK(threshold_checker.tick(bb) == Status::Failure);

        bb.set("current_value", 15);
        CHECK(threshold_checker.tick(bb) == Status::Success);
    }
}

TEST_CASE("Action node state transitions") {
    Blackboard bb;
    int call_count = 0;

    auto stateful_action = Action([&call_count](Blackboard &bb) -> Status {
        call_count++;

        if (call_count == 1) {
            return Status::Running;
        } else if (call_count == 2) {
            return Status::Running;
        } else {
            return Status::Success;
        }
    });

    // First tick - should return Running
    CHECK(stateful_action.tick(bb) == Status::Running);

    // Second tick - should still return Running
    CHECK(stateful_action.tick(bb) == Status::Running);

    // Third tick - should return Success
    CHECK(stateful_action.tick(bb) == Status::Success);

    CHECK(call_count == 3);
}

TEST_CASE("Action node reset functionality") {
    Blackboard bb;
    int execution_count = 0;

    auto counting_action = Action([&execution_count](Blackboard &) -> Status {
        execution_count++;
        return Status::Success;
    });

    // Execute the action
    counting_action.tick(bb);
    CHECK(execution_count == 1);

    // Reset and execute again - reset doesn't affect lambda capture
    counting_action.reset();

    counting_action.tick(bb);
    CHECK(execution_count == 2);
}

TEST_CASE("Action node halt functionality") {
    Blackboard bb;
    int execution_count = 0;

    auto counting_action = Action([&execution_count](Blackboard &) -> Status {
        execution_count++;
        return Status::Success;
    });

    // Execute the action
    CHECK(counting_action.tick(bb) == Status::Success);
    CHECK(execution_count == 1);

    // Halt the action
    counting_action.halt();
    CHECK(counting_action.isHalted());

    // Halted action should return Failure
    CHECK(counting_action.tick(bb) == Status::Failure);
    CHECK(execution_count == 1); // Should not execute again

    // Reset should clear the halt
    counting_action.reset();
    CHECK_FALSE(counting_action.isHalted());

    CHECK(counting_action.tick(bb) == Status::Success);
    CHECK(execution_count == 2);
}

TEST_CASE("Action coroutine task progresses and completes") {
    Blackboard bb;

    auto coroutine_action = Action(Action::TaskFunc([](Blackboard &) -> bonsai::core::task<Status> {
        co_await std::suspend_always{}; // first tick -> Running
        co_await std::suspend_always{}; // second tick -> Running
        co_return Status::Success;      // third tick -> Success
    }));

    CHECK(coroutine_action.tick(bb) == Status::Running);
    CHECK(coroutine_action.tick(bb) == Status::Running);
    CHECK(coroutine_action.tick(bb) == Status::Success);
}

TEST_CASE("Action coroutine reset and halt behavior") {
    Blackboard bb;

    auto coroutine_action = Action(Action::TaskFunc([](Blackboard &) -> bonsai::core::task<Status> {
        co_await std::suspend_always{};
        co_await std::suspend_always{};
        co_return Status::Success;
    }));

    CHECK(coroutine_action.tick(bb) == Status::Running);

    // Halting should clear coroutine state and make next tick fail until reset
    coroutine_action.halt();
    CHECK(coroutine_action.isHalted());
    CHECK(coroutine_action.tick(bb) == Status::Failure);

    // Reset should allow starting over from the beginning
    coroutine_action.reset();
    CHECK_FALSE(coroutine_action.isHalted());
    CHECK(coroutine_action.tick(bb) == Status::Running);
    CHECK(coroutine_action.tick(bb) == Status::Running);
    CHECK(coroutine_action.tick(bb) == Status::Success);
}

TEST_CASE("Builder actionTask integrates coroutine actions") {
    using bonsai::core::task;

    auto tree = Builder()
                    .sequence()
                    .actionTask([](Blackboard &) -> task<Status> {
                        co_await std::suspend_always{};
                        co_return Status::Success;
                    })
                    .action([](Blackboard &) { return Status::Success; })
                    .end()
                    .build();

    // First tick: coroutine child Running => whole sequence Running
    CHECK(tree.tick() == Status::Running);
    // Second tick: coroutine child succeeds, then next action succeeds => whole sequence Success
    CHECK(tree.tick() == Status::Success);
}
