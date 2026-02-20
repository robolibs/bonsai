#include <stateup/stateup.hpp>
#include <chrono>
#include <iostream>
#include <random>

using namespace stateup::tree;

// Demonstration of utility-based AI selection
class UtilityDemo {
  private:
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;

  public:
    UtilityDemo() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()), dist_(0.0f, 1.0f) {}

    void runUtilitySelector() {
        std::cout << "🧠 Utility Selector Demo\n" << std::endl;

        auto utilitySelector = std::make_shared<UtilitySelector>();

        // Add behaviors with different utility functions
        utilitySelector->addChild(std::make_shared<Action>([](Blackboard &) {
                                      std::cout << "🍎 Eating behavior executed" << std::endl;
                                      return Status::Success;
                                  }),
                                  [this](Blackboard &bb) -> float {
                                      // Hunger utility - higher when hungry
                                      float hunger = bb.get<float>("hunger").value_or(0.5f);
                                      return hunger;
                                  });

        utilitySelector->addChild(std::make_shared<Action>([](Blackboard &) {
                                      std::cout << "💤 Sleeping behavior executed" << std::endl;
                                      return Status::Success;
                                  }),
                                  [this](Blackboard &bb) -> float {
                                      // Tiredness utility
                                      float tiredness = bb.get<float>("tiredness").value_or(0.3f);
                                      return tiredness;
                                  });

        utilitySelector->addChild(std::make_shared<Action>([](Blackboard &) {
                                      std::cout << "⚔️ Fighting behavior executed" << std::endl;
                                      return Status::Success;
                                  }),
                                  [this](Blackboard &bb) -> float {
                                      // Aggression utility
                                      float aggression = bb.get<float>("aggression").value_or(0.2f);
                                      float enemyNear = bb.get<float>("enemy_distance").value_or(10.0f);
                                      return aggression * (1.0f / std::max(1.0f, enemyNear));
                                  });

        Tree tree(utilitySelector);

        // Simulate different scenarios
        std::vector<std::string> scenarios = {"Normal state", "Very hungry", "Very tired", "Enemy nearby",
                                              "Hungry and tired"};

        for (const auto &scenario : scenarios) {
            std::cout << "\n📋 Scenario: " << scenario << std::endl;

            // Set different blackboard values for each scenario
            if (scenario == "Very hungry") {
                tree.blackboard().set("hunger", 0.9f);
                tree.blackboard().set("tiredness", 0.2f);
                tree.blackboard().set("aggression", 0.1f);
                tree.blackboard().set("enemy_distance", 10.0f);
            } else if (scenario == "Very tired") {
                tree.blackboard().set("hunger", 0.2f);
                tree.blackboard().set("tiredness", 0.95f);
                tree.blackboard().set("aggression", 0.1f);
                tree.blackboard().set("enemy_distance", 10.0f);
            } else if (scenario == "Enemy nearby") {
                tree.blackboard().set("hunger", 0.3f);
                tree.blackboard().set("tiredness", 0.2f);
                tree.blackboard().set("aggression", 0.8f);
                tree.blackboard().set("enemy_distance", 1.5f);
            } else if (scenario == "Hungry and tired") {
                tree.blackboard().set("hunger", 0.7f);
                tree.blackboard().set("tiredness", 0.8f);
                tree.blackboard().set("aggression", 0.1f);
                tree.blackboard().set("enemy_distance", 10.0f);
            } else { // Normal state
                tree.blackboard().set("hunger", 0.4f);
                tree.blackboard().set("tiredness", 0.3f);
                tree.blackboard().set("aggression", 0.2f);
                tree.blackboard().set("enemy_distance", 5.0f);
            }

            Status result = tree.tick();
            std::cout << "Result: " << (result == Status::Success ? "SUCCESS" : "FAILED") << std::endl;
        }
    }

    void runWeightedRandomSelector() {
        std::cout << "\n\n🎲 Weighted Random Selector Demo\n" << std::endl;

        auto weightedSelector = std::make_shared<WeightedRandomSelector>();

        // Add behaviors with different weights
        weightedSelector->addChild(std::make_shared<Action>([](Blackboard &) {
                                       std::cout << "🚶 Walking patrol" << std::endl;
                                       return Status::Success;
                                   }),
                                   [](Blackboard &) -> float { return 5.0f; } // High weight - common behavior
        );

        weightedSelector->addChild(std::make_shared<Action>([](Blackboard &) {
                                       std::cout << "🔍 Investigating sound" << std::endl;
                                       return Status::Success;
                                   }),
                                   [](Blackboard &) -> float { return 2.0f; } // Medium weight
        );

        weightedSelector->addChild(std::make_shared<Action>([](Blackboard &) {
                                       std::cout << "🎉 Dancing randomly" << std::endl;
                                       return Status::Success;
                                   }),
                                   [](Blackboard &) -> float { return 0.5f; } // Low weight - rare behavior
        );

        Tree tree(weightedSelector);

        std::cout << "Running 10 random selections:" << std::endl;
        for (int i = 0; i < 10; ++i) {
            std::cout << (i + 1) << ". ";
            tree.tick();
        }
    }
};

int main() {
    std::cout << "🔧 Stateup Utility System Demo\n" << std::endl;

    UtilityDemo demo;
    demo.runUtilitySelector();
    demo.runWeightedRandomSelector();

    std::cout << "\n✨ Demo completed!" << std::endl;
    return 0;
}
