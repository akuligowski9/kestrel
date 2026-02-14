#pragma once

#include "rules/rule.h"

namespace kestrel {

// Flags values outside physically possible range (harder bounds than ThresholdRule).
class ImplausibleValueRule : public IRule {
public:
    ImplausibleValueRule(double absolute_min, double absolute_max);

    RuleResult evaluate(const MeasurementWindow& window,
                        const std::string& sensor_id) override;
    std::string name() const override { return "ImplausibleValueRule"; }

private:
    double absolute_min_;
    double absolute_max_;
};

} // namespace kestrel
