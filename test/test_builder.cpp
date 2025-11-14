#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <bonsai/bonsai.hpp>
#include <doctest/doctest.h>

using namespace bonsai::tree;

TEST_CASE("Builder basic functionality") {

    SUBCASE("Simple action tree") {
        auto tree = Builder().action([](Blackboard &) { return Status::Success; }).build();

        CHECK(tree.tick() == Status::Success);
    }

    SUBCASE("Simple sequence") {
        auto tree = Builder()
                        .sequence()
                        .action([](Blackboard &) { return Status::Success; })
                        .action([](Blackboard &) { return Status::Success; })
                        .end()
                        .build();

        CHECK(tree.tick() == Status::Success);
    }

    SUBCASE("Simple selector") {
        auto tree = Builder()
                        .selector()
                        .action([](Blackboard &) { return Status::Failure; })
                        .action([](Blackboard &) { return Status::Success; })
                        .end()
                        .build();

        CHECK(tree.tick() == Status::Success);
    }
}

TEST_CASE("Builder nested structures") {
    Blackboard bb;

    SUBCASE("Nested sequence in selector") {
        auto tree = Builder()
                        .selector()
                        .sequence()
                        .action([](Blackboard &) { return Status::Failure; })
                        .action([](Blackboard &) { return Status::Success; })
                        .end()
                        .action([](Blackboard &) { return Status::Success; })
                        .end()
                        .build();

        // First child (sequence) should fail because first action fails
        // Second child (action) should succeed
        CHECK(tree.tick() == Status::Success);
    }

    SUBCASE("Nested selector in sequence") {
        auto tree = Builder()
                        .sequence()
                        .selector()
                        .action([](Blackboard &) { return Status::Failure; })
                        .action([](Blackboard &) { return Status::Success; })
                        .end()
                        .action([](Blackboard &) { return Status::Success; })
                        .end()
                        .build();

        // First child (selector) should succeed
        // Second child (action) should succeed
        // Overall sequence should succeed
        CHECK(tree.tick() == Status::Success);
    }
}

TEST_CASE("Builder with decorators") {
    Blackboard bb;

    SUBCASE("Inverter decorator") {
        auto tree = Builder().inverter().action([](Blackboard &) { return Status::Failure; }).build();

        // Failure should be inverted to Success
        CHECK(tree.tick() == Status::Success);
    }

    SUBCASE("Succeeder decorator") {
        auto tree = Builder().succeeder().action([](Blackboard &) { return Status::Failure; }).build();

        // Failure should be converted to Success
        CHECK(tree.tick() == Status::Success);
    }

    SUBCASE("Failer decorator") {
        auto tree = Builder().failer().action([](Blackboard &) { return Status::Success; }).build();

        // Success should be converted to Failure
        CHECK(tree.tick() == Status::Failure);
    }

    SUBCASE("Decorator applies to composite nodes") {
        auto tree = Builder().inverter().sequence().action([](Blackboard &) { return Status::Success; }).end().build();

        CHECK(tree.tick() == Status::Failure);
    }

    SUBCASE("Memory decorator wraps next action and remembers success") {
        int count = 0;
        auto tree = Builder()
                        .memory(MemoryNode::MemoryPolicy::REMEMBER_SUCCESSFUL)
                        .action([&](Blackboard &) {
                            count++;
                            return Status::Success;
                        })
                        .build();
        CHECK(tree.tick() == Status::Success);
        CHECK(count == 1);
        CHECK(tree.tick() == Status::Success);
        CHECK(count == 1); // not called again
    }

    SUBCASE("Memory decorator wraps next action and remembers failure") {
        int count = 0;
        auto tree = Builder()
                        .memory(MemoryNode::MemoryPolicy::REMEMBER_FAILURE)
                        .action([&](Blackboard &) {
                            count++;
                            return Status::Failure;
                        })
                        .build();
        CHECK(tree.tick() == Status::Failure);
        CHECK(count == 1);
        CHECK(tree.tick() == Status::Failure);
        CHECK(count == 1);
    }

    SUBCASE("Memory clears on reset") {
        int count = 0;
        auto tree = Builder()
                        .memory(MemoryNode::MemoryPolicy::REMEMBER_FINISHED)
                        .action([&](Blackboard &) {
                            count++;
                            return Status::Success;
                        })
                        .build();
        CHECK(tree.tick() == Status::Success);
        CHECK(tree.tick() == Status::Success);
        CHECK(count == 1);
        tree.reset();
        CHECK(tree.tick() == Status::Success);
        CHECK(count == 2);
    }

    SUBCASE("Memory can wrap composite when placed before it") {
        int count = 0;
        auto tree = Builder()
                        .memory(MemoryNode::MemoryPolicy::REMEMBER_SUCCESSFUL)
                        .sequence()
                        .action([&](Blackboard &) {
                            count++;
                            return Status::Success;
                        })
                        .end()
                        .build();
        CHECK(tree.tick() == Status::Success);
        CHECK(count == 1);
        CHECK(tree.tick() == Status::Success);
        CHECK(count == 1);
    }
}

TEST_CASE("Builder with blackboard interaction") {

    SUBCASE("Actions modify blackboard") {
        auto tree = Builder()
                        .sequence()
                        .action([](Blackboard &bb) {
                            bb.set("step", 1);
                            return Status::Success;
                        })
                        .action([](Blackboard &bb) {
                            auto step = bb.get<int>("step").value_or(0);
                            bb.set("step", step + 1);
                            return Status::Success;
                        })
                        .action([](Blackboard &bb) {
                            auto step = bb.get<int>("step").value_or(0);
                            bb.set("final_step", step);
                            return Status::Success;
                        })
                        .end()
                        .build();

        CHECK(tree.tick() == Status::Success);

        auto final_step = tree.blackboard().get<int>("final_step");
        REQUIRE(final_step.has_value());
        CHECK(final_step.value() == 2);
    }
}

