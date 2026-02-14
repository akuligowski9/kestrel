#include "engine/engine.h"
#include "engine/system_state.h"
#include "fault/fault_injector.h"
#include "fault/fault_profile.h"
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

static const char* fault_type_name(FaultType type) {
    switch (type) {
        case FaultType::INVALID_VALUE:     return "InvalidValue";
        case FaultType::DELAYED_READING:   return "DelayedReading";
        case FaultType::MISSING_UPDATE:    return "MissingUpdate";
        case FaultType::SPIKE:             return "Spike";
        case FaultType::INTERFACE_FAILURE: return "InterfaceFailure";
    }
    return "Unknown";
}

int main(int argc, char* argv[]) {
    std::string fault_config_path;
    std::string log_path = "kestrel.jsonl";
    double threshold = 0.95;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fault") == 0 && i + 1 < argc) {
            fault_config_path = argv[++i];
        } else if (std::strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_path = argv[++i];
        } else if (std::strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            threshold = std::stod(argv[++i]);
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

    // High-value threshold for CPU, memory, storage (high usage = bad)
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, threshold,
                    RuleSeverity::DEGRADED, "cpu_load"));
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, threshold,
                    RuleSeverity::DEGRADED, "memory"));
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, threshold,
                    RuleSeverity::DEGRADED, "storage"));

    // Low-value threshold for battery (low charge = bad, full charge = good)
    double battery_low = 1.0 - threshold; // 0.05 at Normal → alert below 5%
    engine.add_rule(std::make_unique<ThresholdRule>(battery_low, 1.0,
                    RuleSeverity::DEGRADED, "battery"));

    engine.add_rule(std::make_unique<ImplausibleValueRule>(-1.0, 200.0));
    engine.add_rule(std::make_unique<RateOfChangeRule>(0.5));
    engine.add_rule(std::make_unique<MissingDataRule>(5000ms, 15000ms));

    // Load fault profile
    FaultInjector injector;
    std::vector<FaultConfig> fault_configs;

    if (!fault_config_path.empty()) {
        fault_configs = FaultProfile::load(fault_config_path);
        std::cout << "[kestrel] loaded " << fault_configs.size()
                  << " fault(s) from " << fault_config_path << "\n";
        for (const auto& fc : fault_configs) {
            std::cout << "[kestrel]   " << fault_type_name(fc.type)
                      << " on " << fc.sensor_id
                      << " at t+" << fc.trigger_after_s << "s\n";
        }
    }

    std::cout << "[kestrel] monitoring started (Ctrl+C to stop)\n";

    auto start_time = std::chrono::steady_clock::now();
    size_t prev_transition_count = 0;

    while (running) {
        auto now = std::chrono::steady_clock::now();
        double elapsed_s = std::chrono::duration<double>(now - start_time).count();

        // Check timed fault triggers
        for (auto& fc : fault_configs) {
            // Inject when trigger time is reached
            if (!fc.triggered && elapsed_s >= fc.trigger_after_s) {
                injector.inject(fc.sensor_id, fc.type, fc.params);
                fc.triggered = true;
                fc.injected_at_s = elapsed_s;
                logger.log_fault(fc.sensor_id, fault_type_name(fc.type),
                                 fc.params.injected_value);
                std::cout << "[kestrel] FAULT INJECTED: " << fault_type_name(fc.type)
                          << " on " << fc.sensor_id
                          << " at t+" << elapsed_s << "s\n";
            }

            // Auto-clear when duration expires
            if (fc.triggered && !fc.cleared && fc.duration_s > 0 &&
                elapsed_s >= fc.injected_at_s + fc.duration_s) {
                injector.clear(fc.sensor_id);
                fc.cleared = true;
                std::cout << "[kestrel] FAULT CLEARED: " << fc.sensor_id
                          << " at t+" << elapsed_s << "s\n";
            }
        }

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
