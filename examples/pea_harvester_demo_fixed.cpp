#include <bonsai/bonsai.hpp>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

using namespace bonsai::tree;

// PID Controller class for harvester components
class PIDController {
  private:
    double kp_, ki_, kd_;
    double setpoint_;
    double integral_;
    double previous_error_;
    double output_min_, output_max_;
    double current_output_;

  public:
    PIDController(double kp, double ki, double kd, double output_min, double output_max)
        : kp_(kp), ki_(ki), kd_(kd), setpoint_(0.0), integral_(0.0), previous_error_(0.0), output_min_(output_min),
          output_max_(output_max), current_output_(0.0) {}

    void setSetpoint(double setpoint) { setpoint_ = setpoint; }
    double getSetpoint() const { return setpoint_; }
    double getOutput() const { return current_output_; }

    double update(double measured_value, double dt) {
        double error = setpoint_ - measured_value;
        integral_ += error * dt;
        double derivative = (error - previous_error_) / dt;

        current_output_ = kp_ * error + ki_ * integral_ + kd_ * derivative;
        current_output_ = std::clamp(current_output_, output_min_, output_max_);

        previous_error_ = error;
        return current_output_;
    }

    bool isAtLimit() const { return current_output_ >= output_max_ - 0.01 || current_output_ <= output_min_ + 0.01; }

    double getEfficiency(double measured_value) const {
        double error = std::abs(setpoint_ - measured_value);
        return std::max(0.0, 1.0 - error / setpoint_);
    }

    void reset() {
        integral_ = 0.0;
        previous_error_ = 0.0;
        current_output_ = 0.0;
    }
};

// Harvester component simulation
struct HarvesterComponent {
    std::string name;
    double current_value;
    double target_efficiency;
    double noise_factor;
    PIDController controller;

    HarvesterComponent(const std::string &n, double initial_val, double target_eff, double noise, double kp, double ki,
                       double kd, double min_out, double max_out)
        : name(n), current_value(initial_val), target_efficiency(target_eff), noise_factor(noise),
          controller(kp, ki, kd, min_out, max_out) {}

    void update(double dt) {
        // Simulate component response to PID output
        double control_signal = controller.getOutput();
        double response = current_value + control_signal * dt * 0.1;

        // Add some noise and dynamics
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::normal_distribution<> noise_dist(0.0, noise_factor);

        current_value = response + noise_dist(gen);
        current_value = std::max(0.0, current_value);
    }

    double getEfficiency() const { return controller.getEfficiency(current_value); }

    bool isOptimal() const { return getEfficiency() > 0.95; }
};

// Pea Harvesting Machine Simulator
class PeaHarvester {
  public:
    HarvesterComponent wheel_speed{"Harvester Wheel", 100.0, 0.9, 2.0, 1.5, 0.1, 0.05, 0.0, 50.0};
    HarvesterComponent beater_speed{"Beater", 80.0, 0.85, 1.5, 2.0, 0.15, 0.1, 0.0, 40.0};
    HarvesterComponent fan_speed{"Cleaning Fan", 60.0, 0.88, 1.0, 1.8, 0.12, 0.08, 0.0, 35.0};
    HarvesterComponent sieve_opening{"Sieve Opening", 5.0, 0.92, 0.1, 0.8, 0.05, 0.02, 0.0, 3.0};

    double field_conditions; // 0.0 = poor, 1.0 = excellent
    double crop_density;     // peas per square meter
    double harvest_rate;     // kg/hour
    double loss_rate;        // percentage lost
    double fuel_consumption; // L/hour

    std::vector<HarvesterComponent *> components;

    PeaHarvester()
        : field_conditions(0.7), crop_density(150.0), harvest_rate(0.0), loss_rate(0.0), fuel_consumption(0.0) {
        components = {&wheel_speed, &beater_speed, &fan_speed, &sieve_opening};

        // Set initial setpoints based on conditions
        wheel_speed.controller.setSetpoint(120.0);
        beater_speed.controller.setSetpoint(95.0);
        fan_speed.controller.setSetpoint(75.0);
        sieve_opening.controller.setSetpoint(6.0);
    }

    void updateSimulation(double dt) {
        // Update all components
        for (auto *component : components) {
            component->controller.update(component->current_value, dt);
            component->update(dt);
        }

        // Calculate overall performance metrics
        double avg_efficiency = 0.0;
        for (const auto *component : components) {
            avg_efficiency += component->getEfficiency();
        }
        avg_efficiency /= components.size();

        // Calculate harvest metrics based on component performance
        harvest_rate = crop_density * avg_efficiency * field_conditions * 0.5; // kg/hour
        loss_rate = std::max(0.0, (1.0 - avg_efficiency) * 100.0);             // percentage

        // Fuel consumption increases with aggressive settings
        double aggressiveness = 0.0;
        for (const auto *component : components) {
            aggressiveness += component->controller.getOutput() / 50.0; // normalized
        }
        fuel_consumption = 15.0 + aggressiveness * 5.0; // base + variable consumption
    }

