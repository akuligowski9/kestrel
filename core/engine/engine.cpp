#include "engine/engine.h"

#include <algorithm>

namespace kestrel {

Engine::Engine(std::size_t window_capacity)
    : window_(window_capacity) {}

void Engine::add_rule(std::unique_ptr<IRule> rule) {
    rules_.push_back(std::move(rule));
}

void Engine::process(const std::vector<SensorReading>& readings) {
    for (const auto& reading : readings) {
        window_.push(reading);

        // Initialize sensor state if first time seeing this sensor
        if (sensor_states_.find(reading.sensor_id) == sensor_states_.end()) {
            sensor_states_[reading.sensor_id] = SystemState::UNKNOWN;
        }

        SystemState new_state = evaluate_sensor(reading.sensor_id);
        if (new_state != sensor_states_[reading.sensor_id]) {
            transition(reading.sensor_id, new_state, "rule_evaluation");
        }
    }
}

SystemState Engine::sensor_state(const std::string& sensor_id) const {
    auto it = sensor_states_.find(sensor_id);
    if (it == sensor_states_.end()) {
        return SystemState::UNKNOWN;
    }
    return it->second;
}

SystemState Engine::aggregate_state() const {
    if (sensor_states_.empty()) {
        return SystemState::UNKNOWN;
    }

    // Worst state wins
    SystemState worst = SystemState::OK;
    for (const auto& [id, state] : sensor_states_) {
        if (state == SystemState::FAILED) {
            return SystemState::FAILED;
        }
        if (state == SystemState::UNKNOWN) {
            worst = SystemState::UNKNOWN;
        } else if (state == SystemState::DEGRADED && worst == SystemState::OK) {
            worst = SystemState::DEGRADED;
        }
    }
    return worst;
}

SystemState Engine::evaluate_sensor(const std::string& sensor_id) {
    auto latest = window_.latest(sensor_id);

    if (!latest.valid) {
        return SystemState::FAILED;
    }

    // Run all rules against the window
    for (const auto& rule : rules_) {
        RuleResult result = rule->evaluate(window_, sensor_id);
        if (result.severity == RuleSeverity::FAILED) {
            return SystemState::FAILED;
        }
        if (result.severity == RuleSeverity::DEGRADED) {
            return SystemState::DEGRADED;
        }
    }

    return SystemState::OK;
}

void Engine::transition(const std::string& sensor_id, SystemState new_state,
                        const std::string& reason) {
    StateTransition t;
    t.sensor_id = sensor_id;
    t.from = sensor_states_[sensor_id];
    t.to = new_state;
    t.reason = reason;
    t.timestamp = std::chrono::steady_clock::now();

    sensor_states_[sensor_id] = new_state;
    transitions_.push_back(t);
}

} // namespace kestrel
