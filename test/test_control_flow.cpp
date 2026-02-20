#include "doctest/doctest.h"
#include <stateup/stateup.hpp>

using namespace stateup::tree;

TEST_CASE("ConditionalNode") {
    SUBCASE("Execute then branch when condition is true") {
        bool thenExecuted = false;
        bool elseExecuted = false;

        auto tree = Builder()
                        .condition([](Blackboard &bb) { return bb.get<bool>("condition").value_or(false); },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           thenExecuted = true;
                                           return Status::Success;
                                       });
                                   },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           elseExecuted = true;
                                           return Status::Success;
                                       });
                                   })
                        .build();

        tree.blackboard().set("condition", true);
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(thenExecuted == true);
        CHECK(elseExecuted == false);
    }

    SUBCASE("Execute else branch when condition is false") {
        bool thenExecuted = false;
        bool elseExecuted = false;

        auto tree = Builder()
                        .condition([](Blackboard &bb) { return bb.get<bool>("condition").value_or(false); },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           thenExecuted = true;
                                           return Status::Success;
                                       });
                                   },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           elseExecuted = true;
                                           return Status::Success;
                                       });
                                   })
                        .build();

        tree.blackboard().set("condition", false);
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(thenExecuted == false);
        CHECK(elseExecuted == true);
    }

    SUBCASE("No else branch - succeed when condition is false") {
        bool thenExecuted = false;

        auto tree = Builder()
                        .condition([](Blackboard &bb) { return bb.get<bool>("condition").value_or(false); },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           thenExecuted = true;
                                           return Status::Success;
                                       });
                                   })
                        .build();

        tree.blackboard().set("condition", false);
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(thenExecuted == false);
    }
}

TEST_CASE("WhileNode") {
    SUBCASE("Loop while condition is true") {
        int counter = 0;

        auto tree = Builder()
                        .whileLoop(
                            [](Blackboard &bb) {
                                int count = bb.get<int>("counter").value_or(0);
                                return count < 5;
                            },
                            [&](Builder &b) {
                                b.action([&](Blackboard &bb) {
                                    counter++;
                                    bb.set("counter", counter);
                                    return Status::Success;
                                });
                            })
                        .build();

        tree.blackboard().set("counter", 0);
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(counter == 5);
    }

    SUBCASE("Exit on child failure") {
        int counter = 0;

        auto tree = Builder()
                        .whileLoop([](Blackboard &) { return true; },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           counter++;
                                           return counter < 3 ? Status::Success : Status::Failure;
                                       });
                                   })
                        .build();

        Status result = tree.tick();

        CHECK(result == Status::Failure);
        CHECK(counter == 3);
    }

    SUBCASE("Respect max iterations") {
        int counter = 0;

        auto tree = Builder()
                        .whileLoop([](Blackboard &) { return true; },
                                   [&](Builder &b) {
                                       b.action([&](Blackboard &) {
                                           counter++;
                                           return Status::Success;
                                       });
                                   },
                                   3 // Max iterations
                                   )
                        .build();

        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(counter == 3);
    }
}

TEST_CASE("SwitchNode") {
    SUBCASE("Execute matching case") {
        std::string executed = "";

        auto tree = Builder()
                        .switchNode([](Blackboard &bb) { return bb.get<std::string>("case").value_or("default"); })
                        .addCase("case1",
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         executed = "case1";
                                         return Status::Success;
                                     });
                                 })
                        .addCase("case2",
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         executed = "case2";
                                         return Status::Success;
                                     });
                                 })
                        .defaultCase([&](Builder &b) {
                            b.action([&](Blackboard &) {
                                executed = "default";
                                return Status::Success;
                            });
                        })
                        .build();

        tree.blackboard().set("case", std::string("case2"));
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(executed == "case2");
    }

    SUBCASE("Execute default when no match") {
        std::string executed = "";

        auto tree = Builder()
                        .switchNode([](Blackboard &bb) { return bb.get<std::string>("case").value_or("unknown"); })
                        .addCase("case1",
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         executed = "case1";
                                         return Status::Success;
                                     });
                                 })
                        .addCase("case2",
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         executed = "case2";
                                         return Status::Success;
                                     });
                                 })
                        .defaultCase([&](Builder &b) {
                            b.action([&](Blackboard &) {
                                executed = "default";
                                return Status::Success;
                            });
                        })
                        .build();

        tree.blackboard().set("case", std::string("unknown"));
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(executed == "default");
    }

    SUBCASE("Fail when no match and no default") {
        auto switchNode = std::make_shared<SwitchNode>(
            [](Blackboard &bb) { return bb.get<std::string>("case").value_or("unknown"); });

        switchNode->addCase("case1", std::make_shared<Action>([](Blackboard &) { return Status::Success; }));

        Tree tree(switchNode);
        tree.blackboard().set("case", std::string("unknown"));
        Status result = tree.tick();

        CHECK(result == Status::Failure);
    }
}

