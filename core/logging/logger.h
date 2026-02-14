#pragma once

#include "engine/system_state.h"
#include "rules/rule.h"
#include "sensors/sensor.h"

#include <fstream>
#include <mutex>
#include <string>

namespace kestrel {

class Logger {
public:
    explicit Logger(const std::string& output_path);
    ~Logger();

    void log_reading(const SensorReading& reading);
    void log_transition(const StateTransition& transition);
    void log_fault(const std::string& sensor_id, const std::string& fault_type,
                   double injected_value);
    void log_rule_violation(const RuleResult& result);

private:
    void write_line(const std::string& json);
    std::string timestamp_iso8601() const;

    std::ofstream file_;
    std::mutex mutex_;
};

} // namespace kestrel
