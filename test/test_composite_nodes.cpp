#include <stateup/stateup.hpp>
#include <doctest/doctest.h>
#include <functional>
#include <memory>

using namespace stateup::tree;

namespace {
    struct TrackingNode : Node {
        std::function<Status()> behavior;
        int haltCount = 0;

        Status tick(Blackboard &) override {
            setState(State::Running);
            Status result = behavior ? behavior() : Status::Success;
            if (result != Status::Running)
                setState(State::Idle);
            return result;
        }

        void reset() override { Node::reset(); }

        void halt() override {
            ++haltCount;
            Node::halt();
        }
    };
} // namespace

TEST_CASE("Sequence node behavior") {
    Blackboard bb;

    SUBCASE("All children succeed") {
        auto success1 = std::make_unique<Action>([](Blackboard &) { return Status::Success; });
        auto success2 = std::make_unique<Action>([](Blackboard &) { return Status::Success; });
        auto success3 = std::make_unique<Action>([](Blackboard &) { return Status::Success; });

        auto sequence = Sequence();
        sequence.addChild(std::move(success1));
        sequence.addChild(std::move(success2));
        sequence.addChild(std::move(success3));

        CHECK(sequence.tick(bb) == Status::Success);
    }

    SUBCASE("First child fails") {
        auto failure = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });
        auto success = std::make_unique<Action>([](Blackboard &) { return Status::Success; });

        auto sequence = Sequence();
        sequence.addChild(std::move(failure));
        sequence.addChild(std::move(success));

        CHECK(sequence.tick(bb) == Status::Failure);
    }

    SUBCASE("Middle child fails") {
        auto success1 = std::make_unique<Action>([](Blackboard &) { return Status::Success; });
        auto failure = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });
        auto success2 = std::make_unique<Action>([](Blackboard &) { return Status::Success; });

        auto sequence = Sequence();
        sequence.addChild(std::move(success1));
        sequence.addChild(std::move(failure));
        sequence.addChild(std::move(success2));

        CHECK(sequence.tick(bb) == Status::Failure);
    }

    SUBCASE("Running child") {
        int execution_count = 0;
        auto success = std::make_unique<Action>([](Blackboard &) { return Status::Success; });
        auto running = std::make_unique<Action>([&execution_count](Blackboard &) -> Status {
            execution_count++;
            return Status::Running;
        });
        auto not_executed = std::make_unique<Action>([](Blackboard &) {
            FAIL("This action should not be executed");
            return Status::Success;
        });

        auto sequence = Sequence();
        sequence.addChild(std::move(success));
        sequence.addChild(std::move(running));
        sequence.addChild(std::move(not_executed));

        // First tick - should execute success and running
        CHECK(sequence.tick(bb) == Status::Running);
        CHECK(execution_count == 1);

        // Second tick - should only execute running again
        CHECK(sequence.tick(bb) == Status::Running);
        CHECK(execution_count == 2);
    }
}

TEST_CASE("Selector node behavior") {
    Blackboard bb;

    SUBCASE("First child succeeds") {
        auto success = std::make_unique<Action>([](Blackboard &) { return Status::Success; });
        auto not_executed = std::make_unique<Action>([](Blackboard &) {
            FAIL("This action should not be executed");
            return Status::Failure;
        });

        auto selector = Selector();
        selector.addChild(std::move(success));
        selector.addChild(std::move(not_executed));

        CHECK(selector.tick(bb) == Status::Success);
    }

    SUBCASE("All children fail") {
        auto failure1 = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });
        auto failure2 = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });
        auto failure3 = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });

        auto selector = Selector();
        selector.addChild(std::move(failure1));
        selector.addChild(std::move(failure2));
        selector.addChild(std::move(failure3));

        CHECK(selector.tick(bb) == Status::Failure);
    }

    SUBCASE("First fails, second succeeds") {
        auto failure = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });
        auto success = std::make_unique<Action>([](Blackboard &) { return Status::Success; });
        auto not_executed = std::make_unique<Action>([](Blackboard &) {
            FAIL("This action should not be executed");
            return Status::Success;
        });

        auto selector = Selector();
        selector.addChild(std::move(failure));
        selector.addChild(std::move(success));
        selector.addChild(std::move(not_executed));

        CHECK(selector.tick(bb) == Status::Success);
    }

    SUBCASE("Running child") {
        int execution_count = 0;
        auto failure = std::make_unique<Action>([](Blackboard &) { return Status::Failure; });
        auto running = std::make_unique<Action>([&execution_count](Blackboard &) -> Status {
            execution_count++;
            return Status::Running;
        });
        auto not_executed = std::make_unique<Action>([](Blackboard &) {
            FAIL("This action should not be executed");
            return Status::Success;
        });

        auto selector = Selector();
        selector.addChild(std::move(failure));
        selector.addChild(std::move(running));
        selector.addChild(std::move(not_executed));

        // First tick - should execute failure and running
        CHECK(selector.tick(bb) == Status::Running);
        CHECK(execution_count == 1);

        // Second tick - should execute failure and running again
        CHECK(selector.tick(bb) == Status::Running);
        CHECK(execution_count == 2);
    }
}

