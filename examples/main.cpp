#include <stateup/stateup.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <thread>

using namespace stateup::tree;

// Example PID Controller for demonstration
class PIDController {
  public:
    PIDController(double target = 10.0) : target_(target) {}

    Status update() {
        if (std::abs(value_ - target_) < 0.1) {
            std::cout << "✓ PID reached target: " << value_ << " -> " << target_ << std::endl;
            return Status::Success;
        }

        double error = target_ - value_;
        value_ += error * 0.3; // Simple proportional control
        std::cout << "⚙ PID updating: " << value_ << " (target: " << target_ << ")" << std::endl;
        return Status::Running;
    }

    void setTarget(double target) {
        target_ = target;
        std::cout << "🎯 New PID target: " << target << std::endl;
    }

    double getValue() const { return value_; }
    double getTarget() const { return target_; }
    bool hasReached() const { return std::abs(value_ - target_) < 0.1; }

  private:
    double value_ = 0.0;
    double target_ = 10.0;
};

// Simulate a simple robot state
struct RobotState {
    double battery = 100.0;
    bool isCharging = false;
    double position = 0.0;
    bool hasTarget = true;

    void update() {
        if (!isCharging) {
            battery -= 0.5; // Drain battery
        } else {
            battery = std::min(100.0, battery + 2.0); // Charge faster
        }
    }
};

int main() {
    std::cout << "🌳 Stateup Behavior Tree Demo\n" << std::endl;

    // Create our demo objects
    PIDController pid;
    RobotState robot;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // Build a complex behavior tree using the builder pattern
    auto tree = Builder()
                    .selector() // Root selector - try different behaviors

                    // High priority: charge if battery is low
                    .sequence()
                    .action([&](Blackboard &bb) -> Status {
                        if (robot.battery < 20.0) {
                            std::cout << "🔋 Battery critical: " << robot.battery << "%" << std::endl;
                            return Status::Success;
                        }
                        return Status::Failure;
                    })
                    .action([&](Blackboard &bb) -> Status {
                        std::cout << "🔌 Moving to charging station..." << std::endl;
                        robot.isCharging = true;
                        return Status::Success;
                    })
                    .action([&](Blackboard &bb) -> Status {
                        if (robot.battery >= 90.0) {
                            std::cout << "⚡ Battery charged: " << robot.battery << "%" << std::endl;
                            robot.isCharging = false;
                            return Status::Success;
                        }
                        std::cout << "⏳ Charging... " << robot.battery << "%" << std::endl;
                        return Status::Running;
                    })
                    .end()

                    // Normal operation: work with PID controller
                    .sequence()
                    .action([&](Blackboard &bb) -> Status {
                        if (!robot.hasTarget) {
                            // Generate new target
                            double newTarget = 5.0 + dis(gen) * 20.0; // Random between 5-25
                            pid.setTarget(newTarget);
                            robot.hasTarget = true;
                            bb.set("start_time", std::chrono::steady_clock::now());
                        }
                        return Status::Success;
                    })

                    .parallel(Parallel::Policy::RequireOne, Parallel::Policy::RequireAll)
                    // PID control task
                    .action([&](Blackboard &bb) -> Status { return pid.update(); })

                    // Timeout decorator - fail after 10 seconds
                    .decorator([](Status status) -> Status {
                        static auto start = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

                        if (elapsed > 10) {
                            std::cout << "⏰ Timeout reached!" << std::endl;
                            start = now; // Reset for next time
                            return Status::Failure;
                        }
                        return status;
                    })
                    .action([&](Blackboard &bb) -> Status {
                        return Status::Running; // Keep running until timeout
                    })
                    .end()

                    // Success action
                    .action([&](Blackboard &bb) -> Status {
                        std::cout << "✅ Task completed successfully!" << std::endl;
                        robot.hasTarget = false;
                        return Status::Success;
                    })
                    .end()

                    // Fallback: idle behavior
                    .action([&](Blackboard &bb) -> Status {
                        std::cout << "😴 Robot idling..." << std::endl;
                        return Status::Running;
                    })

                    .end()
                    .build();

    // Demonstration loop
    std::cout << "Starting behavior tree execution...\n" << std::endl;

    int ticks = 0;
    const int maxTicks = 100;

    while (ticks < maxTicks) {
        std::cout << "--- Tick " << (++ticks) << " ---" << std::endl;

        // Update robot state
        robot.update();

        // Store current state in blackboard
        tree.blackboard().set("battery", robot.battery);
        tree.blackboard().set("position", robot.position);
        tree.blackboard().set("is_charging", robot.isCharging);

        // Tick the behavior tree
        Status result = tree.tick();

        std::cout << "Tree status: ";
        switch (result) {
        case Status::Success:
            std::cout << "SUCCESS";
            break;
        case Status::Failure:
            std::cout << "FAILURE";
            break;
        case Status::Running:
            std::cout << "RUNNING";
            break;
        case Status::Idle:
            std::cout << "IDLE";
            break;
        }
        std::cout << std::endl;

        // Add some visual separation and delay
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Stop if tree completed
        if (result != Status::Running) {
            std::cout << "Behavior tree completed with status: " << (result == Status::Success ? "SUCCESS" : "FAILURE")
                      << std::endl;
            break;
        }
    }

    std::cout << "\n🎉 Demo completed!" << std::endl;

    // Demonstrate blackboard usage
    std::cout << "\n📋 Final Blackboard Contents:" << std::endl;
    if (auto battery = tree.blackboard().get<double>("battery")) {
        std::cout << "  Battery: " << *battery << "%" << std::endl;
    }
    if (auto position = tree.blackboard().get<double>("position")) {
        std::cout << "  Position: " << *position << std::endl;
    }

    return 0;
}