    void printStatus() const {
        std::cout << "\n=== Pea Harvester Status ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);

        for (const auto *component : components) {
            std::cout << component->name << ": " << component->current_value
                      << " (setpoint: " << component->controller.getSetpoint()
                      << ", output: " << component->controller.getOutput()
                      << ", eff: " << (component->getEfficiency() * 100) << "%)" << std::endl;
        }

        std::cout << "\nPerformance Metrics:" << std::endl;
        std::cout << "Harvest Rate: " << harvest_rate << " kg/hour" << std::endl;
        std::cout << "Loss Rate: " << loss_rate << "%" << std::endl;
        std::cout << "Fuel Consumption: " << fuel_consumption << " L/hour" << std::endl;
        std::cout << "Field Conditions: " << (field_conditions * 100) << "%" << std::endl;
    }

    HarvesterComponent *getWorstPerformingComponent() {
        HarvesterComponent *worst = components[0];
        for (auto *component : components) {
            if (component->getEfficiency() < worst->getEfficiency()) {
                worst = component;
            }
        }
        return worst;
    }

    HarvesterComponent *getMostAggressiveComponent() {
        HarvesterComponent *most_aggressive = components[0];
        for (auto *component : components) {
            if (component->controller.getOutput() > most_aggressive->controller.getOutput()) {
                most_aggressive = component;
            }
        }
        return most_aggressive;
    }

    bool allComponentsOptimal() const {
        for (const auto *component : components) {
            if (!component->isOptimal())
                return false;
        }
        return true;
    }

    double getOverallEfficiency() const {
        double total = 0.0;
        for (const auto *component : components) {
            total += component->getEfficiency();
        }
        return total / components.size();
    }

    HarvesterComponent *findComponent(const std::string &name) {
        for (auto *component : components) {
            if (component->name == name) {
                return component;
            }
        }
        return nullptr;
    }
};

