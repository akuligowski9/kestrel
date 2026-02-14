#pragma once

#include "rules/rule.h"

namespace kestrel {

// Flags when a sensor value changes faster than an expected rate per second.
class RateOfChangeRule : public IRule {
public:
    explicit RateOfChangeRule(double max_rate_per_second);

    RuleResult evaluate(const MeasurementWindow& window,
                        const std::string& sensor_id) override;
    std::string name() const override { return "RateOfChangeRule"; }

private:
    double max_rate_per_second_;
};

} // namespace kestrel
