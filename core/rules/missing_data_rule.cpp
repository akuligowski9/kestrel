#include "rules/missing_data_rule.h"

namespace kestrel {

MissingDataRule::MissingDataRule(std::chrono::milliseconds max_age,
                                 std::chrono::milliseconds fail_age)
    : max_age_(max_age), fail_age_(fail_age) {}

RuleResult MissingDataRule::evaluate(const MeasurementWindow& window,
                                     const std::string& sensor_id) {
    auto latest = window.latest(sensor_id);

    RuleResult result;
    result.rule_name = name();
    result.sensor_id = sensor_id;

    if (!latest.valid) {
        result.severity = RuleSeverity::FAILED;
        result.message = "no valid reading available";
        return result;
    }

    auto age = std::chrono::steady_clock::now() - latest.timestamp;

    if (age > fail_age_) {
        result.severity = RuleSeverity::FAILED;
        result.message = "reading age exceeds failure threshold";
        return result;
    }

    if (age > max_age_) {
        result.severity = RuleSeverity::DEGRADED;
        result.message = "reading age exceeds expected interval";
        return result;
    }

    result.severity = RuleSeverity::OK;
    return result;
}

} // namespace kestrel
