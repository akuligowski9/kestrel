#include "rules/threshold_rule.h"

namespace kestrel {

ThresholdRule::ThresholdRule(double min, double max, RuleSeverity breach_severity,
                             const std::string& target_sensor)
    : min_(min), max_(max), breach_severity_(breach_severity),
      target_sensor_(target_sensor) {
    if (target_sensor_.empty()) {
        use_global_ = true;
    } else {
        bounds_[target_sensor_] = {min_, max_, breach_severity_};
    }
}

ThresholdRule::ThresholdRule(std::unordered_map<std::string, ThresholdBounds> bounds)
    : bounds_(std::move(bounds)) {}

RuleResult ThresholdRule::evaluate(const MeasurementWindow& window,
                                   const std::string& sensor_id) {
    RuleResult result;
    result.rule_name = name();
    result.sensor_id = sensor_id;

    if (use_global_) {
        // Global mode: apply min_/max_ to all sensors
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

    // Map mode: look up sensor in bounds map
    auto it = bounds_.find(sensor_id);
    if (it == bounds_.end()) {
        result.severity = RuleSeverity::OK;
        return result;
    }

    const auto& b = it->second;
    auto latest = window.latest(sensor_id);

    if (!latest.valid) {
        result.severity = RuleSeverity::FAILED;
        result.message = "no valid reading";
        return result;
    }

    if (latest.value < b.min || latest.value > b.max) {
        result.severity = b.breach_severity;
        result.message = "value " + std::to_string(latest.value) +
                         " outside bounds [" + std::to_string(b.min) +
                         ", " + std::to_string(b.max) + "]";
        return result;
    }

    result.severity = RuleSeverity::OK;
    return result;
}

} // namespace kestrel