TEST_CASE("Builder repeat decorator") {
    int execution_count = 0;

    SUBCASE("Repeat successful action limited times") {
        auto tree = Builder()
                        .repeat(3)
                        .action([&execution_count](Blackboard &) -> Status {
                            execution_count++;
                            return Status::Success;
                        })
                        .build();

        Status status = Status::Running;
        int safety = 0;
        while (status == Status::Running && safety++ < 10) {
            status = tree.tick();
        }

        CHECK(status == Status::Success);
        CHECK(execution_count == 3); // Should repeat 3 times then succeed
    }

    SUBCASE("Repeat successful action indefinitely until external condition") {
        execution_count = 0;
        auto tree = Builder()
                        .repeat()
                        .action([&execution_count](Blackboard &) -> Status {
                            execution_count++;
                            // After 5 executions, simulate external failure condition
                            return (execution_count < 5) ? Status::Success : Status::Failure;
                        })
                        .build();

        Status status = Status::Running;
        int iterations = 0;
        while (status == Status::Running && iterations++ < 10) {
            status = tree.tick();
        }

        CHECK(status == Status::Failure);
        CHECK(execution_count == 5);
    }

    SUBCASE("Repeat stops on failure") {
        execution_count = 0;
        auto tree = Builder()
                        .repeat(5)
                        .action([&execution_count](Blackboard &) -> Status {
                            execution_count++;
                            return (execution_count < 3) ? Status::Success : Status::Failure;
                        })
                        .build();

        Status status = Status::Running;
        int safety = 0;
        while (status == Status::Running && safety++ < 10) {
            status = tree.tick();
        }

        CHECK(status == Status::Failure);
        CHECK(execution_count == 3); // Should stop repeating when action fails
    }
}

TEST_CASE("Builder retry decorator") {
    int execution_count = 0;

    SUBCASE("Retry on failure - limited times") {
        auto tree = Builder()
                        .retry(3)
                        .action([&execution_count](Blackboard &) -> Status {
                            execution_count++;
                            return Status::Failure; // Always fail to trigger retry
                        })
                        .build();

        Status status = Status::Running;
        int safety = 0;
        while (status == Status::Running && safety++ < 10) {
            status = tree.tick();
        }

        CHECK(status == Status::Failure);
        CHECK(execution_count == 3); // Should retry 3 times then give up
    }

    SUBCASE("Retry until success") {
        execution_count = 0;
        auto tree = Builder()
                        .retry()
                        .action([&execution_count](Blackboard &) -> Status {
                            execution_count++;
                            return (execution_count < 3) ? Status::Failure : Status::Success;
                        })
                        .build();

        Status status = Status::Running;
        int safety = 0;
        while (status == Status::Running && safety++ < 10) {
            status = tree.tick();
        }

        CHECK(status == Status::Success);
        CHECK(execution_count == 3); // Should retry until success
    }
}

TEST_CASE("Builder error handling") {
    SUBCASE("Build without root") {
        Builder builder;
        CHECK_THROWS_AS(builder.build(), std::runtime_error);
    }

    SUBCASE("End without open composite throws") {
        Builder builder;
        CHECK_THROWS_AS(builder.end(), std::runtime_error);
    }

    SUBCASE("Unbalanced builder stack detected") {
        auto builder = Builder();
        builder.sequence().action([](Blackboard &) { return Status::Success; });

        CHECK_THROWS_AS(builder.build(), std::runtime_error);
    }
}

TEST_CASE("Tree halt resumes execution") {
    int execution_count = 0;

    auto tree = Builder()
                    .action([&execution_count](Blackboard &) -> Status {
                        execution_count++;
                        return Status::Running;
                    })
                    .build();

    CHECK(tree.tick() == Status::Running);
    CHECK(execution_count == 1);

    tree.halt(); // Should not permanently halt the action

    CHECK(tree.tick() == Status::Running);
    CHECK(execution_count == 2);
}

TEST_CASE("Builder warns on unused decorators") {
    SUBCASE("End with pending decorator throws") {
        Builder builder;
        builder.sequence();
        builder.inverter();
        CHECK_THROWS_AS(builder.end(), std::runtime_error);
    }

    SUBCASE("Build with pending decorator throws") {
        Builder builder;
        builder.inverter();
        CHECK_THROWS_AS(builder.build(), std::runtime_error);
    }
}

TEST_CASE("Builder parallel thresholds") {
    SUBCASE("Parallel succeeds when success threshold met") {
        auto tree = Builder()
                        .parallel(2)
                        .action([](Blackboard &) { return Status::Success; })
                        .action([](Blackboard &) { return Status::Success; })
                        .action([](Blackboard &) { return Status::Failure; })
                        .end()
                        .build();

        CHECK(tree.tick() == Status::Success);
    }

    SUBCASE("Parallel fails when failure threshold met") {
        auto tree = Builder()
                        .parallel(2, 2)
                        .action([](Blackboard &) { return Status::Failure; })
                        .action([](Blackboard &) { return Status::Failure; })
                        .action([](Blackboard &) { return Status::Running; })
                        .end()
                        .build();

        CHECK(tree.tick() == Status::Failure);
    }
}