TEST_CASE("Empty composite nodes") {
    Blackboard bb;

    SUBCASE("Empty sequence") {
        auto sequence = Sequence();
        CHECK(sequence.tick(bb) == Status::Success);
    }

    SUBCASE("Empty selector") {
        auto selector = Selector();
        CHECK(selector.tick(bb) == Status::Failure);
    }
}

TEST_CASE("Composite node reset") {
    Blackboard bb;
    int execution_count = 0;

    auto counting_action = std::make_unique<Action>([&execution_count](Blackboard &) -> Status {
        execution_count++;
        return (execution_count < 3) ? Status::Running : Status::Success;
    });

    auto sequence = Sequence();
    sequence.addChild(std::move(counting_action));

    // Execute until running
    CHECK(sequence.tick(bb) == Status::Running);
    CHECK(execution_count == 1);

    // Reset the sequence
    sequence.reset();

    // Execute again - execution_count should continue incrementing
    CHECK(sequence.tick(bb) == Status::Running);
    CHECK(execution_count == 2);

    CHECK(sequence.tick(bb) == Status::Success);
    CHECK(execution_count == 3);
}

TEST_CASE("Parallel node robustness") {
    Blackboard bb;

    SUBCASE("Halts running children when a failure policy triggers") {
        auto runningNode = std::make_shared<TrackingNode>();
        int runningTicks = 0;
        Status runningStatus = Status::Running;
        runningNode->behavior = [&]() {
            ++runningTicks;
            return runningStatus;
        };

        auto failingNode = std::make_shared<TrackingNode>();
        failingNode->behavior = []() { return Status::Failure; };

        Parallel parallel(Parallel::Policy::RequireAll, Parallel::Policy::RequireOne);
        parallel.addChild(runningNode);
        parallel.addChild(failingNode);

        CHECK(parallel.tick(bb) == Status::Failure);
        CHECK(runningNode->haltCount == 1);
        CHECK(runningTicks == 1);

        runningStatus = Status::Success;
        failingNode->behavior = []() { return Status::Success; };

        CHECK(parallel.tick(bb) == Status::Success);
    }

    SUBCASE("Tracks child success across ticks") {
        auto slowNode = std::make_shared<TrackingNode>();
        int slowTicks = 0;
        slowNode->behavior = [&]() {
            ++slowTicks;
            return (slowTicks >= 2) ? Status::Success : Status::Running;
        };

        auto fastNode = std::make_shared<TrackingNode>();
        fastNode->behavior = []() { return Status::Success; };

        Parallel parallel(Parallel::Policy::RequireAll, Parallel::Policy::RequireOne);
        parallel.addChild(slowNode);
        parallel.addChild(fastNode);

        CHECK(parallel.tick(bb) == Status::Running);
        CHECK(parallel.tick(bb) == Status::Success);
    }
}

TEST_CASE("Parallel threshold behavior") {
    Blackboard bb;

    SUBCASE("Succeeds when threshold met and halts rest") {
        Parallel parallel(2);

        auto successNode = std::make_shared<TrackingNode>();
        successNode->behavior = []() { return Status::Success; };

        auto secondSuccess = std::make_shared<TrackingNode>();
        secondSuccess->behavior = []() { return Status::Success; };

        auto runningNode = std::make_shared<TrackingNode>();
        runningNode->behavior = []() { return Status::Running; };

        parallel.addChild(runningNode);
        parallel.addChild(successNode);
        parallel.addChild(secondSuccess);

        CHECK(parallel.tick(bb) == Status::Success);
        CHECK(runningNode->haltCount == 1);
    }

    SUBCASE("Fails when failure threshold met") {
        Parallel parallel(2, 2);

        auto failNode = std::make_shared<TrackingNode>();
        failNode->behavior = []() { return Status::Failure; };

        auto secondFail = std::make_shared<TrackingNode>();
        secondFail->behavior = []() { return Status::Failure; };

        auto runningNode = std::make_shared<TrackingNode>();
        runningNode->behavior = []() { return Status::Running; };

        parallel.addChild(runningNode);
        parallel.addChild(failNode);
        parallel.addChild(secondFail);

        CHECK(parallel.tick(bb) == Status::Failure);
        CHECK(runningNode->haltCount == 1);
    }

    SUBCASE("Fails when success threshold impossible") {
        Parallel parallel(3);

        auto successNode = std::make_shared<TrackingNode>();
        successNode->behavior = []() { return Status::Success; };

        auto failNode = std::make_shared<TrackingNode>();
        failNode->behavior = []() { return Status::Failure; };

        parallel.addChild(successNode);
        parallel.addChild(failNode);

        CHECK(parallel.tick(bb) == Status::Failure);
    }
}
