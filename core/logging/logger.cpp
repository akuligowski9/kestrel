#include "logging/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
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
    std::string line = R"({"ts":")" + timestamp_iso8601() +
                       R"(","type":"reading","sensor":")" + reading.sensor_id +
                       R"(","value":)" + std::to_string(reading.value) +
                       R"(,"valid":)" + (reading.valid ? "true" : "false") + "}";
    write_line(line);
}

void Logger::log_transition(const StateTransition& transition) {
    std::string line = R"({"ts":")" + timestamp_iso8601() +
                       R"(","type":"transition","sensor":")" + transition.sensor_id +
                       R"(","from":")" + to_string(transition.from) +
                       R"(","to":")" + to_string(transition.to) +
                       R"(","reason":")" + transition.reason + R"("})";
    write_line(line);
}

void Logger::log_fault(const std::string& sensor_id,
                       const std::string& fault_type,
                       double injected_value) {
    std::string line = R"({"ts":")" + timestamp_iso8601() +
                       R"(","type":"fault","sensor":")" + sensor_id +
                       R"(","fault_type":")" + fault_type +
                       R"(","injected_value":)" +
                       std::to_string(injected_value) + "}";
    write_line(line);
}

void Logger::log_rule_violation(const RuleResult& result) {
    std::string line = R"({"ts":")" + timestamp_iso8601() +
                       R"(","type":"rule_violation","rule":")" + result.rule_name +
                       R"(","sensor":")" + result.sensor_id +
                       R"(","message":")" + result.message + R"("})";
    write_line(line);
}

void Logger::write_line(const std::string& json) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_ << json << "\n";
        file_.flush();
    }
    std::cout << json << "\n";
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
