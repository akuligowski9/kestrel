#pragma once

#include "rules/rule.h"

namespace kestrel {

class ThresholdRule : public IRule {
public:
    ThresholdRule(double min, double max,
                  RuleSeverity breach_severity = RuleSeverity::DEGRADED);

    RuleResult evaluate(const MeasurementWindow& window,
                        const std::string& sensor_id) override;
    std::string name() const override { return "ThresholdRule"; }

private:
    double min_;
    double max_;
    RuleSeverity breach_severity_;
};

} // namespace kestrel
