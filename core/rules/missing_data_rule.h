#pragma once

#include "rules/rule.h"

#include <chrono>

namespace kestrel {

// Flags degraded/failed state when no reading has arrived within the expected interval.
class MissingDataRule : public IRule {
public:
    explicit MissingDataRule(std::chrono::milliseconds max_age,
                             std::chrono::milliseconds fail_age);

    RuleResult evaluate(const MeasurementWindow& window,
                        const std::string& sensor_id) override;
    std::string name() const override { return "MissingDataRule"; }

private:
    std::chrono::milliseconds max_age_;
    std::chrono::milliseconds fail_age_;
};

} // namespace kestrel
