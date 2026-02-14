#include "logging/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace kestrel {

Logger::Logger(const std::string& output_path) {
    if (!output_path.empty()) {
        file_.open(output_path, std::ios::app);
    }
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::log_reading(const SensorReading& reading) {
    nlohmann::json j;
    j["ts"] = timestamp_iso8601();
    j["type"] = "reading";
    j["sensor"] = reading.sensor_id;
    j["value"] = reading.value;
    j["valid"] = reading.valid;
    write_line(j.dump());
}

void Logger::log_transition(const StateTransition& transition) {
    nlohmann::json j;
    j["ts"] = timestamp_iso8601();
    j["type"] = "transition";
    j["sensor"] = transition.sensor_id;
    j["from"] = to_string(transition.from);
    j["to"] = to_string(transition.to);
    j["reason"] = transition.reason;
    write_line(j.dump());
}

void Logger::log_fault(const std::string& sensor_id,
                       const std::string& fault_type,
                       double injected_value) {
    nlohmann::json j;
    j["ts"] = timestamp_iso8601();
    j["type"] = "fault";
    j["sensor"] = sensor_id;
    j["fault_type"] = fault_type;
    j["injected_value"] = injected_value;
    write_line(j.dump());
}

void Logger::log_rule_violation(const RuleResult& result) {
    nlohmann::json j;
    j["ts"] = timestamp_iso8601();
    j["type"] = "rule_violation";
    j["rule"] = result.rule_name;
    j["sensor"] = result.sensor_id;
    j["message"] = result.message;
    write_line(j.dump());
}

void Logger::write_line(const std::string& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_ << json << "\n";
        file_.flush();
    }
    std::cout << json << std::endl;
}

std::string Logger::timestamp_iso8601() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time_t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace kestrel
