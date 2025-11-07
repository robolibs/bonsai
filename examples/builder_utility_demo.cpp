#include <bonsai/bonsai.hpp>
#include <iostream>
#include <memory>

using namespace bonsai::tree;

// Comprehensive Builder Pattern Demo
class BuilderDemo {
  public:
    void runBasicBuilder() {
        std::cout << "🏗️ Basic Builder Pattern Demo\n" << std::endl;

        // Simple counter for demonstration
        int counter = 0;

        auto tree = Builder()
                        .sequence()
                        .action([&counter](Blackboard &) {
                            std::cout << "Action 1: Counter = " << ++counter << std::endl;
                            return Status::Success;
                        })
                        .action([&counter](Blackboard &) {
                            std::cout << "Action 2: Counter = " << ++counter << std::endl;
                            return Status::Success;
                        })
                        .action([&counter](Blackboard &) {
                            std::cout << "Action 3: Counter = " << ++counter << std::endl;
                            return Status::Success;
                        })
                        .end()
                        .build();

        std::cout << "Executing sequence:" << std::endl;
        Status result = tree.tick();
        std::cout << "Final result: " << (result == Status::Success ? "SUCCESS" : "FAILED") << std::endl;
    }

    void runDecoratorDemo() {
        std::cout << "\n\n🎭 Decorator Pattern Demo\n" << std::endl;

        bool shouldFail = true;

        auto tree = Builder()
                        .selector()
                        // First attempt - will fail
                        .action([&shouldFail](Blackboard &) {
                            std::cout << "Attempt 1: ";
                            if (shouldFail) {
                                std::cout << "FAILED" << std::endl;
                                shouldFail = false; // Next attempt will succeed
                                return Status::Failure;
                            }
                            std::cout << "SUCCESS" << std::endl;
                            return Status::Success;
                        })

                        // Second attempt with inverter decorator - will succeed
                        .inverter()
                        .action([](Blackboard &) {
                            std::cout << "Attempt 2 (inverted): FAILED -> ";
                            return Status::Failure; // This failure becomes success due to inverter
                        })

                        // Fallback
                        .action([](Blackboard &) {
                            std::cout << "Fallback: SUCCESS" << std::endl;
                            return Status::Success;
                        })
                        .end()
                        .build();

        std::cout << "Executing selector with decorators:" << std::endl;
        Status result = tree.tick();
        std::cout << "Final result: " << (result == Status::Success ? "SUCCESS" : "FAILED") << std::endl;
    }

    void runParallelDemo() {
        std::cout << "\n\n⚡ Parallel Execution Demo\n" << std::endl;

        int taskCount = 0;

        auto tree = Builder()
                        .parallel(Parallel::Policy::RequireAll, Parallel::Policy::RequireOne)
                        .action([&taskCount](Blackboard &) {
                            std::cout << "Parallel Task A executing (count: " << ++taskCount << ")" << std::endl;
                            return taskCount >= 3 ? Status::Success : Status::Running;
                        })
                        .action([&taskCount](Blackboard &) {
                            std::cout << "Parallel Task B executing (count: " << taskCount << ")" << std::endl;
                            return taskCount >= 3 ? Status::Success : Status::Running;
                        })
                        .action([&taskCount](Blackboard &) {
                            std::cout << "Parallel Task C executing (count: " << taskCount << ")" << std::endl;
                            return taskCount >= 3 ? Status::Success : Status::Running;
                        })
                        .end()
                        .build();

        std::cout << "Executing parallel tasks (RequireAll success policy):" << std::endl;

        Status result = Status::Running;
        int tick = 0;
        while (result == Status::Running && tick < 5) {
            std::cout << "\n--- Tick " << (++tick) << " ---" << std::endl;
            result = tree.tick();
            std::cout << "Tick result: "
                      << (result == Status::Success   ? "SUCCESS"
                          : result == Status::Running ? "RUNNING"
                                                      : "FAILED")
                      << std::endl;
        }
    }

    void runComplexBuilder() {
        std::cout << "\n\n🏛️ Complex Nested Builder Demo\n" << std::endl;

        auto tree = Builder()
                        .sequence()
                        .action([](Blackboard &) {
                            std::cout << "1. Initialize system" << std::endl;
                            return Status::Success;
                        })

                        .selector()
                        .sequence() // Try primary approach
                        .action([](Blackboard &) {
                            std::cout << "2a. Primary approach: Check resources" << std::endl;
                            return Status::Success;
                        })
                        .action([](Blackboard &) {
                            std::cout << "2b. Primary approach: Execute (will fail)" << std::endl;
                            return Status::Failure; // Simulate failure
                        })
                        .end()

                        .sequence() // Fallback approach
                        .action([](Blackboard &) {
                            std::cout << "3a. Fallback approach: Different strategy" << std::endl;
                            return Status::Success;
                        })
                        .repeater(2) // Try this action twice
                        .action([](Blackboard &) {
                            static int attempts = 0;
                            std::cout << "3b. Fallback approach: Attempt " << (++attempts) << std::endl;
                            return attempts >= 2 ? Status::Success : Status::Failure;
                        })
                        .end()
                        .end()

                        .action([](Blackboard &) {
                            std::cout << "4. Cleanup and finalize" << std::endl;
                            return Status::Success;
                        })
                        .end()
                        .build();

        std::cout << "Executing complex nested behavior:" << std::endl;
        Status result = tree.tick();
        std::cout << "Final result: " << (result == Status::Success ? "SUCCESS" : "FAILED") << std::endl;
    }
};

int main() {
    std::cout << "🌳 Bonsai Builder Pattern Comprehensive Demo\n" << std::endl;

    BuilderDemo demo;
    demo.runBasicBuilder();
    demo.runDecoratorDemo();
    demo.runParallelDemo();
    demo.runComplexBuilder();

    std::cout << "\n🎯 All builder demos completed!" << std::endl;
    return 0;
}