TEST_CASE("MemoryNode") {
    SUBCASE("Remember success status") {
        int executionCount = 0;

        auto memoryNode = std::make_shared<MemoryNode>(std::make_shared<Action>([&](Blackboard &) {
                                                           executionCount++;
                                                           return Status::Success;
                                                       }),
                                                       MemoryNode::MemoryPolicy::REMEMBER_SUCCESSFUL);

        Tree tree(memoryNode);

        // First tick - execute child
        Status result1 = tree.tick();
        CHECK(result1 == Status::Success);
        CHECK(executionCount == 1);

        // Second tick - return remembered status
        Status result2 = tree.tick();
        CHECK(result2 == Status::Success);
        CHECK(executionCount == 1); // Child not executed again
    }

    SUBCASE("Remember failure status") {
        int executionCount = 0;

        auto memoryNode = std::make_shared<MemoryNode>(std::make_shared<Action>([&](Blackboard &) {
                                                           executionCount++;
                                                           return Status::Failure;
                                                       }),
                                                       MemoryNode::MemoryPolicy::REMEMBER_FAILURE);

        Tree tree(memoryNode);

        // First tick - execute child
        Status result1 = tree.tick();
        CHECK(result1 == Status::Failure);
        CHECK(executionCount == 1);

        // Second tick - return remembered status
        Status result2 = tree.tick();
        CHECK(result2 == Status::Failure);
        CHECK(executionCount == 1); // Child not executed again
    }

    SUBCASE("Clear memory on reset") {
        int executionCount = 0;

        auto memoryNode = std::make_shared<MemoryNode>(std::make_shared<Action>([&](Blackboard &) {
                                                           executionCount++;
                                                           return Status::Success;
                                                       }),
                                                       MemoryNode::MemoryPolicy::REMEMBER_FINISHED);

        Tree tree(memoryNode);

        // First execution
        tree.tick();
        CHECK(executionCount == 1);

        // Second execution - uses memory
        tree.tick();
        CHECK(executionCount == 1);

        // Reset and tick again
        tree.reset();
        tree.tick();
        CHECK(executionCount == 2); // Child executed again after reset
    }
}

TEST_CASE("ForNode") {
    SUBCASE("Execute fixed number of times") {
        int counter = 0;

        auto tree = Builder()
                        .forLoop(5,
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         counter++;
                                         return Status::Success;
                                     });
                                 })
                        .build();

        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(counter == 5);
    }

    SUBCASE("Dynamic count from blackboard") {
        int counter = 0;

        auto tree = Builder()
                        .forLoop([](Blackboard &bb) { return bb.get<int>("iterations").value_or(0); },
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         counter++;
                                         return Status::Success;
                                     });
                                 })
                        .build();

        tree.blackboard().set("iterations", 3);
        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(counter == 3);
    }

    SUBCASE("Stop on child failure") {
        int counter = 0;

        auto tree = Builder()
                        .forLoop(10,
                                 [&](Builder &b) {
                                     b.action([&](Blackboard &) {
                                         counter++;
                                         return counter <= 3 ? Status::Success : Status::Failure;
                                     });
                                 })
                        .build();

        Status result = tree.tick();

        CHECK(result == Status::Failure);
        CHECK(counter == 4);
    }
}

