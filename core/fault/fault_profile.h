#pragma once

#include "fault/fault_injector.h"

#include <string>
#include <vector>

namespace kestrel {

struct FaultConfig {
    std::string sensor_id;
    FaultType type;
    FaultParameters params;
    double trigger_after_s = 0.0;
    double duration_s = 0.0;    // 0 = no auto-clear

    // Runtime state
    bool triggered = false;
    bool cleared = false;
    double injected_at_s = 0.0;
};

class FaultProfile {
public:
    static std::vector<FaultConfig> load(const std::string& path);
};

} // namespace kestrel