int main() {
    std::cout << "=== Pea Harvester Behavior Tree Optimization Demo ===" << std::endl;
    std::cout << "This demo shows how behavior trees can optimize multiple PID controllers" << std::endl;
    std::cout << "in a pea harvesting machine by intelligently switching between parameters." << std::endl;

    // Create harvester instance
    auto harvester = std::make_shared<PeaHarvester>();

    // Helper for waiting
    auto wait_start = std::chrono::steady_clock::now();
    bool wait_started = false;

    // Create behavior tree for harvester optimization using lambda functions
    auto tree =
        Builder()
            .sequence()
            // Check if overall performance is already good
            .action([harvester](Blackboard &bb) -> Status {
                double efficiency = harvester->getOverallEfficiency();
                bb.set("overall_efficiency", efficiency);

                if (efficiency >= 0.95) {
                    return Status::Success;
                }
                return Status::Failure;
            })

            .inverter()
            .selector() // If not optimal, try optimization strategies

            // Strategy 1: Check and optimize components at limits
            .sequence()
            .selector()
            // Wheel at limit -> optimize beater
            .sequence()
            .action([harvester](Blackboard &bb) -> Status {
                auto *wheel = harvester->findComponent("Harvester Wheel");
                if (wheel && wheel->controller.isAtLimit()) {
                    bb.set("limited_component", std::string("Harvester Wheel"));
                    return Status::Success;
                }
                return Status::Failure;
            })
            .action([harvester](Blackboard &bb) -> Status {
                auto *beater = harvester->findComponent("Beater");
                if (beater) {
                    double current = beater->controller.getSetpoint();
                    double new_setpoint = current + 2.0;
                    beater->controller.setSetpoint(new_setpoint);
                    std::cout << "Optimizing Beater setpoint: " << current << " -> " << new_setpoint << std::endl;
                    bb.set("last_optimized", std::string("Beater"));
                    return Status::Success;
                }
                return Status::Failure;
            })
            .end()

            // Beater at limit -> optimize fan
            .sequence()
            .action([harvester](Blackboard &bb) -> Status {
                auto *beater = harvester->findComponent("Beater");
                if (beater && beater->controller.isAtLimit()) {
                    bb.set("limited_component", std::string("Beater"));
                    return Status::Success;
                }
                return Status::Failure;
            })
            .action([harvester](Blackboard &bb) -> Status {
                auto *fan = harvester->findComponent("Cleaning Fan");
                if (fan) {
                    double current = fan->controller.getSetpoint();
                    double new_setpoint = current + 1.5;
                    fan->controller.setSetpoint(new_setpoint);
                    std::cout << "Optimizing Cleaning Fan setpoint: " << current << " -> " << new_setpoint << std::endl;
                    bb.set("last_optimized", std::string("Cleaning Fan"));
                    return Status::Success;
                }
                return Status::Failure;
            })
            .end()
            .end()
            .end()

            // Strategy 2: Check fuel efficiency and reduce if needed
            .sequence()
            .action([harvester](Blackboard &bb) -> Status {
                if (harvester->fuel_consumption > 25.0) {
                    bb.set("fuel_critical", true);
                    return Status::Success;
                }
                bb.set("fuel_critical", false);
                return Status::Failure;
            })
            .action([harvester](Blackboard &bb) -> Status {
                auto *aggressive = harvester->getMostAggressiveComponent();
                if (aggressive->controller.getOutput() > 30.0) {
                    double current = aggressive->controller.getSetpoint();
                    double new_setpoint = current * 0.95; // Reduce by 5%
                    aggressive->controller.setSetpoint(new_setpoint);
                    std::cout << "Reducing aggressive " << aggressive->name << " setpoint: " << current << " -> "
                              << new_setpoint << std::endl;
                    return Status::Success;
                }
                return Status::Failure;
            })
            .end()

            // Strategy 3: Optimize worst performing component
            .action([harvester](Blackboard &bb) -> Status {
                auto *worst = harvester->getWorstPerformingComponent();
                if (worst->getEfficiency() < 0.9) {
                    double current = worst->controller.getSetpoint();
                    double new_setpoint = current + 1.0;
                    worst->controller.setSetpoint(new_setpoint);
                    std::cout << "Optimizing worst performing " << worst->name << " setpoint: " << current << " -> "
                              << new_setpoint << std::endl;
                    bb.set("last_optimized", worst->name);
                    return Status::Success;
                }
                return Status::Failure;
            })
            .end()

            // Wait and monitor before next optimization cycle
            .action([&wait_start, &wait_started](Blackboard &bb) -> Status {
                if (!wait_started) {
                    wait_start = std::chrono::steady_clock::now();
                    wait_started = true;
                }

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - wait_start).count();

                if (elapsed >= 2000) { // 2 seconds
                    wait_started = false;
                    return Status::Success;
                }

                return Status::Running;
            })
            .end()
            .build();

    // Simulation parameters
    const double dt = 0.1; // 100ms time step
    const int max_iterations = 100;
    int iteration = 0;

    std::cout << "\nStarting optimization simulation..." << std::endl;

    // Main simulation loop
    while (iteration < max_iterations && !harvester->allComponentsOptimal()) {
        // Update harvester simulation
        harvester->updateSimulation(dt);

        // Run behavior tree every 10 iterations (1 second)
        if (iteration % 10 == 0) {
            std::cout << "\n--- Optimization Cycle " << (iteration / 10 + 1) << " ---" << std::endl;

            Status result = tree.tick();
            auto &blackboard = tree.blackboard();

            std::cout << "Behavior tree result: "
                      << (result == Status::Success   ? "SUCCESS"
                          : result == Status::Failure ? "FAILURE"
                                                      : "RUNNING")
                      << std::endl;

            // Print harvester status
            harvester->printStatus();

            // Show efficiency trends
            std::cout << "\nEfficiency Analysis:" << std::endl;
            for (const auto *component : harvester->components) {
                std::cout << component->name << ": " << (component->getEfficiency() * 100) << "% "
                          << (component->controller.isAtLimit() ? "(AT LIMIT)" : "")
                          << (component->isOptimal() ? "(OPTIMAL)" : "") << std::endl;
            }

            auto last_opt = blackboard.get<std::string>("last_optimized");
            if (last_opt.has_value()) {
                std::cout << "Last optimized: " << last_opt.value() << std::endl;
            }

            auto fuel_critical = blackboard.get<bool>("fuel_critical");
            if (fuel_critical.has_value() && fuel_critical.value()) {
                std::cout << "⚠️  Fuel consumption is critical!" << std::endl;
            }
        }

        iteration++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n=== Final Results ===" << std::endl;
    harvester->printStatus();

    if (harvester->allComponentsOptimal()) {
        std::cout << "\n🎉 All components reached optimal performance!" << std::endl;
    } else {
        std::cout << "\n⏰ Simulation completed after " << max_iterations << " iterations." << std::endl;
    }

    std::cout << "\nFinal Overall Efficiency: " << (harvester->getOverallEfficiency() * 100) << "%" << std::endl;

    return 0;
}
