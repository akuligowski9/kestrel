#pragma once

#include "rules/rule.h"

#include <string>
#include <unordered_map>

namespace kestrel {

struct ThresholdBounds {
    double min;
    double max;
    RuleSeverity breach_severity;
};

class ThresholdRule : public IRule {
public:
    ThresholdRule(double min, double max,
                  RuleSeverity breach_severity = RuleSeverity::DEGRADED,
                  const std::string& target_sensor = "");

    explicit ThresholdRule(std::unordered_map<std::string, ThresholdBounds> bounds);

    RuleResult evaluate(const MeasurementWindow& window,
                        const std::string& sensor_id) override;
    std::string name() const override { return "ThresholdRule"; }

private:
    double min_ = 0.0;
    double max_ = 0.0;
    RuleSeverity breach_severity_ = RuleSeverity::DEGRADED;
    std::string target_sensor_;
    std::unordered_map<std::string, ThresholdBounds> bounds_;
    bool use_global_ = false;
};

} // namespace kestrel
