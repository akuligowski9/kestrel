#include "fault/fault_profile.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace kestrel {

static FaultType parse_fault_type(const std::string& s) {
    if (s == "Spike")             return FaultType::SPIKE;
    if (s == "InvalidValue")      return FaultType::INVALID_VALUE;
    if (s == "MissingUpdate")     return FaultType::MISSING_UPDATE;
    if (s == "DelayedReading")    return FaultType::DELAYED_READING;
    if (s == "InterfaceFailure")  return FaultType::INTERFACE_FAILURE;
    throw std::runtime_error("unknown fault type: " + s);
}

std::vector<FaultConfig> FaultProfile::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open fault profile: " + path);
    }

    auto json = nlohmann::json::parse(file);
    std::vector<FaultConfig> configs;

    for (const auto& entry : json.at("faults")) {
        FaultConfig fc;
        fc.sensor_id = entry.at("sensor_id").get<std::string>();
        fc.type = parse_fault_type(entry.at("type").get<std::string>());
        fc.trigger_after_s = entry.value("trigger_after_s", 0.0);
        fc.duration_s = entry.value("duration_s", 0.0);

        fc.params.injected_value = entry.value("value", 0.0);
        fc.params.suppress_cycles = entry.value("suppress_cycles", 0);
        fc.params.delay_ms = entry.value("delay_ms", 0);

        configs.push_back(fc);
    }

    return configs;
}

} // namespace kestrel
