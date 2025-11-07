#include <bonsai/bonsai.hpp>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bonsai::tree;

// Test all major functionality
class ComprehensiveTest {
  public:
    void testBasicNodes() {
        std::cout << "🧪 Testing Basic Nodes..." << std::endl;

        bool actionExecuted = false;
        auto tree = Builder()
                        .sequence()
                        .action([&actionExecuted](Blackboard &) {
                            actionExecuted = true;
                            return Status::Success;
                        })
                        .end()
                        .build();

        Status result = tree.tick();
        assert(result == Status::Success);
        assert(actionExecuted);
        std::cout << "✅ Basic nodes test passed!" << std::endl;
    }

    void testBlackboard() {
        std::cout << "🧪 Testing Blackboard..." << std::endl;

        auto tree = Builder()
                        .action([](Blackboard &bb) {
                            bb.set("test_value", 42);
                            bb.set("test_string", std::string("hello"));
                            return Status::Success;
                        })
                        .build();

        tree.tick();

        auto value = tree.blackboard().get<int>("test_value");
        auto str = tree.blackboard().get<std::string>("test_string");

        assert(value && *value == 42);
        assert(str && *str == "hello");
        std::cout << "✅ Blackboard test passed!" << std::endl;
    }

    void testCompositeNodes() {
        std::cout << "🧪 Testing Composite Nodes..." << std::endl;

        // Test sequence
        int counter = 0;
        auto sequenceTree = Builder()
                                .sequence()
                                .action([&counter](Blackboard &) {
                                    counter++;
                                    return Status::Success;
                                })
                                .action([&counter](Blackboard &) {
                                    counter++;
                                    return Status::Success;
                                })
                                .action([&counter](Blackboard &) {
                                    counter++;
                                    return Status::Success;
                                })
                                .end()
                                .build();

        Status result = sequenceTree.tick();
        assert(result == Status::Success);
        assert(counter == 3);

        // Test selector
        bool firstActionExecuted = false;
        bool secondActionExecuted = false;
        auto selectorTree = Builder()
                                .selector()
                                .action([&firstActionExecuted](Blackboard &) {
                                    firstActionExecuted = true;
                                    return Status::Failure;
                                })
                                .action([&secondActionExecuted](Blackboard &) {
                                    secondActionExecuted = true;
                                    return Status::Success;
                                })
                                .end()
                                .build();

        result = selectorTree.tick();
        assert(result == Status::Success);
        assert(firstActionExecuted);
        assert(secondActionExecuted);

        std::cout << "✅ Composite nodes test passed!" << std::endl;
    }

    void testDecorators() {
        std::cout << "🧪 Testing Decorators..." << std::endl;

        // Test inverter
        auto inverterTree = Builder()
                                .inverter()
                                .action([](Blackboard &) {
                                    return Status::Failure; // This should become Success
                                })
                                .build();

        Status result = inverterTree.tick();
        assert(result == Status::Success);

        // Test succeeder
        auto succeederTree = Builder()
                                 .succeeder()
                                 .action([](Blackboard &) {
                                     return Status::Failure; // This should become Success
                                 })
                                 .build();

        result = succeederTree.tick();
        assert(result == Status::Success);

        std::cout << "✅ Decorators test passed!" << std::endl;
    }

    void testUtilityNodes() {
        std::cout << "🧪 Testing Utility Nodes..." << std::endl;

        auto utilitySelector = std::make_shared<UtilitySelector>();

        bool lowUtilityExecuted = false;
        bool highUtilityExecuted = false;

        utilitySelector->addChild(std::make_shared<Action>([&lowUtilityExecuted](Blackboard &) {
                                      lowUtilityExecuted = true;
                                      return Status::Success;
                                  }),
                                  [](Blackboard &) -> float { return 0.2f; });

        utilitySelector->addChild(std::make_shared<Action>([&highUtilityExecuted](Blackboard &) {
                                      highUtilityExecuted = true;
                                      return Status::Success;
                                  }),
                                  [](Blackboard &) -> float { return 0.8f; });

        Tree tree(utilitySelector);
        Status result = tree.tick();

        assert(result == Status::Success);
        assert(!lowUtilityExecuted);
        assert(highUtilityExecuted);

        std::cout << "✅ Utility nodes test passed!" << std::endl;
    }

    void runAllTests() {
        std::cout << "🚀 Running Comprehensive Bonsai Tests\n" << std::endl;

        testBasicNodes();
        testBlackboard();
        testCompositeNodes();
        testDecorators();
        testUtilityNodes();

        std::cout << "\n🎉 All tests passed! Bonsai is working correctly!" << std::endl;
    }
};

int main() {
    ComprehensiveTest test;
    test.runAllTests();
    return 0;
}
