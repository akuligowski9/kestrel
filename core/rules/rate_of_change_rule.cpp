#include "rules/rate_of_change_rule.h"

#include <cmath>

namespace kestrel {

RateOfChangeRule::RateOfChangeRule(double max_rate_per_second)
    : max_rate_per_second_(max_rate_per_second) {}

RuleResult RateOfChangeRule::evaluate(const MeasurementWindow& window,
                                      const std::string& sensor_id) {
    auto readings = window.readings_for(sensor_id);

    RuleResult result;
    result.rule_name = name();
    result.sensor_id = sensor_id;

    if (readings.size() < 2) {
        result.severity = RuleSeverity::OK;
        return result;
    }

    const auto& prev = readings[readings.size() - 2];
    const auto& curr = readings[readings.size() - 1];

    if (!prev.valid || !curr.valid) {
        result.severity = RuleSeverity::OK;
        return result;
    }

    auto dt = std::chrono::duration<double>(curr.timestamp - prev.timestamp);
    if (dt.count() <= 0.0) {
        result.severity = RuleSeverity::OK;
        return result;
    }

    double rate = std::abs(curr.value - prev.value) / dt.count();

    if (rate > max_rate_per_second_) {
        result.severity = RuleSeverity::DEGRADED;
        result.message = "rate of change " + std::to_string(rate) +
                         "/s exceeds limit " +
                         std::to_string(max_rate_per_second_) + "/s";
        return result;
    }

    result.severity = RuleSeverity::OK;
    return result;
}

} // namespace kestrel
