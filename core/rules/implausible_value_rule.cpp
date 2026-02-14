#include "rules/implausible_value_rule.h"

namespace kestrel {

ImplausibleValueRule::ImplausibleValueRule(double absolute_min, double absolute_max)
    : absolute_min_(absolute_min), absolute_max_(absolute_max) {}

RuleResult ImplausibleValueRule::evaluate(const MeasurementWindow& window,
                                          const std::string& sensor_id) {
    auto latest = window.latest(sensor_id);

    RuleResult result;
    result.rule_name = name();
    result.sensor_id = sensor_id;

    if (!latest.valid) {
        result.severity = RuleSeverity::OK; // other rules handle missing data
        return result;
    }

    if (latest.value < absolute_min_ || latest.value > absolute_max_) {
        result.severity = RuleSeverity::FAILED;
        result.message = "implausible value " + std::to_string(latest.value) +
                         " outside absolute bounds [" +
                         std::to_string(absolute_min_) + ", " +
                         std::to_string(absolute_max_) + "]";
        return result;
    }

    result.severity = RuleSeverity::OK;
    return result;
}

} // namespace kestrel