TEST_CASE("ConditionalSequence") {
    SUBCASE("Execute all children when conditions pass") {
        std::vector<int> executed;

        auto condSeq = std::make_shared<ConditionalSequence>();

        condSeq->addChild(std::make_shared<Action>([&](Blackboard &) {
                              executed.push_back(1);
                              return Status::Success;
                          }),
                          [](Blackboard &bb) { return bb.get<bool>("cond1").value_or(false); });

        condSeq->addChild(std::make_shared<Action>([&](Blackboard &) {
                              executed.push_back(2);
                              return Status::Success;
                          }),
                          [](Blackboard &bb) { return bb.get<bool>("cond2").value_or(false); });

        condSeq->addChild(std::make_shared<Action>([&](Blackboard &) {
            executed.push_back(3);
            return Status::Success;
        })
                          // No condition for this one
        );

        Tree tree(condSeq);
        tree.blackboard().set("cond1", true);
        tree.blackboard().set("cond2", true);

        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(executed == std::vector<int>{1, 2, 3});
    }

    SUBCASE("Stop on failed condition") {
        std::vector<int> executed;

        auto condSeq = std::make_shared<ConditionalSequence>();

        condSeq->addChild(std::make_shared<Action>([&](Blackboard &) {
                              executed.push_back(1);
                              return Status::Success;
                          }),
                          [](Blackboard &bb) { return bb.get<bool>("cond1").value_or(false); });

        condSeq->addChild(std::make_shared<Action>([&](Blackboard &) {
                              executed.push_back(2);
                              return Status::Success;
                          }),
                          [](Blackboard &bb) { return bb.get<bool>("cond2").value_or(false); });

        condSeq->addChild(std::make_shared<Action>([&](Blackboard &) {
            executed.push_back(3);
            return Status::Success;
        }));

        Tree tree(condSeq);
        tree.blackboard().set("cond1", true);
        tree.blackboard().set("cond2", false); // This will fail

        Status result = tree.tick();

        CHECK(result == Status::Failure);
        CHECK(executed == std::vector<int>{1}); // Only first child executed
    }
}

TEST_CASE("Complex control flow integration") {
    SUBCASE("Nested control structures") {
        std::vector<std::string> log;

        auto tree =
            Builder()
                .sequence()
                // First, a conditional
                .condition([](Blackboard &bb) { return bb.get<int>("mode").value_or(0) == 1; },
                           [&](Builder &b) {
                               // Then branch: while loop
                               b.whileLoop([](Blackboard &bb) { return bb.get<int>("counter").value_or(0) < 3; },
                                           [&](Builder &b) {
                                               b.action([&](Blackboard &bb) {
                                                   int counter = bb.get<int>("counter").value_or(0);
                                                   log.push_back("while_" + std::to_string(counter));
                                                   bb.set("counter", counter + 1);
                                                   return Status::Success;
                                               });
                                           });
                           },
                           [&](Builder &b) {
                               // Else branch: for loop
                               b.forLoop(2, [&](Builder &b) {
                                   b.action([&](Blackboard &) {
                                       log.push_back("for");
                                       return Status::Success;
                                   });
                               });
                           })
                // Then a switch
                .switchNode([](Blackboard &bb) { return bb.get<std::string>("action").value_or("default"); })
                .addCase("action1",
                         [&](Builder &b) {
                             b.action([&](Blackboard &) {
                                 log.push_back("action1");
                                 return Status::Success;
                             });
                         })
                .addCase("action2",
                         [&](Builder &b) {
                             b.action([&](Blackboard &) {
                                 log.push_back("action2");
                                 return Status::Success;
                             });
                         })
                .defaultCase([&](Builder &b) {
                    b.action([&](Blackboard &) {
                        log.push_back("default_action");
                        return Status::Success;
                    });
                })
                .end()
                .build();

        // Test with mode=1 (while branch) and action2
        tree.blackboard().set("mode", 1);
        tree.blackboard().set("counter", 0);
        tree.blackboard().set("action", std::string("action2"));

        Status result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(log == std::vector<std::string>{"while_0", "while_1", "while_2", "action2"});

        // Reset and test with mode=0 (for branch) and default action
        log.clear();
        tree.reset();
        tree.blackboard().set("mode", 0);
        tree.blackboard().set("action", std::string("unknown"));

        result = tree.tick();

        CHECK(result == Status::Success);
        CHECK(log == std::vector<std::string>{"for", "for", "default_action"});
    }
}