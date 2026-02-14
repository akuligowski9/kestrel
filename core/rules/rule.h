#pragma once

#include "engine/measurement_window.h"

#include <string>

namespace kestrel {

enum class RuleSeverity {
    OK,
    DEGRADED,
    FAILED
};

struct RuleResult {
    std::string rule_name;
    std::string sensor_id;
    RuleSeverity severity = RuleSeverity::OK;
    std::string message;
};

class IRule {
public:
    virtual ~IRule() = default;
    virtual RuleResult evaluate(const MeasurementWindow& window,
                                const std::string& sensor_id) = 0;
    virtual std::string name() const = 0;
};

} // namespace kestrel
