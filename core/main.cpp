#include "engine/engine.h"
#include "engine/system_state.h"
#include "fault/fault_injector.h"
#include "logging/logger.h"
#include "rules/implausible_value_rule.h"
#include "rules/missing_data_rule.h"
#include "rules/rate_of_change_rule.h"
#include "rules/threshold_rule.h"
#include "sensors/mac/battery_sensor.h"
#include "sensors/mac/cpu_load_sensor.h"
#include "sensors/mac/memory_sensor.h"
#include "sensors/mac/storage_sensor.h"
#include "sensors/sensor_manager.h"

#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

using namespace kestrel;
using namespace std::chrono_literals;

static volatile sig_atomic_t running = 1;

static void signal_handler(int) {
    running = 0;
}

int main(int argc, char* argv[]) {
    bool fault_mode = false;
    std::string log_path = "kestrel.jsonl";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fault") == 0) {
            fault_mode = true;
        } else if (std::strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_path = argv[++i];
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    Logger logger(log_path);

    // Register sensors
    SensorManager sensor_mgr;
    sensor_mgr.register_sensor(std::make_unique<CpuLoadSensor>(), 1000ms);
    sensor_mgr.register_sensor(std::make_unique<MemorySensor>(), 2000ms);
    sensor_mgr.register_sensor(std::make_unique<BatterySensor>(), 5000ms);
    sensor_mgr.register_sensor(std::make_unique<StorageSensor>(), 10000ms);

    // Configure engine with rules
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.95));
    engine.add_rule(std::make_unique<ImplausibleValueRule>(-1.0, 200.0));
    engine.add_rule(std::make_unique<RateOfChangeRule>(0.5));
    engine.add_rule(std::make_unique<MissingDataRule>(5000ms, 15000ms));

    // Optional fault injection
    FaultInjector injector;
    if (fault_mode) {
        std::cout << "[kestrel] fault injection enabled\n";
        // Example: inject a CPU spike after 10 seconds
        // In a full implementation, faults would be loaded from config
        injector.inject("cpu_load", FaultType::SPIKE, {0.99, 0, 0});
    }

    std::cout << "[kestrel] monitoring started (Ctrl+C to stop)\n";

    size_t prev_transition_count = 0;

    while (running) {
        auto readings = sensor_mgr.poll();

        // Apply fault injection if active
        for (auto& reading : readings) {
            reading = injector.apply(reading);
        }

        // Log raw readings
        for (const auto& reading : readings) {
            logger.log_reading(reading);
        }

        // Process through engine
        engine.process(readings);

        // Log any new state transitions
        const auto& transitions = engine.recent_transitions();
        for (size_t i = prev_transition_count; i < transitions.size(); ++i) {
            logger.log_transition(transitions[i]);
            std::cout << "[kestrel] " << transitions[i].sensor_id
                      << ": " << to_string(transitions[i].from)
                      << " -> " << to_string(transitions[i].to)
                      << " (" << transitions[i].reason << ")\n";
        }
        prev_transition_count = transitions.size();

        std::this_thread::sleep_for(500ms);
    }

    std::cout << "\n[kestrel] shutting down. aggregate state: "
              << to_string(engine.aggregate_state()) << "\n";

    return 0;
}
