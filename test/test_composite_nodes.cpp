#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <bonsai/bonsai.hpp>
#include <doctest/doctest.h>
#include <memory>

using namespace bonsai::tree;

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
