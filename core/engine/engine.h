#pragma once

#include "engine/measurement_window.h"
#include "engine/system_state.h"
#include "rules/rule.h"
#include "sensors/sensor.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace kestrel {

class Engine {
public:
    explicit Engine(std::size_t window_capacity = 64);

    void add_rule(std::unique_ptr<IRule> rule);

    void process(const std::vector<SensorReading>& readings);

    SystemState sensor_state(const std::string& sensor_id) const;
    SystemState aggregate_state() const;

    const std::vector<StateTransition>& recent_transitions() const {
        return transitions_;
    }

    const MeasurementWindow& window() const { return window_; }

private:
    SystemState evaluate_sensor(const std::string& sensor_id);
    void transition(const std::string& sensor_id, SystemState new_state,
                    const std::string& reason);

    MeasurementWindow window_;
    std::vector<std::unique_ptr<IRule>> rules_;
    std::unordered_map<std::string, SystemState> sensor_states_;
    std::vector<StateTransition> transitions_;
};

} // namespace kestrel
