#include "rules/threshold_rule.h"

namespace kestrel {

ThresholdRule::ThresholdRule(double min, double max, RuleSeverity breach_severity,
                             const std::string& target_sensor)
    : min_(min), max_(max), breach_severity_(breach_severity),
      target_sensor_(target_sensor) {}

RuleResult ThresholdRule::evaluate(const MeasurementWindow& window,
                                   const std::string& sensor_id) {
    RuleResult result;
    result.rule_name = name();
    result.sensor_id = sensor_id;

    // Skip sensors this rule doesn't target
    if (!target_sensor_.empty() && target_sensor_ != sensor_id) {
        result.severity = RuleSeverity::OK;
        return result;
    }

    auto latest = window.latest(sensor_id);

    if (!latest.valid) {
        result.severity = RuleSeverity::FAILED;
        result.message = "no valid reading";
        return result;
    }

    if (latest.value < min_ || latest.value > max_) {
        result.severity = breach_severity_;
        result.message = "value " + std::to_string(latest.value) +
                         " outside bounds [" + std::to_string(min_) +
                         ", " + std::to_string(max_) + "]";
        return result;
    }

    result.severity = RuleSeverity::OK;
    return result;
}

} // namespace kestrel
